/// \file surface_hotbit.cc
/// \brief The one-hot pass: an eligible enumerated leaf trades its
/// integer for K bits, one per value, exactly one set. The GAL
/// HotBitRewriter re-expressed on the datum structure —
/// `algorithm.md` §1c names the eligibility rules and the encoding.

#include <map>
#include <optional>
#include <set>
#include <sstream>

#include "hsc/surface/rewrite.hh"
#include "hsc/surface/spec.hh"

namespace hsc::surface {

namespace {

struct hb_var {
  std::int32_t k = 0;           ///< domain is exactly {0 … k-1}
  std::size_t reads = 0;        ///< comparison atoms rewritten
  std::size_t pinned = 0;       ///< writes cleared via a known bit
  std::size_t full_clears = 0;  ///< writes cleared bit-by-bit
};

bool is_int(const datum& d, std::int32_t& v) {
  if (!d.is_atom()) return false;
  try {
    std::size_t used = 0;
    v = std::stoi(d.text(), &used);
    return used == d.text().size();
  } catch (...) {
    return false;
  }
}

class hotbitter {
 public:
  std::map<std::string, hb_var> vars;
  std::map<std::string, std::string> refused;  ///< candidate → reason

  [[nodiscard]] bool candidate(const std::string& n) const {
    return vars.contains(n);
  }
  void refuse(const std::string& n, const std::string& why) {
    if (vars.erase(n) > 0) refused.emplace(n, why);
  }

  static std::string bit(const std::string& n, std::int32_t v) {
    return n + "_" + std::to_string(v);
  }

  // --- eligibility scan -----------------------------------------------------

  /// A comparison `(== x c)` / `(!= x c)` (either side), or `(in x c…)`.
  /// Returns the variable when it is a candidate occurrence.
  std::optional<std::string> const_test(const datum& d) const {
    if (!d.is_list() || d.items().empty()) return std::nullopt;
    const std::string& h = d.head();
    std::int32_t v = 0;
    if ((h == "==" || h == "!=") && d.items().size() == 3) {
      const datum& a = d.items()[1];
      const datum& b = d.items()[2];
      if (a.is_atom() && candidate(a.text()) && is_int(b, v)) return a.text();
      if (b.is_atom() && candidate(b.text()) && is_int(a, v)) return b.text();
    }
    if (h == "in" && d.items().size() >= 3 && d.items()[1].is_atom() &&
        candidate(d.items()[1].text())) {
      return d.items()[1].text();
    }
    return std::nullopt;
  }

  /// Any appearance of a candidate outside a recognized occurrence
  /// disqualifies it.
  void scan_expr(const datum& d) {
    if (d.is_atom()) {
      if (candidate(d.text())) refuse(d.text(), "read as a value");
      return;
    }
    if (const auto x = const_test(d)) {
      // the recognized shape: scan only the non-candidate side
      for (std::size_t i = 1; i < d.items().size(); ++i) {
        if (!(d.items()[i].is_atom() && d.items()[i].text() == *x)) {
          scan_expr(d.items()[i]);
        }
      }
      return;
    }
    const bool at = !d.items().empty() &&
                    (d.head() == "at" || d.head() == "at@");
    for (std::size_t i = at ? 2 : 1; i < d.items().size(); ++i) {
      scan_expr(d.items()[i]);
    }
  }

  void scan_action(const datum& a) {
    if (!a.is_list() || a.items().size() < 2) return;
    const datum& lhs = a.items()[1];
    std::int32_t v = 0;
    if (lhs.is_atom() && candidate(lhs.text())) {
      if (a.head() == ":=" && a.items().size() == 3 &&
          is_int(a.items()[2], v)) {
        return;  // a constant write: fine; the rhs is that constant
      }
      refuse(lhs.text(), "written beyond (:= x CONST)");
      // and fall through: the rhs may read other candidates
    }
    if (lhs.is_list()) scan_expr(lhs);
    for (std::size_t i = 2; i < a.items().size(); ++i) scan_expr(a.items()[i]);
  }

