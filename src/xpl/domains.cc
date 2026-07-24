/// \file domains.cc
/// \brief Domain inference: union-find units, one classification pass over
/// the assignments, then a worklist fixpoint over the copy edges.
/// `algorithm.md` §5.

#include "hsc/xpl/domains.hh"

#include <algorithm>
#include <deque>
#include <set>

namespace hsc::xpl {

namespace {

/// Sets larger than this degrade to their interval hull. Generous: the
/// point is to keep sentinel holes visible, not to bound memory tightly.
constexpr std::size_t kSetCap = 4096;

// --- the abstract value ----------------------------------------------------

struct dom {
  enum class kind : std::uint8_t { bot, set, interval, top };
  kind k = kind::bot;
  std::set<value> vals;
  value lo = 0, hi = 0;

  bool add_value(value v) {
    switch (k) {
      case kind::bot:
        k = kind::set;
        vals.insert(v);
        return true;
      case kind::set:
        if (!vals.insert(v).second) return false;
        if (vals.size() > kSetCap) to_interval();
        return true;
      case kind::interval:
        if (v >= lo && v <= hi) return false;
        lo = std::min(lo, v);
        hi = std::max(hi, v);
        return true;
      case kind::top:
        return false;
    }
    return false;
  }

  bool add_interval(value l, value h) {  // inclusive
    if (k == kind::top) return false;
    const value nl = k == kind::bot ? l : std::min(hull_lo(), l);
    const value nh = k == kind::bot ? h : std::max(hull_hi(), h);
    if (k == kind::interval && nl == lo && nh == hi) return false;
    vals.clear();
    k = kind::interval;
    lo = nl;
    hi = nh;
    return true;
  }

  bool to_top() {
    if (k == kind::top) return false;
    vals.clear();
    k = kind::top;
    return true;
  }

  /// this ⊔= other; true when this changed.
  bool join(const dom& o) {
    switch (o.k) {
      case dom::kind::bot:
        return false;
      case dom::kind::top:
        return to_top();
      case dom::kind::interval:
        return add_interval(o.lo, o.hi);
      case dom::kind::set: {
        bool changed = false;
        for (const value v : o.vals) changed |= add_value(v);
        return changed;
      }
    }
    return false;
  }

