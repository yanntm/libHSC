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

result solve(int n, bool balanced) {
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

  // Every event, summed into one term.
  core::code all_moves = core::op_table::id;
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

        const core::code move = core::product(ops, mgr.shapes(), sort, by_leaf);
        all_moves = ops.sum(all_moves, move);
      }
    }
  }

  // All rings start on pole 0.
  const std::vector<std::int32_t> start(static_cast<std::size_t>(n), 0);
  std::size_t next = 0;
  core::code reachable = point(mgr, theory, sort, start, next);

  util::stopwatch sw;
  std::size_t iterations = 0;
  for (;;) {
    const core::code grown =
        diagrams.join(reachable, diagrams.apply_local(all_moves, reachable));
    ++iterations;
    if (grown == reachable) break;  // equality is an integer comparison
    reachable = grown;
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
  std::printf("towers of hanoi: reachability by naive iteration\n\n");
  std::printf("%4s %14s %8s %10s %8s %10s %8s\n", "n", "states", "iters",
              "shape", "nodes", "op terms", "sec");

  bool all_ok = true;
  for (int n = 3; n <= 12; ++n) {
    const double expected = std::pow(3.0, n);

    for (const bool balanced : {false, true}) {
      // The balanced shape needs a power of two; skip the sizes it cannot take.
      if (balanced && (n & (n - 1)) != 0) continue;

      const result r = solve(n, balanced);
      const bool ok = std::fabs(r.count - expected) < 0.5;
      all_ok = all_ok && ok;

      std::printf("%4d %14.0f %8zu %10s %8zu %10zu %8.3f%s\n", n, r.count,
                  r.iterations, balanced ? "balanced" : "spine", r.nodes,
                  r.op_terms, r.seconds, ok ? "" : "  MISMATCH");
    }
  }

  std::printf(
      "\nEvery Hanoi configuration is reachable, so the answer is the full\n"
      "cube 3^n — which canonization stores in n nodes on a spine and\n"
      "log2(n) on a balanced shape. The cost is entirely in the %s column:\n"
      "2^n - 1 iterations, because naive iteration rediscovers the whole set\n"
      "at every step. That number is what saturation is for.\n",
      "iters");

  if (!all_ok) {
    std::printf("\nFAILED: a state count disagreed with 3^n\n");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
