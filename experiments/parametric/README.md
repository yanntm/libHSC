# parametric — experiment record

Measured results of the parametric surface, reproducible from tracked
files with the `hsc` CLI. The mechanisms are documented next to their code
(`include/hsc/surface/algorithm.md` §1b, `core/operation.hh` for the
head-folded sum) and at high level in the paper
(`research_notes/hsc_core.md` §8); this file is the data record. The
per-sample oracles live with the samples, `examples/param/README.md`.

## Layout: the same model at three shapes

`examples/param/layout/philo_{spine,balanced,blocked}.hsc` — the identical
N=256 philosophers, only the `shape` differs (blocked: 16 blocks of 16).

| layout | R nodes | total nodes | op terms |
|---|---|---|---|
| spine | 2548 | 7124 | 3579 |
| balanced | 32 | 185 | 1555 |
| blocked 16×16 | 216 | 785 | 1035 |

All three count ≈ 9.785·10⁹⁷ states. The order is representation only —
counts invariant is the standing regression check. Note `count` is a
`double`: exact to ~15 significant digits, an order of magnitude beyond
(ruled: it is an order of magnitude, not a number to decide with).

## Scaling, enumerated route (balanced, N raised in one file)

| N | R nodes | op terms | wall |
|---|---|---|---|
| 256 | 32 | 1555 | ~0 |
| 512 | 36 | 3093 | 0.06 s |
| 1024 | 40 | 6167 | 0.06 s |
| 4096 | 48 | 24603 | 0.30 s |
| 16384 | 56 | 98335 | 3.56 s (expander alone: 0.23 s) |

R nodes grow +4 per doubling — the diagram side is O(log N) as designed.
Op terms are Θ(N): pure site enumeration (the wrapper chains saying
*where*; ~2N−1 distinct wrapper codes per family, the residual operations
themselves shared). Wall time is superlinear on top (×4 N → ×12 wall at
N=16384, net of the expander) — unprofiled, see the handoff queue.

## Scaling, declared route (certified families)

`tests/family_scaling.sh`, balanced philosophers, `HSC_FAMILY=declared`:
op terms **21·k − 8 at N = 2^k** (vs ~6N enumerated), R nodes = 4k; reach
in unmeasurable time at N = 2^20. Wall at that size (~4 s) is entirely the
still-enumerated Θ(N) cell *declarations*, not the events.
`HSC_FAMILY=check` (both routes, same interned code required) is green on
every sample and every N tried.

Certification in practice: philosophers' `take1`/`take2` certify; a
mutex's `tSC` ("all j except i" — dependent partial ranges) does not, and
enumerates. Conservative and correct: `check` proves the declared build
where it applies.

## Reproduction

```sh
./build/tools/hsc -DN=1024 examples/param/layout/philo_balanced.hsc
HSC_FAMILY=declared ./build/tools/hsc -DN=1048576 examples/param/layout/philo_balanced.hsc
tests/family_scaling.sh 20         # one declared-route point, N = 2^20
```

Related records: the BEEM sweep archives under `examples/divine/runs/`,
the its-reach baselines under `tests/logs/`.
