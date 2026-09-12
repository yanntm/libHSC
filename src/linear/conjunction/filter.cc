/// Scheduling and residual-sum filtering on existing diagram arcs.
#include "hsc/linear/conjunction/filter.hh"
#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include "hsc/util/hash.hh"
#include "hsc/core/manager.hh"
#include "hsc/leaves/int_set.hh"

namespace hsc::linear {
namespace {
using core::code;
using core::shape_code;
using core::shape_kind;
using key = std::tuple<shape_code, std::size_t, code>;
using sums = std::map<long long, code>;
struct tuple_hash {
  template <typename... T> std::size_t operator()(const std::tuple<T...>& k) const noexcept {
    return std::apply([](const auto&... v) { return util::hash_all(v...); }, k);
  }
};

long long add(long long a, long long b) {
  if ((b > 0 && a > std::numeric_limits<long long>::max() - b) ||
      (b < 0 && a < std::numeric_limits<long long>::min() - b))
    throw std::overflow_error("conjunction sum overflow");
  return a + b;
}
long long subtract(long long a, long long b) {
  if ((b > 0 && a < std::numeric_limits<long long>::min() + b) ||
      (b < 0 && a > std::numeric_limits<long long>::max() + b))
    throw std::overflow_error("conjunction residual overflow");
  return a - b;
}
long long multiply(long long a, long long b) {
  const auto lo = std::numeric_limits<long long>::min(), hi = std::numeric_limits<long long>::max();
  if (a > 0 ? (b > 0 ? a > hi / b : b < lo / a) :
      a < 0 && (b > 0 ? a < lo / b : b < 0 && a < hi / b))
    throw std::overflow_error("conjunction product overflow");
  return a * b;
}

/// One constraint's caches: coordinate context and residual are part of keys.
class knapsack {
  core::manager& mgr;
  leaves::int_set_theory& leaf;
  const constraint& c;
  std::unordered_map<key, sums, tuple_hash> partitions;
  std::unordered_map<key, std::pair<long long, long long>, tuple_hash> ranges;
  mutable std::unordered_map<shape_code, std::size_t> widths;
  std::unordered_map<std::tuple<shape_code, std::size_t, code, long long>, code, tuple_hash> filtered;

