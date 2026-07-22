# hsc/ops — homomorphisms

Read `algorithm.md` in this folder first; it also states the package's two
open questions, both of which are theory questions before they are coding
ones.

| module | responsibility |
|---|---|
| `hom.py` | base class, the (term, data) application cache, `support`/`rerooted` |
| `local.py` | position-addressed: `Filter`, `Write`, parallel `Assign` |
| `combinators.py` | `Identity`, `Compose`, `Sum`, `Star` |
| `branch.py` | classifier-driven: `Guard`, `Put`, `Case` |

A local hom at a cut is *the same hom one level down*: it recurses by
re-invoking itself, so the application cache is hit at every level. A hom
that recursed through a plain helper would walk the diagram's tree unfolding
rather than its DAG and lose the sharing.

Open, and documented in `algorithm.md`: the term normal form (a confluent
rewriting system over commutativity and relative support, not a canonical
guarded word) and the schedule (saturation; `support()` is a static
over-approximation and `Star` is plain BFS).