  void scan_clause(const datum& c) {
    if (!c.is_list() || c.items().empty()) return;
    if (c.head() == "when") {
      for (std::size_t i = 1; i < c.items().size(); ++i) {
        scan_expr(c.items()[i]);
      }
    } else if (c.head() == "do") {
      for (std::size_t i = 1; i < c.items().size(); ++i) {
        scan_action(c.items()[i]);
      }
    }
  }

  void scan_evterm(const datum& d) {
    if (!d.is_list() || d.items().empty()) return;
    const std::string& h = d.head();
    if (h == "when" || h == "do") return scan_clause(d);
    if (h == "alt" || h == "seq") {
      for (std::size_t i = 1; i < d.items().size(); ++i) {
        scan_evterm(d.items()[i]);
      }
    }
  }

  // --- the rewrite ----------------------------------------------------------

  /// `(== x c)` → `(== x_c 1)`; `(!= x c)` → `(== x_c 0)`;
  /// `(in x c…)` → the disjunction of bit tests. Out-of-domain constants
  /// fold to the empty `(or)` / `(and)`.
  datum expr(const datum& d) {
    if (d.is_atom()) return d;
    if (const auto xo = const_test(d)) {
      const std::string& x = *xo;
      hb_var& hv = vars.at(x);
      ++hv.reads;
      const std::string& h = d.head();
      const int line = d.line();
      auto test = [&](std::int32_t c, bool eq) {
        return datum::list({datum::atom("==", line),
                            datum::atom(bit(x, c), line),
                            datum::atom(eq ? "1" : "0", line)},
                           line);
      };
      if (h == "in") {
        std::vector<datum> alts;
        alts.push_back(datum::atom("or", line));
        for (std::size_t i = 2; i < d.items().size(); ++i) {
          std::int32_t c = 0;
          if (is_int(d.items()[i], c) && c >= 0 && c < hv.k) {
            alts.push_back(test(c, true));
          }
        }
        return datum::list(std::move(alts), line);
      }
      std::int32_t c = 0;
      (void)(is_int(d.items()[1], c) || is_int(d.items()[2], c));
      const bool eq = h == "==";
      if (c < 0 || c >= hv.k) {  // never that value: constant truth
        return datum::list({datum::atom(eq ? "or" : "and", line)}, line);
      }
      return test(c, eq);
    }
    std::vector<datum> kids;
    kids.reserve(d.items().size());
    const bool at = d.head() == "at" || d.head() == "at@";
    for (std::size_t i = 0; i < d.items().size(); ++i) {
      kids.push_back(i == 0 || (at && i == 1) ? d.items()[i]
                                              : expr(d.items()[i]));
    }
    return datum::list(std::move(kids), d.line());
  }

  /// Direct `(== x c)` conjuncts of a `when` form pin the pre-state bit.
  void collect_pins(const datum& clause,
                    std::map<std::string, std::int32_t>& pin) const {
    for (std::size_t i = 1; i < clause.items().size(); ++i) {
      const datum& d = clause.items()[i];
      if (!d.is_list() || d.items().empty()) continue;
      if (d.head() == "and") {  // conjuncts of a conjunct still pin
        collect_pins(d, pin);
        continue;
      }
      std::int32_t v = 0;
      if (d.head() == "==" && d.items().size() == 3) {
        const datum& a = d.items()[1];
        const datum& b = d.items()[2];
        if (a.is_atom() && candidate(a.text()) && is_int(b, v)) {
          pin[a.text()] = v;
        } else if (b.is_atom() && candidate(b.text()) && is_int(a, v)) {
          pin[b.text()] = v;
        }
      }
    }
  }

