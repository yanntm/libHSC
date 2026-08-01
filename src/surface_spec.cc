/// \file surface_spec.cc
/// \brief Reading declarations into a `spec`, and the domain analysis
/// over one. Algorithms on the datum structure — no translator, no
/// diagrams; the explicit engine does any interpretation needed.

#include "hsc/surface/spec.hh"

#include <algorithm>
#include <charconv>
#include <ostream>

#include "hsc/surface/translate.hh"
#include "hsc/xpl/interpret/fire.hh"

namespace hsc::surface {

namespace {

[[noreturn]] void fail(const datum& d, const std::string& msg) {
  throw translate_error(d.line(), msg);
}

std::int32_t as_int(const datum& d, const char* what) {
  if (!d.is_atom()) fail(d, std::string("expected an integer ") + what);
  const std::string& t = d.text();
  std::int32_t v = 0;
  const auto* end = t.data() + t.size();
  const auto res = std::from_chars(t.data(), end, v);
  if (res.ec != std::errc{} || res.ptr != end) {
    fail(d, std::string("expected an integer ") + what + ", found '" + t +
               "'");
  }
  return v;
}

/// Flatten a SORT datum to its leaf names, left to right.
void flatten_shape(const datum& s, std::vector<std::string>& out) {
  if (s.is_atom()) {
    if (s.text() != "unit") out.push_back(s.text());
    return;
  }
  if (s.items().empty()) fail(s, "malformed sort");
  const std::string& h = s.head();
  if (h != "pair" && h != "spine" && h != "balanced") {
    fail(s, "unknown sort constructor '" + h + "'");
  }
  for (std::size_t i = 1; i < s.items().size(); ++i) {
    flatten_shape(s.items()[i], out);
  }
}

/// Materialize instance \p i of a family clause: `(at@ a δ)` becomes the
/// cell atom of component (i+δ) mod n.
datum instantiate(const datum& d, long long i, long long n,
                  const std::map<std::string, std::vector<std::string>>&
                      array_cells) {
  if (!d.is_list() || d.items().empty()) return d;
  if (d.head() == "at@") {
    const auto it = array_cells.find(d.items()[1].text());
    if (it == array_cells.end()) fail(d, "at@ names an unknown array");
    const long long c =
        ((i + std::stoll(d.items()[2].text())) % n + n) % n;
    return datum::atom(it->second[static_cast<std::size_t>(c)], d.line());
  }
  std::vector<datum> kids;
  kids.reserve(d.items().size());
  for (const datum& k : d.items()) {
    kids.push_back(instantiate(k, i, n, array_cells));
  }
  return datum::list(std::move(kids), d.line());
}

}  // namespace

spec spec::read(const std::vector<datum>& forms) {
  spec s;
  std::map<std::string, std::pair<bool, std::pair<std::int32_t, std::int32_t>>>
      declared;  // name → (bounded, [lo, hi))
  std::map<std::string, std::vector<std::string>> array_cells;
  for (const datum& f : forms) {
    if (!f.is_list() || f.items().empty()) continue;
    const std::string& kw = f.head();
    if (kw == "leaf") {
      const std::string& name = f.items()[1].text();
      if (f.items().size() > 2) {
        declared[name] = {true,
                          {as_int(f.items()[2], "lower bound"),
                           as_int(f.items()[3], "upper bound")}};
      } else {
        declared[name] = {false, {0, 0}};
      }
    } else if (kw == "array") {
      std::vector<std::string> cells;
      for (std::size_t i = 2; i < f.items().size(); ++i) {
        cells.push_back(f.items()[i].text());
      }
      array_cells[f.items()[1].text()] = std::move(cells);
    } else if (kw == "shape") {
      flatten_shape(f.items()[1], s.order_);
      for (std::uint32_t p = 0; p < s.order_.size(); ++p) {
        const auto it = declared.find(s.order_[p]);
        if (it == declared.end()) {
          fail(f, "shape names undeclared leaf '" + s.order_[p] + "'");
        }
        s.leaves_[s.order_[p]] = {p, it->second.first,
                                  it->second.second.first,
                                  it->second.second.second};
      }
    } else if (kw == "init") {
      // pair form: every item a 2-list whose head is a declared leaf
      bool pairs = f.items().size() > 1;
      for (std::size_t i = 1; i < f.items().size(); ++i) {
        const datum& it = f.items()[i];
        pairs = pairs && it.is_list() && it.items().size() == 2 &&
                it.items()[0].is_atom() &&
                s.leaves_.contains(it.items()[0].text());
      }
      if (pairs) {
        for (std::size_t i = 1; i < f.items().size(); ++i) {
          const datum& it = f.items()[i];
          s.init_pairs_.emplace_back(
              s.leaves_.at(it.items()[0].text()).pos,
              as_int(it.items()[1], "init value"));
        }
      } else if (f.items().size() == 2) {  // (init EVTERM)
        const datum& ev = f.items()[1];
        if (!ev.is_list() ||
            (ev.head() != "when" && ev.head() != "do" && ev.head() != "seq")) {
          fail(f, "init event beyond when/do/seq: unsupported for analysis");
        }
        if (ev.head() == "seq") {
          for (std::size_t i = 1; i < ev.items().size(); ++i) {
            s.init_event_.push_back(ev.items()[i]);
          }
        } else {
          s.init_event_.push_back(ev);
        }
      }
    } else if (kw == "event") {
      s.events_.push_back(
          {f.items()[1].text(), f.line(),
           std::vector<datum>(f.items().begin() + 2, f.items().end())});
    } else if (kw == "family") {
      const std::string& name = f.items()[1].text();
      const long long n = as_int(f.items()[2], "family size");
      for (long long i = 0; i < n; ++i) {
        std::vector<datum> inst;
        for (std::size_t c = 3; c < f.items().size(); ++c) {
          inst.push_back(instantiate(f.items()[c], i, n, array_cells));
        }
        s.events_.push_back(
            {name + "_" + std::to_string(i), f.line(), std::move(inst)});
      }
    }
  }
  for (const auto& [name, cells] : array_cells) {
    std::vector<std::uint32_t> ps;
    for (const std::string& cell : cells) {
      const auto it = s.leaves_.find(cell);
      if (it == s.leaves_.end()) {
        continue;  // an array of unplaced leaves: not on the frontier
      }
      ps.push_back(it->second.pos);
    }
    if (!ps.empty()) s.arrays_[name] = std::move(ps);
  }
  return s;
}

std::optional<std::uint32_t> spec::position(const std::string& name) const {
  const auto it = leaves_.find(name);
  if (it == leaves_.end()) return std::nullopt;
  return it->second.pos;
}

std::optional<std::vector<std::uint32_t>> spec::array(
    const std::string& name) const {
  const auto it = arrays_.find(name);
  if (it == arrays_.end()) return std::nullopt;
  return it->second;
}

std::optional<std::pair<std::int32_t, std::int32_t>> spec::bound(
    std::uint32_t pos) const {
  const leaf_info& l = leaves_.at(order_.at(pos));
  if (!l.bounded) return std::nullopt;
  return std::pair{l.lo, l.hi};
}

std::vector<xpl::word> spec::seeds(lia::expr_factory& ex) const {
  xpl::word base(order_.size(), 0);
  for (const auto& [name, l] : leaves_) {
    if (l.bounded) base[l.pos] = l.lo;
  }
  for (const auto& [pos, v] : init_pairs_) base[pos] = v;
  if (init_event_.empty()) return {base};
  // the seed is the image of the base word by one explicit firing
  const expr_reader reader(ex, *this);
  const xpl_source src{"init", 0, init_event_};
  const xpl::model m =
      build_xpl_model(order_.size(), ex, reader, *this, {&src, 1});
  std::vector<xpl::successor> out;
  if (xpl::quick_enabled(m, 0, base)) xpl::fire(m, 0, base, out);
  if (out.empty()) {
    throw translate_error(0, "init event: empty image, malformed model");
  }
  std::vector<xpl::word> seeds;
  seeds.reserve(out.size());
  for (xpl::successor& sc : out) seeds.push_back(std::move(sc.s));
  return seeds;
}

std::vector<unit_domain> analyze_domains(const std::vector<datum>& forms) {
  const spec s = spec::read(forms);
  std::vector<unit_domain> out;
  if (s.order().empty()) return out;
  lia::expr_factory ex;
  const expr_reader reader(ex, s);
  const xpl::model m =
      build_xpl_model(s.order().size(), ex, reader, s, s.events());
  const std::vector<xpl::word> seeds = s.seeds(ex);
  std::vector<std::vector<std::uint32_t>> groups;
  std::map<std::vector<std::uint32_t>, std::string> group_name;
  for (const auto& [name, ps] : s.arrays()) {
    std::vector<std::uint32_t> sorted = ps;
    std::sort(sorted.begin(), sorted.end());
    group_name[sorted] = name;
    groups.push_back(std::move(sorted));
  }
  for (auto& r : xpl::infer_domains(m, seeds, groups)) {
    unit_domain u;
    const auto it = group_name.find(r.positions);
    if (it != group_name.end()) {
      u.name = it->second;
      u.is_array = true;
    } else if (r.positions.size() == 1) {
      u.name = s.order()[r.positions.front()];
    } else {
      u.name = s.order()[r.positions.front()] + "+";  // merged beyond decl
    }
    if (const auto b = s.bound(r.positions.front())) {
      u.declared = true;
      u.decl_lo = b->first;
      u.decl_hi = b->second;
    }
    u.report = std::move(r);
    out.push_back(std::move(u));
  }
  return out;
}

void print_domains(std::ostream& os, const std::vector<unit_domain>& units) {
  std::size_t nset = 0, nint = 0, ntop = 0, nfrozen = 0, nwidened = 0;
  for (const unit_domain& u : units) {
    const xpl::domain_report& r = u.report;
    os << "xdom " << u.name;
    switch (r.k) {
      case xpl::domain_report::kind::set: {
        ++nset;
        const auto lo = r.values.front();
        const auto hi = r.values.back();
        const std::size_t holes =
            static_cast<std::size_t>(hi - lo + 1) - r.values.size();
        os << " kind=set size=" << r.values.size() << " lo=" << lo
           << " hi=" << hi << " holes=" << holes;
        break;
      }
      case xpl::domain_report::kind::interval:
        ++nint;
        os << " kind=interval size=0 lo=" << r.lo << " hi=" << r.hi
           << " holes=0";
        break;
      default:
        ++ntop;
        os << " kind=top size=0 lo=0 hi=0 holes=0";
        break;
    }
    if (u.declared) os << " decl=[" << u.decl_lo << ',' << u.decl_hi << ')';
    else os << " decl=-";
    os << " frozen=" << (r.assigned ? 0 : 1) << " mod=" << (r.via_mod ? 1 : 0);
    if (r.widened) os << " widened=1";
    nfrozen += r.assigned ? 0 : 1;
    nwidened += r.widened ? 1 : 0;
    if (r.k == xpl::domain_report::kind::set && r.values.size() <= 32) {
      os << " {";
      for (std::size_t i = 0; i < r.values.size(); ++i) {
        os << (i ? " " : "") << r.values[i];
      }
      os << '}';
    }
    os << '\n';
  }
  os << "xdomains units=" << units.size() << " set=" << nset
     << " interval=" << nint << " top=" << ntop << " frozen=" << nfrozen;
  if (nwidened) os << " widened=" << nwidened;
  if (!units.empty() && units.front().report.walk_budget_hit) {
    os << " walk-budget-hit=1";
  }
  os << '\n';
}

}  // namespace hsc::surface
