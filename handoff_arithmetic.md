# Handoff — cross-level arithmetic

Current state and the next objective. Rewritten in place; no history here.

## The objective

**Arithmetic across a cut.** Criteria and updates that relate coordinates on
different sides of a cut — `a < b + c`, `x := y + z`, `tab[i] := e`,
`tab[tab[x]]`. This is the inseparable fragment (hsc_core5 §7): what does *not*
factor into per-position parts, and therefore what forces `split_equiv`. It is
the one capability that is ours and not libITS's, and it is the whole point.

Everything below the cut is finished and validated: shapes, diagrams, the set
algebra, product terms, saturation, the surface, NUPN import with Louvain
decomposition, and the MCC examinations checked against the official oracle. The
surface already *parses and rejects* crossing forms with a §7 pointer — the seam
is cut, the fragment is scoped, nothing before it is in the way.

## Theory queue (next action first)

1. **The §7 bracket contract** (hsc_core5 Open 1). Pin down: the query/case
   bracket at a cut; how a criterion's *residual* is normalised; and its
   interaction with saturation (Thm 5.2) when the crossing reply does **not**
   factor as `l ∘ f`. This is the blocker for all engineering below. Deliver it
   as a spec precise enough to transcribe: the bracket's inputs/outputs, the
   residual normal form, and the re-saturation rule for a `G` event that is a
   case.
2. **Interchange theory** (Def 2.6): fix that the residual of a cross-cut
   criterion is a code of a declared common sort — LIA is the working one —
   interpreted identically at both ends. State the currying: a criterion
   partially evaluated at the head becomes a residual the tail dispatches on.
3. *(Secondary)* syntactic pre-disjointness beyond selectors (Open 2) — a
   property a theory could declare; not on the critical path.

## Engineering queue (next action first; all gated on theory item 1)

The Python prototype already carries this machinery — `python/hsc/hsc/classify/`
(`query.py`: `split_equiv`, `Partition` = kernel + labelling, the federating
merge, `theta`). It is the re-expression source here, exactly as the legacy
engines were for the separable fragment. Read it before writing C++.

1. **`split_equiv` on the leaf** (`int_set` first): partition a code by the
   residual of a criterion after substituting this coordinate; return the
   realised classes indexed by residual. Enumeration is the honest baseline.
2. **The LIA interchange theory**: expressions, currying to a position,
   residuals as interned codes. New leaf-side module.
3. **`split_equiv` on diagrams** (Prop 7.1 / internalisation): one construct,
   the node instance being the bracket one level in. **Federation is the crux**
   — a `Partition` is a *kernel* (the canonical interned tuple of pieces) plus a
   *labelling* (residual code → kernel), so subqueries with different residuals
   but equal realised partitions share a kernel. Not federating is a correctness
   bug, not just a cost one.
4. **The query/case operation term** at a composite sort, with re-saturation
   fused into the reply (the reply is a morphism; morphisms compose).
5. **Surface**: lift the refusal on crossing guards (`a < b + c`), crossing
   assigns (`x := y + z`), and indirection (`tab[i]`) — compile them to the
   bracket instead of erroring.

**Gate (roadmap R4):** an indirection example `tab[tab[x]]` resolving at the
surface, with an independent oracle (brute-force over a tiny carrier) agreeing.

## Blockers

- Engineering is blocked on theory item 1 (the bracket contract). Start there.
- No representational blocker below the cut — the separable engine is done and
  the crossing seam is already isolated in the surface.

## Explicitly not now (exists, or secondary to the destination)

Benchmark campaign, ITS-tools/libDDD baselines, the R2 memory wall (GC), the
fast FDD leaf (R3), P-invariant bounds for non-safe StateSpace. All real, all
deferred behind cross-level arithmetic.
