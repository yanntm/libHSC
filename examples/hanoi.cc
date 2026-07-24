/// \file hanoi.cc
/// \brief Towers of Hanoi: the first example that fires transitions.
///
/// One leaf per ring, holding the pole it sits on. Ring 0 is the smallest.
/// Moving ring k from pole a to pole b requires that no smaller ring sits on
/// either pole:
///
///     :when  pos_k = a  and  pos_j != a, b   for every j < k
///     :do    pos_k := b
///
/// Every condition is about **one** leaf, so the event is a product of local
/// terms — no query, no case. Rings are laid out with the smallest deepest,
/// so an event's support is a suffix of the frontier.
///
/// Reachability here is deliberately naive: `X := X ∪ h(X)` until nothing
/// moves. Hanoi's shortest solution is 2^n − 1 moves, so that loop needs
/// 2^n − 1 iterations. The iteration count printed below is the number
/// saturation has to destroy (M5); this example exists to be beaten.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <vector>

#include "hsc/core/manager.hh"
#include "hsc/core/operation.hh"
#include "hsc/leaves/int_set.hh"
#include "hsc/util/timing.hh"

using namespace hsc;

namespace {

constexpr int poles = 3;

/// The shape: `n` leaves, either as a right spine or as a balanced tree.
core::shape_code spine_shape(core::shape_table& shapes, core::shape_code leaf,
                             int n) {
  core::shape_code sort = shapes.unit();
  for (int i = 0; i < n; ++i) sort = shapes.pair(leaf, sort);
  return sort;
}

core::shape_code balanced_shape(core::shape_table& shapes,
                                core::shape_code leaf, int n) {
  if (n == 1) return leaf;
  const core::shape_code half = balanced_shape(shapes, leaf, n / 2);
  return shapes.pair(half, half);
}

/// The diagram of a single word: one value per leaf, left to right.
core::code point(core::manager& mgr, leaves::int_set_theory& theory,
                 core::shape_code sort, std::span<const std::int32_t> values,
                 std::size_t& next) {
  core::diagram_engine& diagrams = mgr.diagrams();
  switch (mgr.shapes().kind(sort)) {
    case core::shape_kind::unit:
      return diagrams.one();
    case core::shape_kind::leaf:
      return theory.singleton(values[next++]);
    case core::shape_kind::pair: {
      const core::code head =
          point(mgr, theory, mgr.shapes().head(sort), values, next);
      const core::code tail =
          point(mgr, theory, mgr.shapes().tail(sort), values, next);
      return diagrams.rectangle(sort, head, tail);
    }
  }
  return core::none;
}

struct result {
  core::code states = core::none;
  std::size_t iterations = 0;
  std::size_t nodes = 0;
  std::size_t op_terms = 0;
  double seconds = 0.0;
  double count = 0.0;
};

enum class strategy { naive, saturated };

result solve(int n, bool balanced, strategy how) {
  core::manager mgr;
  auto [index, theory] = mgr.import<leaves::int_set_theory>();
  const core::shape_code leaf = mgr.shapes().leaf(index);
  const core::shape_code sort = balanced
                                    ? balanced_shape(mgr.shapes(), leaf, n)
                                    : spine_shape(mgr.shapes(), leaf, n);

  core::diagram_engine& diagrams = mgr.diagrams();
  core::op_table& ops = mgr.operations();

  // Smallest ring deepest: ring k sits at leaf n-1-k.
  const auto leaf_of_ring = [n](int k) {
    return static_cast<std::size_t>(n - 1 - k);
  };

  // Every event, as its own term.
  std::vector<core::code> events;
  std::vector<core::code> by_leaf(static_cast<std::size_t>(n));
  for (int k = 0; k < n; ++k) {
    for (int a = 0; a < poles; ++a) {
      for (int b = 0; b < poles; ++b) {
        if (a == b) continue;
        const int other = poles - a - b;  // the one pole that is neither

        std::ranges::fill(by_leaf, core::op_table::id);
        // move ring k: it must be on a, and it lands on b
        by_leaf[leaf_of_ring(k)] = theory.assign(theory.singleton(a), b);
        // no smaller ring may sit on a or b, so each must be on `other`
        for (int j = 0; j < k; ++j) {
          by_leaf[leaf_of_ring(j)] = theory.keep(theory.singleton(other));
        }

        events.push_back(core::product(ops, mgr.shapes(), sort, by_leaf));
      }
    }
  }

  // All rings start on pole 0.
  const std::vector<std::int32_t> start(static_cast<std::size_t>(n), 0);
  std::size_t next = 0;
  core::code reachable = point(mgr, theory, sort, start, next);

  util::stopwatch sw;
  std::size_t iterations = 0;
  if (how == strategy::saturated) {
    // One term, one application. Every closure sits inside it, so the memo
    // keys on saturated nodes rather than on rounds.
    const core::code closure = core::saturate(mgr, sort, events);
    reachable = diagrams.apply_local(closure, reachable);
    iterations = 1;
  } else {
    const core::code all_moves = ops.sum(events);
    for (;;) {
      const core::code grown =
          diagrams.join(reachable, diagrams.apply_local(all_moves, reachable));
      ++iterations;
      if (grown == reachable) break;  // equality is an integer comparison
      reachable = grown;
    }
  }

  result r;
  r.seconds = sw.seconds();
  r.states = reachable;
  r.iterations = iterations;
  r.nodes = diagrams.size(reachable);
  r.op_terms = ops.size();
  r.count = diagrams.cardinal(reachable);
  return r;
}

}  // namespace

int main() {
  bool all_ok = true;
  const auto check = [&all_ok](const result& r, int n) {
    const bool ok = std::fabs(r.count - std::pow(3.0, n)) < 0.5;
    all_ok = all_ok && ok;
    return ok;
  };

  std::printf("towers of hanoi: naive iteration against saturation\n\n");
  std::printf("%4s %16s %10s %10s %10s %9s\n", "n", "states", "rounds",
              "naive s", "sat s", "speedup");

  for (int n = 3; n <= 14; ++n) {
    const result naive = solve(n, false, strategy::naive);
    const result sat = solve(n, false, strategy::saturated);
    const bool ok = check(naive, n) && check(sat, n);
    const double speedup =
        sat.seconds > 0.0 ? naive.seconds / sat.seconds : 0.0;
    std::printf("%4d %16.0f %10zu %10.3f %10.4f %8.0fx%s\n", n, sat.count,
                naive.iterations, naive.seconds, sat.seconds, speedup,
                ok ? "" : "  MISMATCH");
  }

  std::printf("\nsaturation alone, past where naive iteration can follow:\n\n");
  std::printf("%4s %16s %10s %8s %10s %9s\n", "n", "states", "shape", "nodes",
              "op terms", "sec");
  for (int n = 16; n <= 24; n += 4) {
    for (const bool balanced : {false, true}) {
      if (balanced && (n & (n - 1)) != 0) continue;
      const result r = solve(n, balanced, strategy::saturated);
      const bool ok = check(r, n);
      std::printf("%4d %16.0f %10s %8zu %10zu %9.3f%s\n", n, r.count,
                  balanced ? "balanced" : "spine", r.nodes, r.op_terms,
                  r.seconds, ok ? "" : "  MISMATCH");
    }
  }

  std::printf(
      "\nNaive iteration needs 2^n - 1 rounds and rebuilds a fresh object in\n"
      "each, so every round misses the memo. Saturation puts the closures\n"
      "inside the operator term, so the memo keys on saturated nodes: one\n"
      "application, and a node is saturated once.\n");

  if (!all_ok) {
    std::printf("\nFAILED: a state count disagreed with 3^n\n");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