  /// One event/family body: whens rewrite (and pin), constant writes of a
  /// candidate move the hot bit.
  std::vector<datum> body(std::span<const datum> clauses) {
    std::map<std::string, std::int32_t> pin;
    for (const datum& c : clauses) {
      if (c.is_list() && !c.items().empty() && c.head() == "when") {
        collect_pins(c, pin);
      }
    }
    std::vector<datum> out;
    for (const datum& c : clauses) {
      if (!c.is_list() || c.items().empty()) {
        out.push_back(c);
        continue;
      }
      if (c.head() == "when") {
        std::vector<datum> kids{c.items()[0]};
        for (std::size_t i = 1; i < c.items().size(); ++i) {
          kids.push_back(expr(c.items()[i]));
        }
        out.push_back(datum::list(std::move(kids), c.line()));
        continue;
      }
      if (c.head() != "do") {
        out.push_back(c);
        continue;
      }
      std::vector<datum> kids{c.items()[0]};
      const int line = c.line();
      auto set = [&](const std::string& x, std::int32_t v, const char* to) {
        kids.push_back(datum::list({datum::atom(":=", line),
                                    datum::atom(bit(x, v), line),
                                    datum::atom(to, line)},
                                   line));
      };
      for (std::size_t i = 1; i < c.items().size(); ++i) {
        const datum& a = c.items()[i];
        std::int32_t v = 0;
        if (a.is_list() && a.head() == ":=" && a.items().size() == 3 &&
            a.items()[1].is_atom() && candidate(a.items()[1].text()) &&
            is_int(a.items()[2], v)) {
          const std::string& x = a.items()[1].text();
          hb_var& hv = vars.at(x);
          const auto p = pin.find(x);
          if (p != pin.end()) {  // the old bit is known
            ++hv.pinned;
            if (p->second != v) {
              set(x, p->second, "0");
              set(x, v, "1");
            }  // writing the pinned value again: the bit already holds
          } else {  // clear everything, set one — one simultaneous clause
            ++hv.full_clears;
            for (std::int32_t b = 0; b < hv.k; ++b) {
              if (b != v) set(x, b, "0");
            }
            set(x, v, "1");
          }
          pin[x] = v;  // later clauses see this write as the pin
          continue;
        }
        // not a candidate write: substitute inside its expressions
        if (a.is_list() && a.items().size() >= 2) {
          std::vector<datum> ak{a.items()[0]};
          ak.push_back(a.items()[1].is_list() ? expr(a.items()[1])
                                              : a.items()[1]);
          for (std::size_t j = 2; j < a.items().size(); ++j) {
            ak.push_back(a.head() == "havoc" ? a.items()[j]
                                             : expr(a.items()[j]));
          }
          kids.push_back(datum::list(std::move(ak), a.line()));
        } else {
          kids.push_back(a);
        }
      }
      out.push_back(datum::list(std::move(kids), line));
    }
    return out;
  }

  datum evterm(const datum& d) {
    if (!d.is_list() || d.items().empty()) return d;
    const std::string& h = d.head();
    if (h == "when" || h == "do") {
      return body({&d, 1}).front();  // a lone clause: no cross-clause pins
    }
    if (h == "alt" || h == "seq") {
      std::vector<datum> kids{d.items()[0]};
      for (std::size_t i = 1; i < d.items().size(); ++i) {
        kids.push_back(evterm(d.items()[i]));
      }
      return datum::list(std::move(kids), d.line());
    }
    return d;
  }

