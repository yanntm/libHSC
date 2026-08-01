# `fsp/` — the FSP (LTSA) front end

FSP is the process calculus of the LTSA tool (Magee & Kramer, *Concurrency:
State Models and Java Programs*): labelled processes composed by synchronizing
on shared action labels, plus safety `property` automata. The corpus we target
is the CAC08 artifact (`examples/cac08/curated/`, see its README): one `.lts`
file per (system, size, property), each holding the component processes and
the property DFA — precisely our leaves and our monitor.

The pipeline is MDA-split like the surface and the `dve/` front end:

* **T2M** (`ast.hh`, `src/fsp_parser.cc`) — text → an FSP model:
  constants, ranges, processes as parameterized state definitions with
  label patterns. Names unresolved, expressions as trees; knows nothing
  of `hsc`.
* **M2LTS** (`ground.hh`, `src/fsp_ground.cc`) — each process → its ground
  LTS: states enumerated from the initial reference, label patterns
  expanded to ground labels, `when` guards and target arithmetic
  evaluated, hiding applied. This is FSP's own first semantic step
  (LTSA compiles each process the same way).
* **M2M** (`to_surface.hh`, `src/fsp_to_surface.cc`) — the ground LTSs →
  surface `datum` forms in the **separable fragment** the cegar bridge
  accepts (`hsc/surface/cegar_build.hh`): one leaf per process, one monitor
  leaf for the property, one event per (ground label, target-piece
  tuple). Printing the datums is the `.hsc` serialization (M2T).

The emitted model is **model-only**; the driver (`(input …)`, `(cegar …)`,
`(xreach …)`) is emitted separately, following `examples/cegar/`'s
model/driver split. `tools/fsp2hsc.cc` drives it: parse, ground, transform,
print model and driver.

The subset parsed is exactly what the corpus uses — `const`, `range`,
guarded choice, indexed states and labels, label sets and ranges, alphabet
extension `+{…}`, hiding `\{…}`, `property`, `STOP`, `minimal` (ignored) —
and the front end **refuses loudly** on anything else (`||` composition,
parameterized process headers, relabelling `/{…}`, progress/LTL). See `algorithm.md` for the
grammar, the grounding rules, and the event mapping.
