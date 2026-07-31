# CEGAR — engineering report

Response to `cegar_spec.md` (v2: the finite instance, HSC-native).
Status: **delivered end to end** — one language, one parser; the loop,
the certificate and the checker are commands of the main.
Reproducibility: every number traces to `experiments/cegar/*.tsv`,
produced by `experiments/cegar/sweep.sh` against the committed tree;
models are the committed `examples/cegar/*_model.hsc` files sized on
the command line, so the TSVs are reproducible from the repository
alone.

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

**F1 — publish chaos until the first counterexample**: the learner's
closed-but-uncertified table rejects genuine traces; publishing it
yields false `holds`, and it is the independent checker that catches
it — the architecture's claim, demonstrated by its own bug. The
discipline is spec §3.2; the seeds that exposed it are pinned in
`test_loop.cc`.

**F2 — the paper's §6 narrative is search-order dependent** (stands):
BFS returns `g1·g1`, which the *client* also rejects, so both entries
end exact; culprit sets are witness-relative (paper Remark 4.2). The
K-independence that survives is entry-count locality via interning.

**F3 — teacher-cost data** (columns in every table): the
`ns_search/replay/refine` split makes the paper's economics visible.
Ring, interned: refine is **55–56 µs at every N from 16 to 64** —
flat, the per-behavior budget as wall time. Clients at K=64: refine
is 3.2 s and 99.9 % of the run (the L\* rebuild of the K+2-class
server contract — the paper's §8 overhead pole), while the entire
symbolic + explicit parity pass takes 35 ms. Where the teacher lives
is a measured column.

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

## T-B / T-D — awaiting their corpora

The cone measurement (T-B) needs properties that ignore most of a
model (the CAC08 and DVE corpora below); the policy knobs (T-D) need
the refinement-heavy corpus (Theory queue item). The families cannot
show either — every leaf is either property-coupled or symmetric —
so neither table carries rows yet.

## CAC08 corpus — the authors' artifacts

**The original artifacts of the study are in hand** — the subjects
are translated from source, not derived from the paper's prose.

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

### The translation — delivered

`tools/fsp2hsc` (package `include/hsc/fsp/`: parse → ground each
process to its LTS → compose into the separable fragment; docs there)
translates **all 91 LTSA subject files** — every (system, size,
property) in `curated/`, Chiron at 2 artists — into model/driver
`.hsc` pairs under `examples/cac08/hsc/`, regenerated by
`translate_all.sh`. One leaf per process, the property DFA totalized
into a `mon` leaf (error = top value), one event per (ground label,
target-piece tuple); drivers run `(cegar v (== mon E))` + `(xreach x)`.
Expectations are **not yet pinned** — that is the campaign step.

Verified on the smallest instance of every family (all runs < 15 s
unless noted): every property **holds**, consistent with CAC08 (their
subjects are all true properties); cegar `inv` and `xreach` agree
where the abstraction is exact (relay 18/18, peterson2 52/52,
smokers2 52/52). The grounded Chiron dispatcher has exactly the **38
states** the artifact's own table gives at 2 artists — a free oracle
hit on the front end.

Findings:

1. **`gas_c002_correct_change` blows the 15 s cap inside the cegar
   loop** while `xreach` finishes instantly (165 concrete states) and
   the three sibling properties of the same model verify in ~2 s. The
   property is 2 states; the monitor watches `operator` labels (the
   121-state leaf). This is teacher-economics data (policy/refinement
   behavior), not a translation defect — queued for analysis, not
   tuned away.
2. LTSA semantics the corpus depends on, now in the front end's
   contract: a target whose index leaves its declared range is
   **ERROR** (`never_fill_table` errs exactly that way); `STOP` is a
   state with no branches (Chiron p09); alphabet-extension labels a
   process never fires are globally blocked (the "dead labels" the
   tool reports — e.g. 30 of Peterson's 52).
3. The unsized template files (`peterson.lts`: `||` composition,
   parameterized process headers) are refused loudly as designed; the
   sized per-subject files are the corpus.
4. Upstream supplies fewer properties at the largest Smokers sizes
   (smokers5: 6, smokers6: 3 of the 8) — the 91 count is the
   artifact's own coverage, not a translation loss.

The teacher-cost split the CAC08 comparison needs is in the loop's
output (F3). Next: sweep the 91 drivers into a TSV (15 s cap, a
timeout is a row), pin the `expect` lines from verified rows, then
pull larger Chiron sizes from the tarballs as the tables demand.

## Deviations from spec

* Spec §5.7's builder oracle (O6) names `mono` vs `xreach`; the sweep
  realizes it as the cegar-vs-symbolic-vs-explicit triangle (three
  implementations rather than two; `mono` parity runs in-process in
  the test suite). Same end, stronger witnesses.
* The bridge's letter induction is exercised by the families and the
  unit fixtures, not fuzzed — reported as the known gap, spec §9.
