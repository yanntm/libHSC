# `examples/` — what you can do, demonstrated

Each example is a runnable program that prints something worth looking at and
**checks itself against an independent oracle**, exiting nonzero if they
disagree. They are registered with CTest, so they are also the validation
suite: a milestone is done when its feature works here.

Run one directly to see its output:

    cmake --build build -j && ./build/examples/philosophers

* `philosophers.cc` — Ex1 of the calculus. The dining philosophers state
  space built as *data*: no operations, only the set algebra over
  Construction 3.3. The same word set is built over two shapes, balanced and
  flat, and validated against `trace(Mⁿ)` computed independently by transfer
  matrix.

  What it shows, at n = 512 with 1.9·10²⁴⁴ configurations:

  | shape | nodes |
  |---|---|
  | balanced `V(2k) = (V(k), V(k))` | **8·log₂ n** (64) |
  | flat `V(m) = (⟨A⟩, V(m−1))` | **2n − 2** (2046) |

  The balanced shape does not grow with n because a segment of length k is
  one code wherever it sits in the ring — §2.6, a code is relative to its
  position, so isomorphic positions share it. The flat shape is the classical
  degeneration and pays one level per philosopher. Same words, same counts.

* `hanoi.cc` — Towers of Hanoi: the first example that *moves*. One leaf per
  ring holding its pole; a move is `pos_k = a and pos_j != a,b for j < k`
  then `pos_k := b`. Every condition is about one leaf, so the event is a
  product of local terms — no query, no case — and rings are laid out
  smallest-deepest so an event's support is a suffix of the frontier.

  Reachability is deliberately naive (`X := X ∪ h(X)` to a fixed point).
  Every configuration is reachable, so the answer is the full cube `3^n`,
  stored in `n` nodes on a spine and `log2 n` on a balanced shape. The whole
  cost sits in the iteration count:

  | n | states | iterations | spine nodes | seconds |
  |---|---|---|---|---|
  | 8 | 6 561 | 256 | 9 | 0.011 |
  | 10 | 59 049 | 1 024 | 11 | 0.079 |
  | 12 | 531 441 | 4 096 | 13 | 0.835 |

  `2^n − 1` iterations, because naive iteration rediscovers the whole set at
  every step. **This example exists to be beaten**: it is the baseline
  saturation (M5) has to destroy, and the numbers above are the record of
  what it has to beat.
