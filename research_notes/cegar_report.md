# CEGAR — engineering report

Response to `cegar_spec.md` (v2: the finite instance, HSC-native).
Status: **delivered end to end** — one language, one parser; the loop,
the certificate and the checker are commands of the main; the previous
toolchain's formats and binaries are gone from the tree.
Reproducibility: every number traces to `experiments/cegar/*.tsv`,
produced by `experiments/cegar/sweep.sh` against the committed tree;
models are the committed `examples/cegar/*_model.hsc` files sized on
the command line, so the TSVs are reproducible from the repository
alone. Records of the retired toolchain's campaign live in git
history, superseded by the tables below.

## Delivered

* Core `include/hsc/cegar/` + `src/cegar/` (static lib `hsc_cegar`):
  parser-free PODs and walks; certificates emitted as `.hsc`
  s-expressions.
* Bridge `src/surface_cegar.cc`: the separable fragment of `.hsc`
  (spec §5) → the core's model; per-leaf letters induced by domain
  enumeration, canonically numbered; the property-leaf monitor;
  refusals name the event and the construct.
* Checker `src/surface_certcheck.cc`: own translation unit; shares
  the parser and the bridge, none of the loop.
* Commands in the main (manual §8d): `cegar` (binds the validated bad
  state, or empty on holds), `certificate` (skip-note on violation),
  `certcheck`; `(input FILE)` for the model/driver split; a `cegar`
  command auto-appends the normalizing passes to the rewrite chain.
* Tests `tests/cegar/` (own binary, links the surface library): 18
  cases / 1205 assertions green — bridge, classifier, teacher/learner,
  loop parity on the in-process random fuzzer (all policies, pinned
  regression seeds, bit-exact historical RNG), certificate emitter,
  in-process certcheck mutation battery.
* Examples `examples/cegar/` (CTest-registered): clients,
  clients-bug, ring — model-only files plus drivers; `ring_check.hsc`
  is the full round trip (prove, export, re-check, cross-count).
* Campaign `experiments/cegar/`: T-A, T-C; per-row parity triangle
  (cegar / symbolic / explicit) with in-sweep `certcheck`.

## Findings

**F1 — publish chaos until the first counterexample** (historical,
now absorbed): the learner's closed-but-uncertified table rejects
genuine traces; publishing it produced false `holds` on the first
campaign, caught by the independent checker — the architecture's
claim, demonstrated by its own bug. Spec v2 §3.2 now states the
discipline; the failing seeds stay pinned in `test_loop.cc`.

**F2 — the paper's §6 narrative is search-order dependent** (stands):
BFS returns `g1·g1`, which the *client* also rejects, so both entries
end exact; culprit sets are witness-relative (paper Remark 4.2). The
K-independence that survives is entry-count locality via interning.

**F3 — latent crash in the certified-uniform-families fast path**
(found by this port, fixed forward): a uniform `exists` family whose
body also touches a scalar leaf (a counter, a monitor) drove the
fold's array-cell indexing with the scalar's frontier position —
`std::out_of_range`, uncaught, core dump. Neither existing example
family mixes a scalar into a family event, so it had never fired.
Fixed by the closed-support gate (C5): such a family is
index-invariant in the scalar, not index-periodic — detected on the
representative instance, routed to the enumerated sum with its own
note. Hardened separately: `run_file` now converts any escaped
internal exception into a named diagnostic and exit 3.