  auto begin(std::size_t first) const {
    return std::lower_bound(c.terms.begin(), c.terms.end(), first,
      [](const auto& term, std::size_t p) { return term.first < p; });
  }
  std::size_t width(shape_code s) const {
    if (auto it = widths.find(s); it != widths.end()) return it->second;
    const auto& sh = mgr.shapes();
    const auto n = sh.kind(s) == shape_kind::pair ? width(sh.head(s)) + width(sh.tail(s)) :
                   sh.kind(s) == shape_kind::leaf ? std::size_t{1} : std::size_t{0};
    widths.emplace(s, n);
    return n;
  }
  bool touches(shape_code s, std::size_t first) const {
    const auto it = begin(first);
    return it != c.terms.end() && it->first < first + width(s);
  }
  long long coefficient(std::size_t first) const {
    const auto it = begin(first);
    return it != c.terms.end() && it->first == first ? it->second : 0;
  }
  void merge(sums& out, long long sum, code part, shape_code s) {
    auto [it, fresh] = out.try_emplace(sum, part);
    if (!fresh) it->second = mgr.algebra(s).join(it->second, part);
  }
  std::pair<long long, long long> range(shape_code s, std::size_t first, code x) {
    mgr.poll();
    const key k{s, first, x};
    if (auto it = ranges.find(k); it != ranges.end()) return it->second;
    std::pair<long long, long long> r{0, 0};
    const auto& sh = mgr.shapes();
    if (touches(s, first)) {
      if (sh.kind(s) == shape_kind::leaf) {
        const auto vs = leaf.elements(x);
        const auto a = coefficient(first);
        const auto lo = multiply(a, vs.front()), hi = multiply(a, vs.back());
        r = {std::min(lo, hi), std::max(lo, hi)};
      } else if (sh.kind(s) == shape_kind::pair) {
        bool initial = true;
        const auto hs = sh.head(s), ts = sh.tail(s);
        for (const auto& arc : mgr.diagrams().arcs(x)) {
          mgr.poll();
          const auto h = range(hs, first, arc.prime);
          const auto t = range(ts, first + width(hs), arc.sub);
          const auto lo = add(h.first, t.first), hi = add(h.second, t.second);
          if (initial) { r = {lo, hi}; initial = false; }
          else { r.first = std::min(r.first, lo); r.second = std::max(r.second, hi); }
        }
      }
    }
    ranges.emplace(k, r);
    return r;
  }
  const sums& partition(shape_code s, std::size_t first, code x) {
    mgr.poll();
    const key k{s, first, x};
    if (auto it = partitions.find(k); it != partitions.end()) return it->second;
    sums out;
    const auto& sh = mgr.shapes();
    if (!touches(s, first)) out[0] = x;
    else if (sh.kind(s) == shape_kind::leaf) {
      std::map<long long, std::vector<std::int32_t>> groups;
      for (auto v : leaf.elements(x)) { mgr.poll(); groups[multiply(coefficient(first), v)].push_back(v); }
      for (const auto& [sum, vs] : groups) out[sum] = leaf.of(vs);
    } else {
      const auto hs = sh.head(s), ts = sh.tail(s);
      for (const auto& arc : mgr.diagrams().arcs(x)) {
        const auto& h = partition(hs, first, arc.prime);
        const auto& t = partition(ts, first + width(hs), arc.sub);
        for (const auto& [wh, ph] : h) for (const auto& [wt, pt] : t) {
          mgr.poll();
          merge(out, add(wh, wt), mgr.diagrams().rectangle(s, ph, pt), s);
        }
      }
    }
    return partitions.emplace(k, std::move(out)).first->second;
  }
public:
  knapsack(core::manager& m, leaves::int_set_theory& l, const constraint& value)
    : mgr(m), leaf(l), c(value) {}
  code apply(shape_code s, std::size_t first, code x, long long target) {
    mgr.check_interrupt();
    if (x == core::none) return x;
    const auto k = std::make_tuple(s, first, x, target);
    if (auto it = filtered.find(k); it != filtered.end()) return it->second;
    const auto remember = [&](code value) { filtered.emplace(k, value); return value; };
    const auto [lo, hi] = range(s, first, x);
    if (target < lo || (!c.at_most && target > hi)) return remember(core::none);
    if ((c.at_most && hi <= target) || (lo == hi && lo == target)) return remember(x);
    code out = core::none;
    const auto& sh = mgr.shapes();
    if (sh.kind(s) == shape_kind::leaf) {
      std::vector<std::int32_t> vs;
      for (auto v : leaf.elements(x)) {
        mgr.poll();
        const auto sum = multiply(coefficient(first), v);
        if (c.at_most ? sum <= target : sum == target) vs.push_back(v);
      }
      out = leaf.of(vs);
    } else if (sh.kind(s) == shape_kind::pair) {
      const auto hs = sh.head(s), ts = sh.tail(s);
      for (const auto& arc : mgr.diagrams().arcs(x)) {
        const auto& h = partition(hs, first, arc.prime);
        for (const auto& [sum, prime] : h) {
          mgr.poll();
          const auto tail = apply(ts, first + width(hs), arc.sub, subtract(target, sum));
          if (tail != core::none) out = mgr.diagrams().join(out, mgr.diagrams().rectangle(s, prime, tail));
        }
      }
    }
    filtered.emplace(k, out);
    return out;
  }
};

struct schedule {
  shape_code shape;
  std::size_t first;
  std::unique_ptr<schedule> head, tail;
  std::vector<std::size_t> here;
  std::unordered_map<code, code> memo;
};

std::unique_ptr<schedule> plan(core::manager& mgr, shape_code s, std::size_t first,
                              const std::vector<constraint>& cs, const std::vector<std::size_t>& ids) {
  if (ids.empty()) return {};
  mgr.check_interrupt();
  auto out = std::make_unique<schedule>();
  out->shape = s; out->first = first;
  if (mgr.shapes().kind(s) != shape_kind::pair) { out->here = ids; return out; }
  const auto hs = mgr.shapes().head(s), ts = mgr.shapes().tail(s);
  const auto cut = first + mgr.shapes().width(hs);
  std::vector<std::size_t> h, t;
  for (auto id : ids) {
    mgr.poll();
    if (cs[id].terms.back().first < cut) h.push_back(id);
    else if (cs[id].terms.front().first >= cut) t.push_back(id);
    else out->here.push_back(id);
  }
  out->head = plan(mgr, hs, first, cs, h);
  out->tail = plan(mgr, ts, cut, cs, t);
  return out;
}

code run(core::manager& mgr, schedule* p, code x, const std::vector<constraint>& cs,
         std::vector<std::unique_ptr<knapsack>>& evaluators) {
  mgr.check_interrupt();
  if (!p || x == core::none) return x;
  if (auto it = p->memo.find(x); it != p->memo.end()) return it->second;
  code y = x;
  if (p->head || p->tail) {
    std::vector<core::arc> arcs;
    for (const auto& a : mgr.diagrams().arcs(x)) {
      mgr.poll();
      const auto h = run(mgr, p->head.get(), a.prime, cs, evaluators);
      const auto t = run(mgr, p->tail.get(), a.sub, cs, evaluators);
      if (h != core::none && t != core::none) arcs.push_back({h, t});
    }
    y = mgr.diagrams().canonize(p->shape, arcs);
  }
  for (auto id : p->here) {
    if (y == core::none) break;
    y = evaluators[id]->apply(p->shape, p->first, y, cs[id].target);
  }
  p->memo.emplace(x, y);
  return y;
}
}  // namespace

code conjunction(core::manager& mgr, leaves::int_set_theory& leaves,
                 shape_code top, code input, std::span<const constraint> constraints) {
  mgr.check_interrupt();
  std::vector<constraint> cs;
  for (auto c : constraints) {
    mgr.check_interrupt();
    std::sort(c.terms.begin(), c.terms.end());
    std::vector<std::pair<std::size_t, long long>> terms;
    for (auto [p, coefficient] : c.terms) {
      if (p >= mgr.shapes().width(top)) throw std::invalid_argument("conjunction position outside shape");
      if (!terms.empty() && terms.back().first == p) terms.back().second = add(terms.back().second, coefficient);
      else terms.emplace_back(p, coefficient);
    }
    std::erase_if(terms, [](const auto& t) { return t.second == 0; });
    c.terms = std::move(terms);
    if (c.terms.empty()) {
      if (!(c.at_most ? 0 <= c.target : c.target == 0)) return core::none;
    } else cs.push_back(std::move(c));
  }
  const auto less = [](const constraint& a, const constraint& b) {
    return std::tie(a.terms, a.at_most, a.target) < std::tie(b.terms, b.at_most, b.target);
  };
  std::sort(cs.begin(), cs.end(), less);
  cs.erase(std::unique(cs.begin(), cs.end(), [&](const auto& a, const auto& b) { return !less(a,b) && !less(b,a); }), cs.end());
  std::vector<std::size_t> ids;
  std::vector<std::unique_ptr<knapsack>> evaluators;
  for (std::size_t i = 0; i < cs.size(); ++i) {
    ids.push_back(i);
    evaluators.push_back(std::make_unique<knapsack>(mgr, leaves, cs[i]));
  }
  auto p = plan(mgr, top, 0, cs, ids);
  const auto result = run(mgr, p.get(), input, cs, evaluators);
  mgr.check_interrupt();
  return result;
}
}  // namespace hsc::linear
