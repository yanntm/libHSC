# Parametric surface — engineering report

Response to `hsc_parametric.md`. All data reproducible from the named
git-tracked files with `hscrun` (`examples/hsc.cc`); every sample is
self-checking (`expect`, exit code).

## M1 — the expander: done

`include/hsc/surface/expand.hh` + `src/surface_expand.cc` (~600 LOC), wired
into `run_file`; `hscrun --expand FILE` dumps the lowering. `hsc-mcc` was
left unwired on purpose: its forms come from the Petri front end, generated
and binder-free. Gates held:

- Identity on binder-free input: `--expand samples/divine/hsc/at.1.hsc`
  reproduces the file modulo whitespace; the full doctest suite (25926
  assertions) and spot-checked divine samples pass unchanged.
- `--expand` output of every parametric sample is itself a runnable `.hsc`
  with identical results (checked on hanoi: 63 = N·P·P events, the a==b
  degenerates present as text and folded to zero terms by the translator).

Deviation from spec, favorable: empty-`exists` wrap positions emit
`(abort)`, a new base-grammar EVTERM meaning the zero term
(`surface/algorithm.md` grammar updated) — replacing the planned
`(when (!= 0 0))` spelling.

## M2 — samples: done (`samples/param/`, see its README)

| sample | N | count | oracle |
|---|---|---|---|
| hanoi.hsc | 7 rings, 3 poles | 2187 | `= 3^7`; `tools/hanoi.py 7` agrees (independent generator); goal select = 1 |
| philo.hsc | 5 | 82 | saturate ≡ naive; neighbour-clash image empty |
| mutex.hsc | 4 | 82 | saturate ≡ naive; = 3^4 + 1 by hand; mutual-exclusion image empty |
| rotate.hsc | 6 | 6 | = the N cyclic shifts (indexed init + simultaneous rotation) |

The mutex count 82 is verified by hand: 3^4 = 81 free interleavings plus
the single all-frozen critical-section state. Note the GAL original's
bootstrap pseudo-state *aliases* that section state, so its count is not
inflated — the init event's advantage is stating the intended start
directly, not a smaller state space. The GAL references live in
`~/git/ITS-Exercise` (read-only).

## Findings for Theory

1. **`select` vs the doc's grammar.** `algorithm.md` reads
   `QATOM ::= BEXP`, but `do_select` accepts only single-leaf atoms and
   two-leaf comparisons; a quantified disjunction across leaves
   (`(exists (i N) (and …))`) is refused ("expected a symbol"). The
   canonical workaround — a `(when …)` filter applied with `apply` — works
   and is used in philo/mutex; it lands in the §7 case engine. Spec §3 now
   carries the seam; either the doc narrows or `select` routes rich BEXPs
   through the same path.
2. **Suffix-naming of families** (`move_2_0_1`) is user-visible in traces
   and witnesses. Fine for now; worth a thought when witnesses cite events.

## M3 — layout experiment: not started

Next in queue: philosophers at spine / balanced / grain-blocked shapes,
nodes-peak-time per layout.
