/// \file surface_rewrite.cc
/// \brief The rewrite chain, and its first pass: constant elision. An
/// algorithm on the datum structure — the domain inference (`spec.hh`)
/// names the constants, the walk folds reads, drops writes, and removes
/// the leaves from declarations, shape and init.

#include "hsc/surface/rewrite.hh"

#include <map>
#include <optional>
#include <set>
#include <sstream>

#include "hsc/surface/spec.hh"

namespace hsc::surface {

namespace {

/// The state of one elision walk: the constants, and the tally.
struct eliding {
  std::map<std::string, std::int32_t> consts;
  std::map<std::string, std::size_t> reads, writes;
  std::vector<std::string> notes;

  [[nodiscard]] std::optional<std::int32_t> value(const datum& d) {
    if (!d.is_atom()) return std::nullopt;
    const auto it = consts.find(d.text());
    if (it == consts.end()) return std::nullopt;
    return it->second;
  }

  /// Substitute constants in an expression form. `at`/`at@` protect their
  /// array-name argument; every other atom position is a read.
  datum expr(const datum& d) {
    if (d.is_atom()) {
      if (const auto v = value(d)) {
        ++reads[d.text()];
        return datum::atom(std::to_string(*v), d.line());
      }
      return d;
    }
    std::vector<datum> kids;
    kids.reserve(d.items().size());
    const bool at = d.is_list() && !d.items().empty() &&
                    (d.head() == "at" || d.head() == "at@");
    for (std::size_t i = 0; i < d.items().size(); ++i) {
      kids.push_back(at && i < 2 ? d.items()[i] : expr(d.items()[i]));
    }
    return datum::list(std::move(kids), d.line());
  }

  /// One action of a `do` clause; nullopt drops it (a write of a constant).
  std::optional<datum> action(const datum& a) {
    if (!a.is_list() || a.items().size() < 2) return a;
    const datum& lhs = a.items()[1];
    if (const auto v = value(lhs)) {
      (void)v;
      ++writes[lhs.text()];
      return std::nullopt;
    }
    std::vector<datum> kids;
    kids.push_back(a.items()[0]);
    if (lhs.is_list()) {  // (at NAME IDX): the index reads, the name not
      kids.push_back(expr(lhs));
    } else {
      kids.push_back(lhs);
    }
    for (std::size_t i = 2; i < a.items().size(); ++i) {
      // havoc bounds are integers; rhs expressions substitute
      kids.push_back(a.head() == "havoc" ? a.items()[i] : expr(a.items()[i]));
    }
    return datum::list(std::move(kids), a.line());
  }

  /// A `(when …)` or `(do …)` clause of an event/family body.
  datum clause(const datum& c) {
    if (!c.is_list() || c.items().empty()) return c;
    std::vector<datum> kids;
    kids.push_back(c.items()[0]);
    if (c.head() == "when") {
      for (std::size_t i = 1; i < c.items().size(); ++i) {
        kids.push_back(expr(c.items()[i]));
      }
    } else if (c.head() == "do") {
      for (std::size_t i = 1; i < c.items().size(); ++i) {
        if (auto a = action(c.items()[i])) kids.push_back(std::move(*a));
      }
    } else {
      return c;  // family markers etc.: untouched
    }
    return datum::list(std::move(kids), c.line());
  }

  /// An inline event term: NAME | (when …) | (do …) | (alt …) | (seq …)
  /// | (abort).
  datum evterm(const datum& d) {
    if (d.is_atom()) return d;  // a named term
    if (d.items().empty()) return d;
    const std::string& h = d.head();
    if (h == "when" || h == "do") return clause(d);
    if (h == "alt" || h == "seq") {
      std::vector<datum> kids;
      kids.push_back(d.items()[0]);
      for (std::size_t i = 1; i < d.items().size(); ++i) {
        kids.push_back(evterm(d.items()[i]));
      }
      return datum::list(std::move(kids), d.line());
    }
    return d;  // (abort), anything else
  }

