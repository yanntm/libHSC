/// \file domains.cc
/// \brief Domain inference: union-find units, one classification walk
/// over the events carrying guard constraints, then a fixpoint over the
/// (restricted, possibly shifted) copy edges. Structural throughout — no
/// expression evaluation, no widening. `algorithm.md` §5.

#include "hsc/xpl/domains.hh"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace hsc::xpl {

namespace {

/// Sets larger than this degrade to their interval hull. Generous: the
/// point is to keep sentinel holes visible, not to bound memory tightly.
constexpr std::size_t kSetCap = 4096;

constexpr value kMin = std::numeric_limits<value>::min();
constexpr value kMax = std::numeric_limits<value>::max();

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

  [[nodiscard]] value hull_lo() const {
    return k == kind::set ? *vals.begin() : lo;
  }
  [[nodiscard]] value hull_hi() const {
    return k == kind::set ? *vals.rbegin() : hi;
  }

 private:
  void to_interval() {
    lo = *vals.begin();
    hi = *vals.rbegin();
    vals.clear();
    k = kind::interval;
  }
};

/// Saturating value addition (shifts are small; saturation keeps hulls
/// sound at the extremes without an overflow case).
value sat_add(value a, value b) {
  const long long r = static_cast<long long>(a) + b;
  if (r < kMin) return kMin;
  if (r > kMax) return kMax;
  return static_cast<value>(r);
}

/// \p d restricted to [\p glo, \p ghi], then shifted by \p o: exact on
/// sets (holes survive), a clamp on intervals; bot when the restriction
/// is empty. A restricted top is the window itself — the read was inside
/// it; an unrestricted top stays top.
dom restrict_shift(const dom& d, value glo, value ghi, value o) {
  dom out;
  switch (d.k) {
    case dom::kind::bot:
      break;
    case dom::kind::top:
      if (glo == kMin && ghi == kMax) {
        out.to_top();
      } else {
        out.add_interval(glo == kMin ? kMin : sat_add(glo, o),
                         ghi == kMax ? kMax : sat_add(ghi, o));
      }
      break;
    case dom::kind::set:
      for (const value v : d.vals) {
        if (v >= glo && v <= ghi) out.add_value(sat_add(v, o));
      }
      break;
    case dom::kind::interval: {
      const value l = std::max(d.lo, glo);
      const value h = std::min(d.hi, ghi);
      if (l <= h) {
        out.add_interval(l == kMin ? kMin : sat_add(l, o),
                         h == kMax ? kMax : sat_add(h, o));
      }
      break;
    }
  }
  return out;
}

/// Live guard constraints: position → inclusive [lo, hi].
using env_t = std::map<std::uint32_t, std::pair<value, value>>;

