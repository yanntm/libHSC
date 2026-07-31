# CEGAR v1 — engineering report

Response to `cegar_spec.md` (v1: finite instance). Status: **M0–M6
delivered**; all oracles wired; campaign data below. Reproducibility:
every number traces to `experiments/cegar/*.tsv`, produced by
`experiments/cegar/sweep.sh` against the committed tree; models are
regenerated from recorded seeds by `hsc-cegar gen`, so the TSVs and the
script are the whole artefact.

## Delivered

* Package `include/hsc/cegar/` + `src/cegar/` (own static lib
  `hsc_cegar`, isolation as specified: `util/` only).
* Tools `tools/cegar/`: `hsc-cegar` (run / mono / gen),
  `hsc-certcheck` (independent, zero shared code).
* Tests `tests/cegar/` (own binary): 18 cases / 1210 assertions, all
  green; oracles O1–O4 in-process, O5 via the sweep + a pinned
  in-sweep mutation battery (G1 drop, action redirect, dead-flip both
  ways: all rejected).
* Campaign `experiments/cegar/`: T-A/T-B, T-C, T-D.

## Findings the spec (or paper) should absorb

**F1 — the initial hypothesis must be published as chaos; anything
else is unsound until certified.** The learner's observation table,
closed at construction, already distinguishes letters by
initial-state firability; publishing that table uncertified rejects
genuine traces and is not an over-approximation. First sweep: 5/200
random models returned false `holds`, and `hsc-certcheck` rejected
all 25 certificates built on uncertified tables (obligation L) — the
independent checker caught the loop's bug, which is precisely the
architecture's claim. Fix: publish chaos until the first
counterexample; a counterexample the raw table already classifies
correctly switches publication without spending budget (index still
strictly grows, so round progress holds). The failing seeds are
pinned as a regression test. *Spec impact:* §3.2's "publication"
paragraph should state the chaos-until-first-counterexample rule
explicitly; it currently permits the unsound reading.

**F2 — the paper's §6 narrative is search-order dependent.** With BFS
in event order, the first witness for `clients(2)` is `g1·g1`, not
the paper's `g1·g2` — and the *client* also rejects `grant·grant`,
so both the server and the (shared) client entry are culprits, and
both end exact. "The clients are never touched again" holds only for
the witness the paper happened to choose. What survives, and what the
data shows: interning makes the refinement count K-independent (all
clients are one entry), and the verdict/certificate are unaffected
(T1–T3 are policy- and order-independent, as claimed). *Paper
impact:* §6 should either pick the witness deliberately ("a search
returning `g1·g2`…") or report both culprits; the phenomenon to
advertise is entry-count, not leaf-count, locality.

## T-A — verdict parity (O1 as data)

`ta_tb_parity.tsv`: families (clients, clients-bug, ring × sizes
{2,3,5,8}) + two rand grids × 100 seeds each. **212 rows: 56 holds,
156 violation; 212/212 agree with `mono`; 56/56 certificates pass
`hsc-certcheck`; 0 timeouts.** Every violation witness refires
concretely (the driver hard-fails otherwise; 0 such failures).

## T-B — budget and the emergent cone (O3 as data)

Over all 212 rows, **counterexamples ≤ budget everywhere** (0
violations of the T3 bound). Cone on the 48 rand `holds` rows
(distinct-leaf entries): **71 chaotic / 6 intermediate / 81 exact** —
about 45 % of entries are never probed by the property at all, and
the loop leaves them at the free rung. Abstract states walked on
those rows total 80 vs 95 for `mono` — at these model sizes the two
walks are comparable; the separation argument is T-C.

## T-C — symmetry scaling

`tc_scaling.tsv`, clients/ring × n ∈ {2..64} × intern {on, off}.

* **ring is the advertised phenomenon**: with interning, rounds = 3
  and counterexamples = 4 at *every* N from 2 to 64 (one shared
  station entry + station 0); with interning off, rounds = N+1 and
  counterexamples = 2N. Refinement effort is size-independent
  exactly when the symmetry is shared; the verdict never changes
  (ablation confirms policy-independence of T1–T3).
* **clients prices the promise honestly** (paper §7): the server is
  the property's enforcer and its exact contract has 2K+2 classes,
  so refinement rebuilds it wholesale — cex = K+1, and walltime grows
  to 4.2 s at K=64 while `mono` stays at milliseconds (the concrete
  product is *small*: 2K+1 states — clients are tightly coupled to
  the server, so there is nothing for abstraction to skip). The loop
  wins when the property is local, ties-with-overhead when it is
  not; this family is the "not".
* |Inv| = mono states on both families at every size: the final
  abstraction is exactly as coarse as the property allows.

## T-D — policy knobs

`td_policies.tsv`, 50 seeds × {all, first, cheapest} × jump-exact
{off, on}: **no separation** — only 7/300 runs refine at all (the
rand corpus is violation-dominated; witnesses are real on round 1),
so all configurations coincide within noise (~2.4 ms/run). The knobs
need a refinement-heavy corpus to be measurable; question returned
to Theory (spec §10 revision).

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
2. **The spec's §5 prediction is already visible in the artifact.** In
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

Not started: any translation into `.cts`. Next engineering step is an
FSP reader for the parameterized subset (Gas Station / Peterson / Relay
/ Smokers) plus the flattened subset (Chiron).

## Deviations from spec

* `run --tsv` column order is verdict-first (spec §10 lists model
  first; the sweep script prepends model columns, net format as
  specified).
* The in-process test for O5 checks the emitter's shape only; the
  mutation battery runs out-of-process in the sweep (spec allows
  either placement).