**F4 — the first teacher-cost data** (new columns, all tables): the
`ns_search/replay/refine` split makes the paper's economics visible.
Ring, interned: refine is **55–56 µs at every N from 16 to 64** —
flat, the per-behavior budget as wall time. Clients at K=64: refine
is 3.2 s and 99.9 % of the run (the L\* rebuild of the K+2-class
server contract — the paper's §8 overhead pole), while the entire
symbolic + explicit parity pass takes 35 ms. Where the teacher lives
is now a measured column, not a narrative.

## T-A — parity and budget

`ta_parity.tsv`: clients, clients-bug, ring at sizes {2,3,5,8} —
12 rows: 8 holds, 4 violation; **12/12 agree** across the triangle
(cegar vs `reach`+`select` vs `xreach`); **8/8 certificates pass
`certcheck` in-sweep; 0 timeouts; cex ≤ budget everywhere.**
Violations bind their validated bad state (the driver prints it
re-runnably).

## T-C — symmetry scaling

`tc_scaling.tsv`, clients(K)/ring(N), sizes {2,4,8,16,32,64},
interning on/off:

* **ring is the advertised phenomenon**: interned — rounds 3,
  counterexamples 4, refine ~55 µs at *every* N (two entries: station
  0 and the shared rest); interning off — rounds N+1,
  counterexamples 2N, refine growing superlinearly. The verdict and
  |Inv| = 2N never change (the ablation confirms policy-independence
  of T1–T3).
* **clients prices the promise honestly**: the server is the
  property's enforcer, its exact contract rebuilds wholesale — cex =
  K+1, wall 3.3 s at K=64 vs 35 ms for the parity pass on a 65-state
  concrete space. The loop wins when the property is local; this
  family is the "not", kept in the table for exactly that reason.
* |Inv| = concrete states on both families at every size: the final
  abstraction is exactly as coarse as the property allows.

## T-B / T-D — retired pending real corpora

The cone measurement (T-B) and the policy knobs (T-D) were previously
reported on a random corpus; those numbers are withdrawn as evidence
— random models are a correctness fuzzer (now in-process in the test
suite), not a benchmark. The cone needs properties that ignore most
of a model (the CAC08 and DVE corpora below); the knobs need the
refinement-heavy corpus (Theory queue item).

## CAC08 corpus — acquisition (supersedes the reconstruction spec)

`cegar_spec_cac08.md` asked us to *rebuild* the CAC08 family from the
paper's prose, calling it explicitly "a reconstruction, not a
reproduction". That spec is deleted: **the authors' own artifacts were
recovered**, so we translate the real subjects instead of re-deriving
them, and the fidelity contract in that spec no longer binds anything.

* The URL printed in the paper (p. 7:14),
  `laser.cs.umass.edu/~jcobleig/breakingup-examples/`, is **wrong** —
  that path was never served and 404s live and in the archive. The
  subjects sat at `laser.cs.umass.edu/breakingup-examples/`, dead today,
  recovered from the Internet Archive crawl of 2010-06-27.
* Ten tarballs, checksummed, committed under `examples/cac08/upstream/`
  with `fetch_upstream.sh`; the archived index and the CDX listing that
  located it are in `experiments/cegar/cac08_acquisition/`. Two
  toolchains: **LTSA** (FSP `.lts` — component processes *and* the
  safety `property` DFA in one file, which is precisely our leaves +
  monitor) and **FLAVERS** (Ada source, CFGs, QRE). Each subject also
  ships a `.txt` naming the components of the paper's chosen
  decomposition — their best two-way split, i.e. a direct comparator for
  our per-leaf contracts, which we had no way to reconstruct from prose.
* `examples/cac08/curated/` is the working set actually to be
  translated: 1.3 MB, one subfolder per model, byte-identical to
  upstream.

Findings that bear on the evaluation, before any modelling:

1. **The LTSA side supplies 30 of the paper's 32 subjects.** Chiron
   property 8 is absent from both variants at every size; it survives
   only as QRE + Ada on the FLAVERS side. Not a fetch failure — the
   tarball never held it. Subjects 8 and 17 therefore need either a
   hand-translation from QRE or dropping from the table, with the gap
   stated. **Theory question.**
2. **The spec's prediction is already visible in the artifact.** In
   Chiron single the dispatcher runs 38 / 422 / 2 021 / **42 071**
   states at 2 / 3 / 4 / 5 artists while the artists stay at **8–10
   states each** — server superexponential in k, clients flat, exactly
   the asymmetry the evaluation targets. The *multiple* variant is the
   authors' own mitigation: splitting per event kind cuts 42 071 to
   1 958.
3. Sizes are wider than the spec assumed, and uneven between toolchains:
   Gas Station to 200 customers and Smokers to 58 on the FLAVERS side,
   but LTSA stops at 9 and 6. A `.lts` sweep beyond those needs the
   generator the authors used, which is **not** in the artifact.
4. Chiron's FSP is pre-flattened (one clause per state), which is why
   its files are large; the other four systems are parameterized FSP
   with `const`/`range`/`when` guards, i.e. a real (small) FSP front end
   is needed to consume them.
5. Licensing is uneven: only Chiron states terms (UC Regents,
   research/non-profit, notice must appear in all copies — honoured).
   The other four ship no notice at all. See the README.

Not started: the translation. Next engineering step is an FSP reader
emitting **separable-fragment `.hsc`** (model-only files, drivers via
`(input …)`) for the parameterized subset (Gas Station / Peterson /
Relay / Smokers), then Chiron's pre-flattened files. The teacher-cost
split the CAC08 comparison needs is already in the loop's output (F4).

## Deviations from spec

* Spec §5.7's builder oracle (O6) names `mono` vs `xreach`; the sweep
  realizes it as the cegar-vs-symbolic-vs-explicit triangle (three
  implementations rather than two; `mono` parity runs in-process in
  the test suite). Same end, stronger witnesses.
* The bridge's letter induction is exercised by the families and the
  unit fixtures, not fuzzed — reported as the known gap, spec §9.