/// Gather var–constant constraints from the conjunction atoms of \p g
/// into \p env, intersecting with what is already there. Disjunctions,
/// negations and var–var atoms contribute nothing.
void constrain(const lia::expr_factory& ex, lia::bexpr g, env_t& env) {
  if (g == lia::btrue || g == lia::bfalse || g == lia::bundef) return;
  const lia::bkind k = ex.bool_kind(g);
  const lia::expr_node& n = ex.bool_node(g);
  if (k == lia::bkind::conj) {
    for (const lia::bexpr c : n.operands()) constrain(ex, c, env);
    return;
  }
  if (k != lia::bkind::eq && k != lia::bkind::lt && k != lia::bkind::leq &&
      k != lia::bkind::gt && k != lia::bkind::geq) {
    return;
  }
  const auto side = [&](lia::iexpr e,
                        bool* is_var) -> std::pair<std::uint32_t, value> {
    if (lia::expr_factory::is_const(e)) {
      *is_var = false;
      return {0, ex.value(e)};
    }
    if (e != lia::iundef) {
      const lia::expr_node& sn = ex.node(e);
      if (static_cast<lia::ikind>(sn.kind) == lia::ikind::var) {
        *is_var = true;
        return {static_cast<std::uint32_t>(sn.payload), 0};
      }
    }
    *is_var = false;
    return {0, 0};
  };
  bool lv = false;
  bool rv = false;
  const auto l = side(n.data()[0], &lv);
  const auto r = side(n.data()[1], &rv);
  if (lv == rv) return;  // var–var or const–const: nothing usable
  const std::uint32_t pos = lv ? l.first : r.first;
  const value c = lv ? r.second : l.second;
  value lo = kMin;
  value hi = kMax;
  lia::bkind rel = k;  // orient as `var rel c`
  if (!lv) {           // c rel var: flip
    if (k == lia::bkind::lt) rel = lia::bkind::gt;
    if (k == lia::bkind::leq) rel = lia::bkind::geq;
    if (k == lia::bkind::gt) rel = lia::bkind::lt;
    if (k == lia::bkind::geq) rel = lia::bkind::leq;
  }
  switch (rel) {
    case lia::bkind::eq:
      lo = hi = c;
      break;
    case lia::bkind::lt:
      if (c == kMin) return;
      hi = c - 1;
      break;
    case lia::bkind::leq:
      hi = c;
      break;
    case lia::bkind::gt:
      if (c == kMax) return;
      lo = c + 1;
      break;
    case lia::bkind::geq:
      lo = c;
      break;
    default:
      return;
  }
  auto [it, fresh] = env.emplace(pos, std::pair<value, value>{lo, hi});
  if (!fresh) {
    it->second.first = std::max(it->second.first, lo);
    it->second.second = std::min(it->second.second, hi);
  }
}

