/// \file philosophers.cc
/// \brief Ex1 of the calculus: the philosophers state space, as data.
///
/// `n` philosophers in a ring. Philosopher `i` sits between fork `F_{i-1}`
/// (its right) and `F_i` (its left); each is one leaf carrying its local
/// state, and the only constraint is that a fork is not held twice:
///
///     s in {think, holds-left, holds-right, eating}
///     philosopher i holds F_i     iff  s_i in {holds-left, eating}
///     philosopher i+1 holds F_i   iff  s_{i+1} in {holds-right, eating}
///
/// The set is built by divide and conquer over segments, indexed by the two
/// boundary states — which is exactly the observation that in a balanced
/// shape **every cut is crossed by exactly two forks**. No operations of the
/// calculus are used: this is statics, the set algebra only.
///
/// It is built twice, over two shapes denoting the same words:
///
///   balanced   V(2k) = (V(k), V(k))          — hierarchy
///   flat       V(m)  = (<A>, V(m-1))         — the classical degeneration
///
/// and the point of the example is the node counts side by side.

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "hsc/core/manager.hh"
#include "hsc/leaves/int_set.hh"
#include "hsc/util/timing.hh"

using namespace hsc;

namespace {

/// Local states of one philosopher.
enum state : int { think = 0, holds_left = 1, holds_right = 2, eating = 3 };
constexpr int states = 4;

/// Whether philosopher `i` in state \p a may sit left of one in state \p b:
/// they must not both hold the fork between them.
constexpr bool compatible(int a, int b) {
  const bool left_holds = (a == holds_left || a == eating);
  const bool right_holds = (b == holds_right || b == eating);
  return !(left_holds && right_holds);
}

/// A segment of the ring: its sort, and its configurations indexed by the
/// states of its two boundary philosophers.
struct segment {
  core::shape_code sort = 0;
  std::array<std::array<core::code, states>, states> by_ends{};
};

/// \brief Segments of length 1, 2, 4, ... over the balanced shape.
///
/// Both halves of a segment have the same shape *and the same table*: a
/// segment of length k does not know where in the ring it sits. That is
/// §2.6 — a code is relative to its position, so isomorphic positions share
/// it — and it is why the node count below does not grow with n.
segment balanced(core::manager& mgr, leaves::int_set_theory& theory,
                 core::theory_index index, int n) {
  segment seg;
  if (n == 1) {
    seg.sort = mgr.shapes().leaf(index);
    for (int a = 0; a < states; ++a) {
      seg.by_ends[a][a] = theory.singleton(a);  // others stay `none`
    }
    return seg;
  }

  const segment half = balanced(mgr, theory, index, n / 2);
  seg.sort = mgr.shapes().pair(half.sort, half.sort);

  std::vector<core::arc> rectangles;
  for (int a = 0; a < states; ++a) {
    for (int b = 0; b < states; ++b) {
      rectangles.clear();
      // Split the segment in two: the left half runs from a to some c, the
      // right half from some d to b, and the fork between c and d is the one
      // crossing this cut.
      for (int c = 0; c < states; ++c) {
        for (int d = 0; d < states; ++d) {
          if (!compatible(c, d)) continue;
          rectangles.push_back({half.by_ends[a][c], half.by_ends[d][b]});
        }
      }
      seg.by_ends[a][b] = mgr.diagrams().canonize(seg.sort, rectangles);
    }
  }
  return seg;
}

/// The same set over a right-leaning spine: one philosopher per level.
segment flat(core::manager& mgr, leaves::int_set_theory& theory,
             core::theory_index index, int n) {
  const core::shape_code leaf = mgr.shapes().leaf(index);
  core::diagram_engine& diagrams = mgr.diagrams();

  segment seg;
  seg.sort = mgr.shapes().pair(leaf, mgr.shapes().unit());
  for (int a = 0; a < states; ++a) {
    seg.by_ends[a][a] =
        diagrams.rectangle(seg.sort, theory.singleton(a), diagrams.one());
  }

  for (int length = 2; length <= n; ++length) {
    const segment prev = seg;
    core::support_algebra& tail = mgr.algebra(prev.sort);
    seg.sort = mgr.shapes().pair(leaf, prev.sort);
    for (int a = 0; a < states; ++a) {
      for (int b = 0; b < states; ++b) {
        core::code rest = core::none;
        for (int c = 0; c < states; ++c) {
          if (compatible(a, c)) rest = tail.join(rest, prev.by_ends[c][b]);
        }
        seg.by_ends[a][b] =
            diagrams.rectangle(seg.sort, theory.singleton(a), rest);
      }
    }
  }
  return seg;
}

/// Close the ring: the last philosopher must be compatible with the first.
core::code close_ring(core::manager& mgr, const segment& seg) {
  core::support_algebra& algebra = mgr.algebra(seg.sort);
  core::code ring = core::none;
  for (int a = 0; a < states; ++a) {
    for (int b = 0; b < states; ++b) {
      if (compatible(b, a)) ring = algebra.join(ring, seg.by_ends[a][b]);
    }
  }
  return ring;
}

/// \brief How many configurations the ring really has: `trace(M^n)`.
///
/// The independent oracle. Nothing in it touches libHSC.
long double expected_states(int n) {
  using matrix = std::array<std::array<long double, states>, states>;
  matrix m{};
  for (int a = 0; a < states; ++a) {
    for (int b = 0; b < states; ++b) m[a][b] = compatible(a, b) ? 1.0L : 0.0L;
  }
  matrix power = m;
  for (int step = 1; step < n; ++step) {
    matrix next{};
    for (int a = 0; a < states; ++a) {
      for (int b = 0; b < states; ++b) {
        for (int k = 0; k < states; ++k) next[a][b] += power[a][k] * m[k][b];
      }
    }
    power = next;
  }
  long double trace = 0.0L;
  for (int a = 0; a < states; ++a) trace += power[a][a];
  return trace;
}

bool agrees(double got, long double want) {
  if (want == 0.0L) return got == 0.0;
  return std::fabs((static_cast<long double>(got) - want) / want) < 1e-9L;
}

}  // namespace

int main() {
  std::printf("philosophers: the state space as data\n\n");
  std::printf("%6s %26s %14s %14s %10s\n", "n", "configurations",
              "balanced", "flat", "ok");
  std::printf("%6s %26s %14s %14s %10s\n", "", "", "nodes", "nodes", "");

  bool all_ok = true;
  for (int n = 2; n <= 512; n *= 2) {
    core::manager mgr;
    auto [index, theory] = mgr.import<leaves::int_set_theory>();

    const core::code hier = close_ring(mgr, balanced(mgr, theory, index, n));
    const core::code spine = close_ring(mgr, flat(mgr, theory, index, n));

    core::diagram_engine& diagrams = mgr.diagrams();
    const double count = diagrams.cardinal(hier);
    const long double want = expected_states(n);
    const bool ok = agrees(count, want) &&
                    agrees(diagrams.cardinal(spine), want);
    all_ok = all_ok && ok;

    std::printf("%6d %26.6Le %14zu %14zu %10s\n", n, want,
                diagrams.size(hier), diagrams.size(spine),
                ok ? "yes" : "NO");
  }

  std::printf(
      "\nThe two shapes denote the same words. The balanced one stays flat in\n"
      "n because a segment of length k is one code wherever it sits in the\n"
      "ring; the spine pays one level per philosopher.\n");

  if (!all_ok) {
    std::printf("\nFAILED: a count disagreed with trace(M^n)\n");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
