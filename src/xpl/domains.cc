/// \file domains.cc
/// \brief Domain inference: union-find units, assignments gathered by an
/// event-tree walk that carries guard constraints, then an abstract
/// evaluation fixpoint with a round cap. `algorithm.md` §5.

#include "hsc/xpl/domains.hh"

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <span>

namespace hsc::xpl {

namespace {

/// Sets larger than this degrade to their interval hull. Generous: the
/// point is to keep sentinel holes visible, not to bound memory tightly.
constexpr std::size_t kSetCap = 4096;

/// Exact-enumeration budget of one arithmetic evaluation: past this many
/// operand combinations, corners of the interval hulls.
constexpr std::size_t kComboCap = 4096;

/// Gathered-assignment budget across all events; past it, the remaining
/// updates process constraint-free (sound, blunter).
constexpr std::size_t kJobCap = 20000;

/// Fixpoint rounds before every still-growing target widens to top.
constexpr int kRounds = 200;

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

/// \p d restricted to [\p glo, \p ghi]: exact on sets (holes survive), a
/// clamp on intervals; bot when the restriction is empty.
dom restrict_dom(const dom& d, value glo, value ghi) {
  dom out;
  switch (d.k) {
    case dom::kind::bot:
      break;
    case dom::kind::top:
      out.add_interval(glo, ghi);
      break;
    case dom::kind::set:
      for (const value v : d.vals) {
        if (v >= glo && v <= ghi) out.add_value(v);
      }
      break;
    case dom::kind::interval: {
      const value l = std::max(d.lo, glo);
      const value h = std::min(d.hi, ghi);
      if (l <= h) out.add_interval(l, h);
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
  constexpr value kMin = std::numeric_limits<value>::min();
  constexpr value kMax = std::numeric_limits<value>::max();
  value lo = kMin;
  value hi = kMax;
  lia::bkind rel = k;  // orient as `var rel c`
  if (!lv) {  // c rel var: flip
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

/// Abstract evaluation of an rhs over the units' current domains, under
/// the live guard constraints. Exact set enumeration while the operand
/// combination count stays under `kComboCap`; interval corners else;
/// overflow and unhandled operators are top.
struct evaler {
  const lia::expr_factory& ex;
  const std::vector<dom>& doms;
  const std::vector<std::uint32_t>& root;  ///< position → unit root
  const env_t& env;

  [[nodiscard]] dom eval(lia::iexpr e) const {
    if (lia::expr_factory::is_const(e)) {
      dom d;
      d.add_value(ex.value(e));
      return d;
    }
    if (e == lia::iundef) {
      dom d;
      d.to_top();
      return d;
    }
    const lia::expr_node& n = ex.node(e);
    switch (static_cast<lia::ikind>(n.kind)) {
      case lia::ikind::constant: {
        dom d;
        d.add_value(static_cast<value>(n.payload));
        return d;
      }
      case lia::ikind::var: {
        const auto pos = static_cast<std::uint32_t>(n.payload);
        const dom& d = doms[root[pos]];
        const auto it = env.find(pos);
        if (it == env.end()) return d;
        return restrict_dom(d, it->second.first, it->second.second);
      }
      case lia::ikind::array:  // the value read is some cell of a's unit
        return doms[root[n.data()[1]]];
      case lia::ikind::wrap_bool: {
        dom d;
        d.add_value(0);
        d.add_value(1);
        return d;
      }
      case lia::ikind::mod: {  // e % k, k a positive constant: [0, k)
        const lia::iexpr div = n.data()[1];
        if (lia::expr_factory::is_const(div) && ex.value(div) > 0) {
          const value k = ex.value(div);
          // a wrap that cannot fire passes the operand through unwrapped —
          // the byte-arithmetic pattern `(j+1) % 256` under a guard on j
          const dom a = eval(n.data()[0]);
          if ((a.k == dom::kind::set || a.k == dom::kind::interval) &&
              a.hull_lo() >= 0 && a.hull_hi() < k) {
            return a;
          }
          dom d;
          d.add_interval(0, k - 1);
          return d;
        }
        return top();
      }
      case lia::ikind::div: {  // e / k, k a positive constant
        const lia::iexpr div = n.data()[1];
        if (!lia::expr_factory::is_const(div) || ex.value(div) <= 0) {
          return top();
        }
        const value k = ex.value(div);
        const dom a = eval(n.data()[0]);
        return map1(a, [k](long long v) { return v / k; });
      }
      case lia::ikind::plus:
        return fold(n.operands(),
                    [](long long a, long long b) { return a + b; });
      case lia::ikind::mult:
        return fold(n.operands(),
                    [](long long a, long long b) { return a * b; });
      case lia::ikind::minus: {
        const dom a = eval(n.data()[0]);
        const dom b = eval(n.data()[1]);
        return combine(a, b, [](long long x, long long y) { return x - y; });
      }
      default:  // pow, bit ops, …: unanalyzed
        return top();
    }
  }

 private:
  static dom top() {
    dom d;
    d.to_top();
    return d;
  }

  static bool fits(long long v) {
    return v >= std::numeric_limits<value>::min() &&
           v <= std::numeric_limits<value>::max();
  }

  /// Apply \p op to every value of \p a (set) or its hull ends (interval).
  template <class Op>
  static dom map1(const dom& a, Op op) {
    dom out;
    if (a.k == dom::kind::bot) return out;
    if (a.k == dom::kind::top) return top();
    if (a.k == dom::kind::set) {
      for (const value v : a.vals) {
        const long long r = op(static_cast<long long>(v));
        if (!fits(r)) return top();
        out.add_value(static_cast<value>(r));
      }
      return out;
    }
    const long long r1 = op(static_cast<long long>(a.lo));
    const long long r2 = op(static_cast<long long>(a.hi));
    if (!fits(r1) || !fits(r2)) return top();
    out.add_interval(static_cast<value>(std::min(r1, r2)),
                     static_cast<value>(std::max(r1, r2)));
    return out;
  }

  /// a ∘ b: exact when both are sets small enough, corner arithmetic else.
  template <class Op>
  static dom combine(const dom& a, const dom& b, Op op) {
    dom out;
    if (a.k == dom::kind::bot || b.k == dom::kind::bot) return out;
    if (a.k == dom::kind::top || b.k == dom::kind::top) return top();
    if (a.k == dom::kind::set && b.k == dom::kind::set &&
        a.vals.size() * b.vals.size() <= kComboCap) {
      for (const value x : a.vals) {
        for (const value y : b.vals) {
          const long long r =
              op(static_cast<long long>(x), static_cast<long long>(y));
          if (!fits(r)) return top();
          out.add_value(static_cast<value>(r));
        }
      }
      return out;
    }
    long long lo = std::numeric_limits<long long>::max();
    long long hi = std::numeric_limits<long long>::min();
    for (const long long x : {static_cast<long long>(a.hull_lo()),
                              static_cast<long long>(a.hull_hi())}) {
      for (const long long y : {static_cast<long long>(b.hull_lo()),
                                static_cast<long long>(b.hull_hi())}) {
        const long long r = op(x, y);
        lo = std::min(lo, r);
        hi = std::max(hi, r);
      }
    }
    if (!fits(lo) || !fits(hi)) return top();
    out.add_interval(static_cast<value>(lo), static_cast<value>(hi));
    return out;
  }

  /// Left fold of \p op over n-ary operands.
  template <class Op>
  dom fold(std::span<const std::uint32_t> ops, Op op) const {
    dom acc = eval(ops[0]);
    for (std::size_t i = 1; i < ops.size(); ++i) {
      acc = combine(acc, eval(ops[i]), op);
      if (acc.k == dom::kind::top) return acc;
    }
    return acc;
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
  std::vector<std::uint32_t> root(m.arity);
  for (std::uint32_t p = 0; p < m.arity; ++p) root[p] = u.find(p);

  // seeds: every unit starts from its initial values
  for (const word& s : seeds) {
    for (std::uint32_t p = 0; p < s.size(); ++p) {
      doms[root[p]].add_value(s[p]);
    }
  }

  // gather the assignments by walking every event's term tree, carrying
  // the live guard constraints; havoc joins directly (no rhs to evaluate)
  const lia::expr_factory& ex = *m.ex;
  struct job {
    const action* act = nullptr;
    env_t env;
    std::uint32_t dst = 0;
  };
  std::vector<job> jobs;
  std::vector<bool> visited(m.pool.size(), false);

  const auto emit = [&](const term& t, const env_t& env) {
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
      if (!lia::expr_factory::is_const(a.rhs) && a.rhs != lia::iundef) {
        const lia::expr_node& n = ex.node(a.rhs);
        const auto k = static_cast<lia::ikind>(n.kind);
        if (k == lia::ikind::mod) {  // keep the visible-assumption tag
          const lia::iexpr div = n.data()[1];
          if (lia::expr_factory::is_const(div) && ex.value(div) > 0) {
            via_mod[dst] = true;
          }
        }
      }
      jobs.push_back({&a, env, dst});
    }
  };

  // returns the positions written in the subtree; env evolves along a seq
  const std::function<std::set<std::uint32_t>(std::uint32_t, env_t&)> walk =
      [&](std::uint32_t ti, env_t& env) -> std::set<std::uint32_t> {
    std::set<std::uint32_t> w;
    if (jobs.size() >= kJobCap) return w;  // budget: fallback covers the rest
    const term& t = m.pool[ti];
    switch (t.k) {
      case term::kind::filter:
        constrain(ex, t.guard, env);
        break;
      case term::kind::update:
        visited[ti] = true;
        emit(t, env);
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
          env_t branch = env;  // a branch's guards never leak out
          const auto kw = walk(k, branch);
          w.insert(kw.begin(), kw.end());
        }
        for (const std::uint32_t c : w) env.erase(c);  // its writes do
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
  // updates the walk missed (unreachable, or past the budget): guard-free
  for (std::size_t ti = 0; ti < m.pool.size(); ++ti) {
    if (m.pool[ti].k == term::kind::update && !visited[ti]) {
      emit(m.pool[ti], env_t{});
    }
  }

  // abstract-evaluation fixpoint; joins only grow, arithmetic may diverge,
  // so past the round cap every still-growing target widens to top (tops
  // absorb, so the trailing loop runs at most once per unit)
  bool changed = true;
  for (int round = 0; changed && round < kRounds; ++round) {
    changed = false;
    for (const job& j : jobs) {
      const dom v = evaler{ex, doms, root, j.env}.eval(j.act->rhs);
      if (v.k != dom::kind::bot) changed |= doms[j.dst].join(v);
    }
  }
  while (changed) {
    changed = false;
    std::set<std::uint32_t> grew;
    for (const job& j : jobs) {
      const dom v = evaler{ex, doms, root, j.env}.eval(j.act->rhs);
      if (v.k != dom::kind::bot && doms[j.dst].join(v)) {
        changed = true;
        grew.insert(j.dst);
      }
    }
    for (const std::uint32_t d : grew) doms[d].to_top();
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