 private:
  [[nodiscard]] value hull_lo() const {
    return k == kind::set ? *vals.begin() : lo;
  }
  [[nodiscard]] value hull_hi() const {
    return k == kind::set ? *vals.rbegin() : hi;
  }
  void to_interval() {
    lo = *vals.begin();
    hi = *vals.rbegin();
    vals.clear();
    k = kind::interval;
  }
};

// --- union-find over positions ---------------------------------------------

struct uf {
  std::vector<std::uint32_t> parent;
  explicit uf(std::size_t n) : parent(n) {
    for (std::size_t i = 0; i < n; ++i) {
      parent[i] = static_cast<std::uint32_t>(i);
    }
  }
  std::uint32_t find(std::uint32_t x) {
    while (parent[x] != x) x = parent[x] = parent[parent[x]];
    return x;
  }
  void unite(std::uint32_t a, std::uint32_t b) {
    a = find(a);
    b = find(b);
    if (a != b) parent[std::max(a, b)] = std::min(a, b);
  }
  void unite_all(std::span<const std::uint32_t> ps) {
    for (std::size_t i = 1; i < ps.size(); ++i) unite(ps[0], ps[i]);
  }
};

/// Union the cells of every array node reachable in \p e — an array is
/// one unit wherever it is touched.
void unite_array_nodes(const lia::expr_factory& ex, lia::iexpr e, uf& u) {
  if (lia::expr_factory::is_const(e) || e == lia::iundef) return;
  const lia::expr_node& n = ex.node(e);
  const auto k = static_cast<lia::ikind>(n.kind);
  if (k == lia::ikind::constant || k == lia::ikind::var) return;
  if (k == lia::ikind::array) {
    u.unite_all(n.operands().subspan(1));
    unite_array_nodes(ex, n.data()[0], u);  // the index expression
    return;
  }
  if (k == lia::ikind::wrap_bool) return;  // bexpr operand: no reads merged
  for (const lia::iexpr op : n.operands()) unite_array_nodes(ex, op, u);
}

}  // namespace

std::vector<domain_report> infer_domains(
    const model& m, std::span<const word> seeds,
    std::span<const std::vector<std::uint32_t>> groups) {
  uf u(m.arity);
  for (const auto& g : groups) u.unite_all(g);
  for (const term& t : m.pool) {
    if (t.k != term::kind::update) continue;
    for (const action& a : t.acts) {
      u.unite_all(a.lhs.cells);
      if (a.k == action::kind::assign) unite_array_nodes(*m.ex, a.rhs, u);
      if (a.lhs.indexed) unite_array_nodes(*m.ex, a.lhs.index, u);
    }
  }

  std::vector<dom> doms(m.arity);
  std::vector<bool> assigned(m.arity, false);
  std::vector<bool> via_mod(m.arity, false);
  std::vector<std::pair<std::uint32_t, std::uint32_t>> edges;  // src → dst

  // seeds: every unit starts from its initial values
  for (const word& s : seeds) {
    for (std::uint32_t p = 0; p < s.size(); ++p) {
      doms[u.find(p)].add_value(s[p]);
    }
  }

  // one classification pass over the assignments
  const lia::expr_factory& ex = *m.ex;
  for (const term& t : m.pool) {
    if (t.k != term::kind::update) continue;
    for (const action& a : t.acts) {
      const std::uint32_t dst = u.find(a.lhs.cells.front());
      assigned[dst] = true;
      if (a.k == action::kind::havoc) {
        if (a.hi <= a.lo) continue;  // empty range assigns nothing
        if (static_cast<std::size_t>(a.hi) - a.lo <= kSetCap) {
          for (value v = a.lo; v < a.hi; ++v) doms[dst].add_value(v);
        } else {
          doms[dst].add_interval(a.lo, a.hi - 1);
        }
        continue;
      }
      const lia::iexpr rhs = a.rhs;
      if (lia::expr_factory::is_const(rhs)) {
        doms[dst].add_value(ex.value(rhs));
        continue;
      }
      if (rhs == lia::iundef) {
        doms[dst].to_top();
        continue;
      }
      const lia::expr_node& n = ex.node(rhs);
      switch (static_cast<lia::ikind>(n.kind)) {
        case lia::ikind::constant:
          doms[dst].add_value(static_cast<value>(n.payload));
          break;
        case lia::ikind::var:  // x := y — a copy edge
          edges.emplace_back(u.find(static_cast<std::uint32_t>(n.payload)),
                             dst);
          break;
        case lia::ikind::array:  // x := (at a e) — a copy from a's unit
          edges.emplace_back(u.find(n.data()[1]), dst);
          break;
        case lia::ikind::mod: {  // x := e % k — [0, k), operand nonnegative
          const lia::iexpr div = n.data()[1];
          if (lia::expr_factory::is_const(div) && ex.value(div) > 0) {
            doms[dst].add_interval(0, ex.value(div) - 1);
            via_mod[dst] = true;
          } else {
            doms[dst].to_top();
          }
          break;
        }
        case lia::ikind::wrap_bool:  // a boolean read as 0/1
          doms[dst].add_value(0);
          doms[dst].add_value(1);
          break;
        default:  // arithmetic: honestly unconstrained (a counter stays top)
          doms[dst].to_top();
          break;
      }
    }
  }

  // fixpoint over the copy edges: joins only grow, so this terminates
  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto& [src, dst] : edges) {
      if (src != dst) changed |= doms[dst].join(doms[src]);
    }
  }

  // reports, one per root unit, positions gathered back
  std::vector<std::vector<std::uint32_t>> members(m.arity);
  for (std::uint32_t p = 0; p < m.arity; ++p) members[u.find(p)].push_back(p);
  std::vector<domain_report> out;
  for (std::uint32_t r = 0; r < m.arity; ++r) {
    if (members[r].empty()) continue;
    domain_report rep;
    rep.positions = std::move(members[r]);
    rep.assigned = assigned[r];
    rep.via_mod = via_mod[r];
    const dom& d = doms[r];
    switch (d.k) {
      case dom::kind::set:
        rep.k = domain_report::kind::set;
        rep.values.assign(d.vals.begin(), d.vals.end());
        break;
      case dom::kind::interval:
        rep.k = domain_report::kind::interval;
        rep.lo = d.lo;
        rep.hi = d.hi;
        break;
      default:  // top; bot cannot happen — seeds feed every unit
        rep.k = domain_report::kind::top;
        break;
    }
    out.push_back(std::move(rep));
  }
  return out;
}

}  // namespace hsc::xpl