  /// The shape: a candidate's atom becomes the balanced block of its bits.
  datum sort(const datum& s) {
    if (s.is_atom()) {
      if (!candidate(s.text())) return s;
      const hb_var& hv = vars.at(s.text());
      std::vector<datum> kids{datum::atom("balanced", s.line())};
      for (std::int32_t b = 0; b < hv.k; ++b) {
        kids.push_back(datum::atom(bit(s.text(), b), s.line()));
      }
      return datum::list(std::move(kids), s.line());
    }
    if (s.items().empty()) return s;
    std::vector<datum> kids{s.items()[0]};
    for (std::size_t i = 1; i < s.items().size(); ++i) {
      kids.push_back(sort(s.items()[i]));
    }
    return datum::list(std::move(kids), s.line());
  }
};

}  // namespace

rewrite_result hotbit(std::vector<datum> forms, const datum& directive) {
  // the window: below MIN a variable is nearly a boolean already, above
  // MAX the bit vector outgrows the win
  std::int32_t lo = 3;
  std::int32_t hi = 16;
  if (directive.is_list() && directive.items().size() > 1) {
    (void)is_int(directive.items()[1], lo);
  }
  if (directive.is_list() && directive.items().size() > 2) {
    (void)is_int(directive.items()[2], hi);
  }

  // candidates: scalar, domain exactly {0 … K-1}, MIN ≤ K ≤ MAX
  hotbitter hb;
  std::set<std::string> names;  // every declared name, for collisions
  bool init_event = false;
  for (const datum& f : forms) {
    if (!f.is_list() || f.items().empty()) continue;
    if (f.items().size() > 1 && f.items()[1].is_atom()) {
      names.insert(f.items()[1].text());
    }
    if (f.head() == "init" && f.items().size() == 2 &&
        f.items()[1].is_list() && !f.items()[1].items().empty() &&
        !f.items()[1].items()[0].is_list()) {
      const std::string& h = f.items()[1].head();
      if (h == "when" || h == "do" || h == "alt" || h == "seq") {
        init_event = true;
      }
    }
  }
  for (const unit_domain& u : analyze_domains(forms)) {
    if (u.is_array || u.report.k != xpl::domain_report::kind::set) continue;
    const auto& vs = u.report.values;
    const auto k = static_cast<std::int32_t>(vs.size());
    if (k < lo || k > hi) continue;
    bool contiguous = true;
    for (std::int32_t i = 0; i < k; ++i) contiguous &= vs[i] == i;
    if (!contiguous) continue;
    bool collide = false;
    for (std::int32_t b = 0; b < k; ++b) {
      collide |= names.contains(hotbitter::bit(u.name, b));
    }
    if (collide || init_event) continue;  // silently: not a candidate yet
    hb.vars.emplace(u.name, hb_var{k});
  }

  // eligibility: scan every occurrence; offenders leave the candidate set
  for (const datum& f : forms) {
    if (!f.is_list() || f.items().empty()) continue;
    const std::string& h = f.head();
    if (h == "event") {
      for (std::size_t i = 2; i < f.items().size(); ++i) {
        hb.scan_clause(f.items()[i]);
      }
    } else if (h == "family") {
      for (std::size_t i = 3; i < f.items().size(); ++i) {
        hb.scan_clause(f.items()[i]);
      }
    } else if (h == "alt" || h == "seq") {
      for (std::size_t i = 2; i < f.items().size(); ++i) {
        hb.scan_evterm(f.items()[i]);
      }
    } else if (h == "select") {
      for (std::size_t i = 3; i < f.items().size(); ++i) {
        hb.scan_expr(f.items()[i]);
      }
    } else if (h == "reach" || h == "apply") {
      for (std::size_t i = 1; i < f.items().size(); ++i) {
        if (f.items()[i].is_list()) hb.scan_evterm(f.items()[i]);
      }
    } else if (h == "max-value" && f.items().size() > 1 &&
               f.items()[1].is_atom()) {
      hb.refuse(f.items()[1].text(), "max-value reads it as a value");
    }
  }
  if (hb.vars.empty()) {
    std::string t = "no eligible variable";
    for (const auto& [n, why] : hb.refused) {
      t += "\n  " + n + " refused: " + why;
    }
    return {std::move(forms), false, t};
  }

  // the rewrite
  std::vector<datum> out;
  out.reserve(forms.size());
  for (datum& f : forms) {
    if (!f.is_list() || f.items().empty()) {
      out.push_back(std::move(f));
      continue;
    }
    const std::string& h = f.head();
    const int line = f.line();
    if (h == "leaf" && f.items().size() > 1 &&
        hb.candidate(f.items()[1].text())) {
      const std::string x = f.items()[1].text();
      for (std::int32_t b = 0; b < hb.vars.at(x).k; ++b) {
        out.push_back(datum::list({datum::atom("leaf", line),
                                   datum::atom(hotbitter::bit(x, b), line),
                                   datum::atom("0", line),
                                   datum::atom("2", line)},
                                  line));
      }
      continue;
    }
    if (h == "shape" && f.items().size() > 1) {
      out.push_back(
          datum::list({f.items()[0], hb.sort(f.items()[1])}, line));
      continue;
    }
    if (h == "init" || h == "word") {
      // a pair (x v) becomes (x_v 1); a candidate with no pair sits at
      // its default 0, whose bit must still be set
      std::vector<datum> kids{f.items()[0]};
      std::size_t first = 1;
      if (h == "word") {
        kids.push_back(f.items()[1]);
        first = 2;
      }
      std::set<std::string> seen;
      for (std::size_t i = first; i < f.items().size(); ++i) {
        const datum& it = f.items()[i];
        std::int32_t v = 0;
        if (it.is_list() && it.items().size() == 2 &&
            it.items()[0].is_atom() &&
            hb.candidate(it.items()[0].text()) && is_int(it.items()[1], v)) {
          const std::string& x = it.items()[0].text();
          seen.insert(x);
          kids.push_back(datum::list({datum::atom(hotbitter::bit(x, v), line),
                                      datum::atom("1", line)},
                                     line));
        } else {
          kids.push_back(it);
        }
      }
      if (h == "init") {
        for (const auto& [x, hv] : hb.vars) {
          if (!seen.contains(x)) {
            kids.push_back(
                datum::list({datum::atom(hotbitter::bit(x, 0), line),
                             datum::atom("1", line)},
                            line));
          }
        }
      }
      out.push_back(datum::list(std::move(kids), line));
      continue;
    }
    if (h == "event" || h == "family") {
      const std::size_t nb = h == "event" ? 2 : 3;
      std::vector<datum> kids(f.items().begin(),
                              f.items().begin() +
                                  static_cast<std::ptrdiff_t>(nb));
      for (datum& c : hb.body(std::span(f.items()).subspan(nb))) {
        kids.push_back(std::move(c));
      }
      out.push_back(datum::list(std::move(kids), line));
      continue;
    }
    if (h == "alt" || h == "seq") {
      std::vector<datum> kids(f.items().begin(), f.items().begin() + 2);
      for (std::size_t i = 2; i < f.items().size(); ++i) {
        kids.push_back(hb.evterm(f.items()[i]));
      }
      out.push_back(datum::list(std::move(kids), line));
      continue;
    }
    if (h == "select") {
      std::vector<datum> kids(f.items().begin(), f.items().begin() + 3);
      for (std::size_t i = 3; i < f.items().size(); ++i) {
        kids.push_back(hb.expr(f.items()[i]));
      }
      out.push_back(datum::list(std::move(kids), line));
      continue;
    }
    if (h == "reach" || h == "apply") {
      std::vector<datum> kids{f.items()[0]};
      for (std::size_t i = 1; i < f.items().size(); ++i) {
        kids.push_back(f.items()[i].is_list() ? hb.evterm(f.items()[i])
                                              : f.items()[i]);
      }
      out.push_back(datum::list(std::move(kids), line));
      continue;
    }
    out.push_back(std::move(f));
  }

  std::ostringstream trace;
  trace << hb.vars.size() << " variable" << (hb.vars.size() > 1 ? "s" : "")
        << " one-hot encoded";
  for (const auto& [x, hv] : hb.vars) {
    trace << "\n  " << x << ": K=" << hv.k << ", " << hv.reads
          << " tests, " << hv.pinned << " pinned writes, " << hv.full_clears
          << " full clears";
  }
  for (const auto& [n, why] : hb.refused) {
    trace << "\n  " << n << " refused: " << why;
  }
  return {std::move(out), true, trace.str()};
}

}  // namespace hsc::surface
