/// \file force.cc
/// \brief FORCE: iterate spring relaxation over the constraints, keep the
/// cheapest order seen. Algorithm and provenance: `hsc/order/README.md`.

#include "hsc/order/force.hh"

#include <algorithm>
#include <numeric>

namespace hsc::order {

namespace {

/// Summed cost of the current ranks: clique span + violated precedences.
double total_cost(std::span<const clique> cliques,
                  std::span<const precedence> precedences,
                  const std::vector<float>& rank) {
  double cost = 0;
  for (const clique& c : cliques) {
    if (c.vars.size() < 2) continue;
    float lo = rank[c.vars.front()];
    float hi = lo;
    for (const std::uint32_t v : c.vars) {
      lo = std::min(lo, rank[v]);
      hi = std::max(hi, rank[v]);
    }
    cost += static_cast<double>(c.weight) * (hi - lo);
  }
  for (const precedence& p : precedences) {
    const float d = rank[p.before] - rank[p.after];
    if (d > 0) cost += static_cast<double>(p.weight) * d;
  }
  return cost;
}

}  // namespace

std::vector<std::uint32_t> force(std::size_t nvars,
                                 std::span<const clique> cliques,
                                 std::span<const precedence> precedences,
                                 std::size_t max_iters) {
  std::vector<std::uint32_t> best(nvars);
  std::iota(best.begin(), best.end(), 0u);
  if (nvars < 2 || (cliques.empty() && precedences.empty())) return best;
  if (max_iters == 0) max_iters = std::min<std::size_t>(nvars, 256);

  // rank[v]: v's current position; the working state of the relaxation.
  std::vector<float> rank(nvars);
  std::iota(rank.begin(), rank.end(), 0.0f);
  double best_cost = total_cost(cliques, precedences, rank);

  std::vector<float> pull(nvars);    // Σ weight · cog over constraints
  std::vector<float> total(nvars);   // Σ weight
  std::vector<std::uint32_t> perm(nvars);
  for (std::size_t iter = 0; iter < max_iters; ++iter) {
    std::ranges::fill(pull, 0.0f);
    std::ranges::fill(total, 0.0f);
    for (const clique& c : cliques) {
      if (c.vars.empty()) continue;
      float cog = 0;
      for (const std::uint32_t v : c.vars) cog += rank[v];
      cog /= static_cast<float>(c.vars.size());
      for (const std::uint32_t v : c.vars) {
        pull[v] += c.weight * cog;
        total[v] += c.weight;
      }
    }
    for (const precedence& p : precedences) {
      // The satisfied direction is free; the pull sends `before` to the
      // smaller of the two ranks and `after` to the larger (the libITS
      // QueryEdge asymmetric spring).
      const float lo = std::min(rank[p.before], rank[p.after]);
      const float hi = std::max(rank[p.before], rank[p.after]);
      pull[p.before] += p.weight * lo;
      pull[p.after] += p.weight * hi;
      total[p.before] += p.weight;
      total[p.after] += p.weight;
    }
    for (std::size_t v = 0; v < nvars; ++v) {
      if (total[v] > 0) rank[v] = pull[v] / total[v];
    }

    // Re-rank: sort by the new float positions, previous rank breaking
    // ties, so the result is a permutation again.
    std::iota(perm.begin(), perm.end(), 0u);
    std::ranges::stable_sort(
        perm, [&](std::uint32_t a, std::uint32_t b) { return rank[a] < rank[b]; });
    for (std::size_t i = 0; i < nvars; ++i) {
      rank[perm[i]] = static_cast<float>(i);
    }

    const double cost = total_cost(cliques, precedences, rank);
    if (cost < best_cost) {
      best_cost = cost;
      best = perm;
    }
  }
  return best;
}

}  // namespace hsc::order