  /// Remove elided leaves from a SORT; nullopt when nothing remains.
  std::optional<datum> sort(const datum& s) {
    if (s.is_atom()) {
      if (s.text() != "unit" && consts.contains(s.text())) return std::nullopt;
      return s;
    }
    if (s.items().empty()) return s;
    std::vector<datum> kids;
    kids.push_back(s.items()[0]);
    for (std::size_t i = 1; i < s.items().size(); ++i) {
      if (auto k = sort(s.items()[i])) kids.push_back(std::move(*k));
    }
    if (kids.size() == 1) return std::nullopt;  // every child elided
    if (s.head() == "pair" && kids.size() == 2) {
      return kids[1];  // one side left: the pair collapses to it
    }
    return datum::list(std::move(kids), s.line());
  }
};

}  // namespace

rewrite_result elide_constants(std::vector<datum> forms) {
  // the constants: scalar units whose inferred domain is one value, not
  // colliding with a name of the event algebra
  std::set<std::string> term_names;
  std::set<std::string> leaf_names;
  for (const datum& f : forms) {
    if (!f.is_list() || f.items().size() < 2) continue;
    const std::string& h = f.head();
    if (h == "event" || h == "family" || h == "alt" || h == "seq") {
      term_names.insert(f.items()[1].text());
    } else if (h == "leaf") {
      leaf_names.insert(f.items()[1].text());
    }
  }
  eliding e;
  for (const unit_domain& u : analyze_domains(forms)) {
    if (u.is_array || u.report.k != xpl::domain_report::kind::set ||
        u.report.values.size() != 1 || term_names.contains(u.name)) {
      continue;
    }
    e.consts[u.name] = u.report.values.front();
  }
  if (e.consts.empty()) {
    return {std::move(forms), false, "no constants found"};
  }

  std::vector<datum> out;
  out.reserve(forms.size());
  for (datum& f : forms) {
    if (!f.is_list() || f.items().empty()) {
      out.push_back(std::move(f));
      continue;
    }
    const std::string& h = f.head();
    if (h == "leaf" && f.items().size() > 1 &&
        e.consts.contains(f.items()[1].text())) {
      continue;  // the declaration goes
    }
    if (h == "shape" && f.items().size() > 1) {
      auto s = e.sort(f.items()[1]);
      if (!s) {  // every leaf constant: nothing left to run — bail whole
        return {std::move(out), false,
                "every leaf is constant; not applied"};
      }
      out.push_back(datum::list({f.items()[0], std::move(*s)}, f.line()));
      continue;
    }
    if (h == "init" || h == "word") {
      // pair entries binding an elided leaf drop; an init event body
      // substitutes like any term
      std::vector<datum> kids;
      kids.push_back(f.items()[0]);
      if (h == "word") kids.push_back(f.items()[1]);  // the result name
      for (std::size_t i = kids.size(); i < f.items().size(); ++i) {
        const datum& it = f.items()[i];
        if (it.is_list() && it.items().size() == 2 && it.items()[0].is_atom() &&
            leaf_names.contains(it.items()[0].text())) {
          if (const auto v = e.value(it.items()[0])) {
            if (h == "word" || it.items()[1].text() != std::to_string(*v)) {
              e.notes.push_back(h + " drops (" + it.items()[0].text() + " " +
                                it.items()[1].text() + ")");
            }
            continue;
          }
          kids.push_back(it);
        } else {
          kids.push_back(e.evterm(it));
        }
      }
      out.push_back(datum::list(std::move(kids), f.line()));
      continue;
    }
    if (h == "event" || h == "family") {
      const std::size_t body = h == "event" ? 2 : 3;
      std::vector<datum> kids(f.items().begin(),
                              f.items().begin() +
                                  static_cast<std::ptrdiff_t>(body));
      for (std::size_t i = body; i < f.items().size(); ++i) {
        kids.push_back(e.clause(f.items()[i]));
      }
      out.push_back(datum::list(std::move(kids), f.line()));
      continue;
    }
    if (h == "alt" || h == "seq") {  // the named-combinator declaration
      std::vector<datum> kids(f.items().begin(), f.items().begin() + 2);
      for (std::size_t i = 2; i < f.items().size(); ++i) {
        kids.push_back(e.evterm(f.items()[i]));
      }
      out.push_back(datum::list(std::move(kids), f.line()));
      continue;
    }
    if (h == "select") {  // atoms are boolean forms over the leaves
      std::vector<datum> kids(f.items().begin(), f.items().begin() + 3);
      for (std::size_t i = 3; i < f.items().size(); ++i) {
        kids.push_back(e.expr(f.items()[i]));
      }
      out.push_back(datum::list(std::move(kids), f.line()));
      continue;
    }
    if (h == "reach" || h == "apply") {  // may carry an inline event term
      std::vector<datum> kids;
      kids.push_back(f.items()[0]);
      for (std::size_t i = 1; i < f.items().size(); ++i) {
        kids.push_back(f.items()[i].is_list() ? e.evterm(f.items()[i])
                                              : f.items()[i]);
      }
      out.push_back(datum::list(std::move(kids), f.line()));
      continue;
    }
    out.push_back(std::move(f));
  }

  std::ostringstream trace;
  trace << e.consts.size() << " constant" << (e.consts.size() > 1 ? "s" : "")
        << " elided";
  for (const auto& [name, v] : e.consts) {
    trace << "\n  " << name << " = " << v << ": " << e.reads[name]
          << " reads folded, " << e.writes[name] << " writes dropped";
  }
  for (const std::string& n : e.notes) trace << "\n  note: " << n;
  return {std::move(out), true, trace.str()};
}

std::pair<std::vector<datum>, std::vector<trace_entry>> rewrite(
    std::vector<datum> forms, std::span<const pass> passes) {
  std::vector<trace_entry> log;
  for (const pass& p : passes) {
    rewrite_result r = p.apply(std::move(forms));
    log.push_back({p.name, r.applied, std::move(r.trace)});
    forms = std::move(r.forms);
  }
  return {std::move(forms), std::move(log)};
}

std::vector<pass> default_chain() {
  return {{"simplify-constants", elide_constants}};
}

std::vector<pass_def> pass_registry() {
  return {{"simplify-constants",
           [](std::vector<datum> f, const datum&) {
             return elide_constants(std::move(f));
           }},
          {"hotbit", hotbit},
          {"reorder-force", reorder_force},
          {"flatten", flatten},
          {"simplify-arrays", simplify_arrays}};
}

}  // namespace hsc::surface
