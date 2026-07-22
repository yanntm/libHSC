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