/// `var + Σ consts` / `var − const` recognizer: the single-source affine
/// shape the edge rule accepts. Returns the var position and the signed
/// offset; nullopt for anything else.
std::optional<std::pair<std::uint32_t, value>> var_offset(
    const lia::expr_factory& ex, lia::iexpr e) {
  if (e == lia::iundef || lia::expr_factory::is_const(e)) return std::nullopt;
  const lia::expr_node& n = ex.node(e);
  const auto k = static_cast<lia::ikind>(n.kind);
  if (k == lia::ikind::var) {
    return std::pair{static_cast<std::uint32_t>(n.payload), value{0}};
  }
  if (k == lia::ikind::plus) {
    std::optional<std::uint32_t> var;
    long long off = 0;
    for (const lia::iexpr op : n.operands()) {
      if (lia::expr_factory::is_const(op)) {
        off += ex.value(op);
        continue;
      }
      if (op == lia::iundef) return std::nullopt;
      const lia::expr_node& on = ex.node(op);
      if (static_cast<lia::ikind>(on.kind) != lia::ikind::var || var) {
        return std::nullopt;  // second variable, or a nested operator
      }
      var = static_cast<std::uint32_t>(on.payload);
    }
    if (!var || off < kMin || off > kMax) return std::nullopt;
    return std::pair{*var, static_cast<value>(off)};
  }
  if (k == lia::ikind::minus) {
    const lia::iexpr a = n.data()[0];
    const lia::iexpr b = n.data()[1];
    if (a == lia::iundef || lia::expr_factory::is_const(a)) return std::nullopt;
    const lia::expr_node& an = ex.node(a);
    if (static_cast<lia::ikind>(an.kind) != lia::ikind::var ||
        !lia::expr_factory::is_const(b)) {
      return std::nullopt;
    }
    const long long off = -static_cast<long long>(ex.value(b));
    if (off < kMin || off > kMax) return std::nullopt;
    return std::pair{static_cast<std::uint32_t>(an.payload),
                     static_cast<value>(off)};
  }
  return std::nullopt;
}

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
    std::span<const std::vector<std::uint32_t>> groups,
    std::span<const std::optional<std::pair<value, value>>> declared) {
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
  std::vector<std::uint32_t> root(m.arity);
  for (std::uint32_t p = 0; p < m.arity; ++p) root[p] = u.find(p);

  // per root unit, the declared window: the union of the members'
  // [lo, hi) declarations, present only when every member declares. A
  // write outside a declared bound is a run error, never a state, so a
  // unit's domain can never leave its window — "top" for a declared
  // unit means its whole window, and every join clips to it.
  std::vector<std::optional<std::pair<value, value>>> decl_win(m.arity);
  if (!declared.empty()) {
    std::vector<std::vector<std::uint32_t>> members_of(m.arity);
    for (std::uint32_t p = 0; p < m.arity; ++p) {
      members_of[root[p]].push_back(p);
    }
    for (std::uint32_t r = 0; r < m.arity; ++r) {
      if (members_of[r].empty()) continue;
      value clo = kMax;
      value chi = kMin;
      bool all = true;
      for (const std::uint32_t p : members_of[r]) {
        if (!declared[p]) {
          all = false;
          break;
        }
        clo = std::min(clo, declared[p]->first);
        chi = std::max(chi, static_cast<value>(declared[p]->second - 1));
      }
      if (all && clo <= chi) decl_win[r] = {clo, chi};
    }
  }
  // an unanalyzable assignment blunts to the declared window, top else
  const auto blunt = [&](std::uint32_t dst) {
    if (decl_win[dst]) {
      doms[dst].add_interval(decl_win[dst]->first, decl_win[dst]->second);
    } else {
      doms[dst].to_top();
    }
  };

  // seeds: every unit starts from its initial values
  for (const word& s : seeds) {
    for (std::uint32_t p = 0; p < s.size(); ++p) {
      doms[root[p]].add_value(s[p]);
    }
  }

  // classification: one pass per assignment occurrence. Direct facts join
  // immediately; reads become edges — restricted by the guard constraints
  // live at that occurrence, shifted for the single-var affine shape.
  struct edge {
    std::uint32_t src = 0, dst = 0;
    value rlo = kMin, rhi = kMax;  ///< restriction on the read
    value shift = 0;
    bool check = false;  ///< an unguarded genuine shift: allowed only when
                         ///< it cannot diverge (dst declared, or acyclic)
  };
  std::vector<edge> edges;
  const lia::expr_factory& ex = *m.ex;

  const auto classify = [&](const term& t, const env_t& env) {
    for (const action& a : t.acts) {
      const std::uint32_t dst = root[a.lhs.cells.front()];
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
        blunt(dst);
        continue;
      }
      // the single-var affine shape: x := v [± consts] — an edge from v's
      // unit, restricted by the live constraints on v. A genuine shift
      // whose advancing side no guard bounds diverges on a cycle; the
      // check defers that decision to the cycle test below.
      if (const auto vo = var_offset(ex, rhs)) {
        const auto [pos, off] = *vo;
        value rlo = kMin;
        value rhi = kMax;
        if (const auto it = env.find(pos); it != env.end()) {
          rlo = it->second.first;
          rhi = it->second.second;
        }
        const bool unguarded =
            (off > 0 && rhi == kMax) || (off < 0 && rlo == kMin);
        edges.push_back({root[pos], dst, rlo, rhi, off, unguarded});
        continue;
      }
      const lia::expr_node& n = ex.node(rhs);
      switch (static_cast<lia::ikind>(n.kind)) {
        case lia::ikind::constant:
          doms[dst].add_value(static_cast<value>(n.payload));
          break;
        case lia::ikind::array:  // x := (at a e) — a copy from a's unit
          edges.push_back({root[n.data()[1]], dst, kMin, kMax, 0, false});
          break;
        case lia::ikind::mod: {  // x := e % k — [0, k), operand nonnegative
          const lia::iexpr div = n.data()[1];
          if (lia::expr_factory::is_const(div) && ex.value(div) > 0) {
            doms[dst].add_interval(0, ex.value(div) - 1);
            via_mod[dst] = true;
          } else {
            blunt(dst);
          }
          break;
        }
        case lia::ikind::wrap_bool:  // a boolean read as 0/1
          doms[dst].add_value(0);
          doms[dst].add_value(1);
          break;
        default:  // other arithmetic: honestly unconstrained here; the
                  // declared window is all a declared unit can hold
          blunt(dst);
          break;
      }
    }
  };

  // walk every event's term tree with the live constraints: filters add
  // them, an update reads its pre-state and then kills the constraints of
  // what it wrote, alt branches fork the environment (a branch's guards
  // never leak out; its writes invalidate for what follows)
  std::vector<bool> visited(m.pool.size(), false);
  const std::function<std::set<std::uint32_t>(std::uint32_t, env_t&)> walk =
      [&](std::uint32_t ti, env_t& env) -> std::set<std::uint32_t> {
    const term& t = m.pool[ti];
    std::set<std::uint32_t> w;
    switch (t.k) {
      case term::kind::filter:
        constrain(ex, t.guard, env);
        break;
      case term::kind::update:
        visited[ti] = true;
        classify(t, env);
        for (const action& a : t.acts) {
          for (const std::uint32_t c : a.lhs.cells) {
            w.insert(c);
            env.erase(c);
          }
        }
        break;
      case term::kind::seq:
        for (const std::uint32_t k : t.kids) {
          const auto kw = walk(k, env);
          w.insert(kw.begin(), kw.end());
        }
        break;
      case term::kind::alt:
        for (const std::uint32_t k : t.kids) {
          env_t branch = env;
          const auto kw = walk(k, branch);
          w.insert(kw.begin(), kw.end());
        }
        for (const std::uint32_t c : w) env.erase(c);
        break;
      case term::kind::abort:
        break;
    }
    return w;
  };
  for (const event& e : m.events) {
    env_t env;
    walk(e.root, env);
  }
  for (std::size_t ti = 0; ti < m.pool.size(); ++ti) {
    if (m.pool[ti].k == term::kind::update && !visited[ti]) {
      const env_t none;
      classify(m.pool[ti], none);
    }
  }

  // an unguarded shift can only diverge by feeding itself: if its
  // destination has a declared window (every join clips to it) or cannot
  // reach back to its source through the edges, it is safe; on a cycle
  // into an undeclared unit, the honest answer is top and the edge goes
  const auto reaches = [&](std::uint32_t from, std::uint32_t to) {
    std::set<std::uint32_t> seen{from};
    std::vector<std::uint32_t> stack{from};
    while (!stack.empty()) {
      const std::uint32_t n = stack.back();
      stack.pop_back();
      if (n == to) return true;
      for (const edge& e : edges) {
        if (e.src == n && seen.insert(e.dst).second) stack.push_back(e.dst);
      }
    }
    return false;
  };
  std::erase_if(edges, [&](const edge& e) {
    if (!e.check || e.shift == 0 || decl_win[e.dst]) return false;
    if (!reaches(e.dst, e.src)) return false;
    doms[e.dst].to_top();
    return true;
  });

  // fixpoint over the edges, every join clipped to the destination's
  // declared window. Terminates without any cap: copies invent no
  // values, guarded shifts live inside their constant window shifted,
  // and the surviving unguarded shifts either land in a declared window
  // or sit on no cycle — every hull stays within a fixed finite range.
  bool changed = true;
  while (changed) {
    changed = false;
    for (const edge& e : edges) {
      dom v = restrict_shift(doms[e.src], e.rlo, e.rhi, e.shift);
      if (decl_win[e.dst]) {
        v = restrict_shift(v, decl_win[e.dst]->first,
                           decl_win[e.dst]->second, 0);
      }
      if (v.k != dom::kind::bot) changed |= doms[e.dst].join(v);
    }
  }

  // final clip: direct facts (a constant written on a dead branch, a
  // havoc range wider than the declaration) may still exceed a declared
  // window; stored values cannot
  for (std::uint32_t r = 0; r < m.arity; ++r) {
    if (!decl_win[r]) continue;
    if (doms[r].k == dom::kind::top) {
      doms[r] = dom{};
      doms[r].add_interval(decl_win[r]->first, decl_win[r]->second);
    } else if (doms[r].k != dom::kind::bot) {
      doms[r] = restrict_shift(doms[r], decl_win[r]->first,
                               decl_win[r]->second, 0);
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
