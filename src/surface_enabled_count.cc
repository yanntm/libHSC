/// Exact counting of original enablings in free-distribution families.
/// See tools/enabling/README.md and petri/reduction/counting/algorithm.md.
#include <map>
#include <tuple>
#include <unordered_map>
#include "hsc/util/hash.hh"
#include "surface_translator.hh"

namespace hsc::surface {
namespace {
struct enabled_hash {
  template<class... T> std::size_t operator()(const std::tuple<T...>& key) const {
    return std::apply([](const auto&... v) { return util::hash_all(v...); }, key);
  }
};
class enabled_counter {
  core::manager& mgr;
  const leaves::int_set_theory& theory;
  const std::map<std::size_t, long long>& weights;
  const std::map<std::size_t, long long>& demands;
  std::unordered_map<std::tuple<core::shape_code, std::size_t, code>, mpz_class, enabled_hash> memo;
  std::map<std::pair<long long, long long>, mpz_class> binomials;
  std::unordered_map<core::shape_code, std::size_t> widths;
  std::size_t width(core::shape_code s) {
    mgr.poll();
    if (auto it = widths.find(s); it != widths.end()) return it->second;
    const auto& sh = mgr.shapes();
    const auto n = sh.kind(s) == core::shape_kind::pair ? width(sh.head(s)) + width(sh.tail(s)) :
                   sh.kind(s) == core::shape_kind::leaf ? std::size_t{1} : std::size_t{0};
    widths.emplace(s, n);
    return n;
  }
  mpz_class ways(long long tokens, long long k) {
    if (tokens < 0) return 0;
    const auto key = std::make_pair(tokens, k);
    if (auto it = binomials.find(key); it != binomials.end()) return it->second;
    mpz_class v = 1;
    // Multiplicative binomial with polling; no overflowing fixed-width top.
    const long long steps = std::min(tokens, k - 1);
    const long long base = std::max(tokens, k - 1);
    for (long long i = 1; i <= steps; ++i) {
      mgr.poll();
      v *= mpz_class(std::to_string(base)) + mpz_class(std::to_string(i));
      v /= mpz_class(std::to_string(i));
    }
    binomials.emplace(key, v);
    return v;
  }
public:
  enabled_counter(core::manager& m, const leaves::int_set_theory& t,
                  const std::map<std::size_t, long long>& w,
                  const std::map<std::size_t, long long>& d)
      : mgr(m), theory(t), weights(w), demands(d) {}
  mpz_class count(code x, core::shape_code s, std::size_t first) {
    mgr.check_interrupt();
    if (x == core::none) return 0;
    const auto key = std::make_tuple(s, first, x);
    if (auto it = memo.find(key); it != memo.end()) return it->second;
    const auto& sh = mgr.shapes();
    mpz_class value = 0;
    if (sh.kind(s) == core::shape_kind::unit) value = 1;
    else if (sh.kind(s) == core::shape_kind::leaf) {
      const auto wi = weights.find(first), di = demands.find(first);
      const auto k = wi == weights.end() ? 1 : wi->second;
      const auto demand = di == demands.end() ? 0 : di->second;
      for (const auto v : theory.elements(x)) {
        mgr.poll();
        if (v >= demand) value += ways(v - demand, k);
      }
    } else {
      const auto h = sh.head(s), t = sh.tail(s);
      const auto split = first + width(h);
      for (const auto& a : mgr.diagrams().arcs(x)) {
        mgr.poll();
        value += count(a.prime, h, first) * count(a.sub, t, split);
      }
    }
    memo.emplace(key, value);
    return value;
  }
};
}

/// `(count-enabled RESULT (PLACE DEMAND) ...)`: pure weighted counting query.
void translator::do_count_enabled(const datum& form) {
  const auto name = sym(arg(form, 1, "result name"));
  const auto input = named(form.items()[1]);
  std::map<std::size_t, long long> weights, demands;
  for (const auto& [leaf, k] : weights_) {
    mgr_.poll();
    const auto p = position(leaf);
    if (p) weights.emplace(*p, k);
  }
  for (std::size_t i = 2; i < form.items().size(); ++i) {
    mgr_.poll();
    const auto& term = form.items()[i];
    if (!term.is_list() || term.items().size() != 2) fail(term, "expected (PLACE DEMAND)");
    const auto p = position(sym(term.items()[0]));
    if (!p) fail(term, "unknown enabling leaf");
    const auto d = std::stoll(term.items()[1].text());
    if (d < 0 || demands.contains(*p)) fail(term, "negative or duplicate enabling demand");
    demands.emplace(*p, d);
  }
  enabled_counter counter(mgr_, *theory_, weights, demands);
  const auto value = counter.count(input, top_, 0);
  out_ << name << " enabled " << value << '\n';
}
}
