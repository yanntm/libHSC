/// \file surface_weighted_count.cc
/// \brief Counting a diagram when a leaf stands for several places.
///
/// A structural reduction may fuse a *free component* of a net — a set of K
/// places over which tokens travel freely — into one place holding the
/// component's total. Every distribution of that total over the K places is
/// reachable, so a marking of v in the fused place represents
/// C(v+K-1, K-1) markings of the original net.
///
/// Counting is then the ordinary recursion with a single substitution. Where
/// the plain count contributes the number of values a leaf arc holds, this
/// one contributes the sum of those binomials over the same values; the
/// recursion over sorts, the arcs and the products are unchanged, and
/// nothing below a leaf is affected.
///
/// It is a separate algorithm with separate caches on purpose: no ordinary
/// state count pays for it, and a run that declares no weight never enters
/// this file.

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "surface_translator.hh"

namespace hsc::surface {

/// `(leaf-weight NAME K)`: the leaf NAME stands for K places of a free
/// component. K = 1 is the default and is not recorded.
void translator::do_leaf_weight(const datum& form) {
  const std::string& name = sym(arg(form, 1, "leaf name"));
  const std::int32_t k = as_int(arg(form, 2, "number of places"));
  if (k < 1) fail(form, "a leaf stands for at least one place");
  if (!leaves_.contains(name)) fail(form, "unknown leaf '" + name + "'");
  if (k == 1) {
    weights_.erase(name);
  } else {
    weights_[name] = k;
  }
}

namespace {

/// The walk, with the caches it needs and nothing else.
class weigher {
 public:
  weigher(core::manager& mgr, const leaves::int_set_theory& theory,
          std::vector<long long> per_position)
      : mgr_(mgr), theory_(theory), weight_(std::move(per_position)) {}

  /// The weighted count of \p c, whose sort is \p sort and whose leftmost
  /// leaf sits at frontier position \p base.
  mpz_class count(code c, core::shape_code sort, std::size_t base) {
    switch (mgr_.shapes().kind(sort)) {
      case core::shape_kind::unit:
        return 1;  // the empty word
      case core::shape_kind::leaf: {
        const long long k = base < weight_.size() ? weight_[base] : 1;
        mpz_class sum = 0;
        for (const std::int32_t v : theory_.elements(c)) sum += binomial(v, k);
        return sum;
      }
      case core::shape_kind::pair: {
        if (c == core::none) return 0;
        // the same sort can sit at several places in the shape, so a node's
        // count depends on where its leaves are: the memo is keyed by both
        const auto key = std::make_pair(c, base);
        const auto hit = memo_.find(key);
        if (hit != memo_.end()) return hit->second;
        const core::shape_code head = mgr_.shapes().head(sort);
        const core::shape_code tail = mgr_.shapes().tail(sort);
        const std::size_t split = base + mgr_.shapes().width(head);
        mpz_class total = 0;
        for (const core::arc& a : mgr_.diagrams().arcs(c)) {
          total += count(a.prime, head, base) * count(a.sub, tail, split);
        }
        memo_.emplace(key, total);
        return total;
      }
    }
    return 0;
  }

 private:
  /// How many markings of a free component of \p k places hold \p tokens:
  /// the compositions of `tokens` into `k` non-negative parts.
  const mpz_class& binomial(std::int32_t tokens, long long k) {
    const auto key = std::make_pair(tokens, k);
    const auto hit = binomials_.find(key);
    if (hit != binomials_.end()) return hit->second;
    mpz_class value;
    mpz_bin_uiui(value.get_mpz_t(), static_cast<unsigned long>(tokens + k - 1),
                 static_cast<unsigned long>(k - 1));
    return binomials_.emplace(key, std::move(value)).first->second;
  }

  core::manager& mgr_;
  const leaves::int_set_theory& theory_;
  std::vector<long long> weight_;  ///< by frontier position, 1 by default
  std::map<std::pair<code, std::size_t>, mpz_class> memo_;
  std::map<std::pair<std::int32_t, long long>, mpz_class> binomials_;
};

}  // namespace

mpz_class translator::weighted_count(code c) {
  // the declarations are by name; the walk needs them by frontier position
  std::vector<long long> per_position(order_.size(), 1);
  for (const auto& [name, k] : weights_) {
    const auto it = leaves_.find(name);
    if (it == leaves_.end() || !it->second.placed) continue;
    per_position[it->second.index] = k;
  }
  weigher w(mgr_, *theory_, std::move(per_position));
  return w.count(c, top_, 0);
}

}  // namespace hsc::surface
