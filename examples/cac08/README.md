# `examples/cac08/` — the original CAC08 experimental subjects

The benchmark family of

> J. M. Cobleigh, G. S. Avrunin, L. A. Clarke. *Breaking Up is Hard to
> Do: An Evaluation of Automated Assume-Guarantee Reasoning.* ACM TOSEM
> 17(2), article 7, 2008.

held here as the **authors' own artifacts**, byte-for-byte as published,
not as a re-modelling from the paper's prose. Paper text:
`papers/Cobleigh_Avrunin_Clarke_2008_TOSEM.{pdf,txt}`; citation key
[CAC08] in `research_notes/cegar_citations.md`.

## Provenance

The paper (its p. 7:14) points at

```
http://laser.cs.umass.edu/~jcobleig/breakingup-examples/
```

**That URL is wrong in the paper** — no such path was ever served, and it
404s both live and in the archive. The subjects were actually served from
the lab root:

```
http://laser.cs.umass.edu/breakingup-examples/
```

which is dead today. Recovered from the Internet Archive: the ten
tarballs from the crawl of **2010-06-27**, the index page from
**2011-08-19**. `fetch_upstream.sh` re-fetches them; `upstream/SHA256SUMS`
pins what was fetched on 2026-07-31. The archived index is kept at
`experiments/cegar/cac08_acquisition/index_20110819.html`, the CDX
listing that located it alongside.

The index page describes each tarball as containing "the properties,
source code, and information about the generalized decompositions used
for each subject in our experiments" — all three are present.

## Contents

Two layers, both committed:

* `upstream/` — the ten tarballs (~14 MB) exactly as archived, plus
  `SHA256SUMS`. Kept in git deliberately: the Wayback copy is the only
  one left and may not outlive us. Not meant to be opened.
* `curated/` — **1.3 MB, 187 files: the working set we actually
  translate.** Built from the tarballs by `make_curated.sh`, every file
  byte-identical to upstream (verified with `diff -r`). One subfolder
  per model.

The extracted trees `upstream/ext_LTSA/` and `upstream/ext_FLAVERS/`
(~214 MB) are not committed; regenerate the full corpus with

```sh
sh examples/cac08/fetch_upstream.sh
cd examples/cac08/upstream && for f in *.tar.gz; do
  d=ext_${f##*-}; d=${d%.tar.gz}; mkdir -p "$d"; tar xzf "$f" -C "$d"; done
```

### What `curated/` keeps, and why

Only the **LTSA** side, and of it only `.lts` (FSP: the component
processes plus the safety `property` DFA — our leaves and our monitor)
and `.txt` (the paper's chosen decomposition, i.e. the component set
S1). Dropped: `.sedl` / `.query` glue and the whole FLAVERS toolchain.

Chiron is curated **at its smallest size only** (2 artists, both
variants): its FSP is pre-flattened, so one property file costs ~500 KB
at 4 artists and ~10 MB at 5. Larger Chiron sizes stay in the tarball
and are pulled on demand. `curated/chiron_p08/` holds the two
`p08a.qre` files — the only surviving form of Chiron property 8, which
has no FSP anywhere (see Gaps below).

### Two toolchains, same systems

* **LTSA** — FSP source (`.lts`): one file per (system, size, property),
  each holding the component processes *and* the safety `property`
  automaton in one file. This is the side that maps onto our leaves +
  monitor. Companion `.txt` names the components of the paper's chosen
  decomposition (the set S1); `.sedl`/`.query` appear for some systems.
* **FLAVERS** — the original **Ada** source (`*.adb`, `*.ads`, per-task
  splits), control-flow graphs, and properties as QRE (`*.qre`), under
  `original/` and `decomposed/` variants.

## Inventory

`experiments/cegar/cac08_acquisition/inventory.tsv` (regenerate with
`inventory.py`, which reads the tarballs without extracting).

| system | properties | LTSA sizes | FLAVERS sizes |
|---|---|---|---|
| Chiron single | 8 (p01–p07, p09) | 2–5 artists | 2–6 |
| Chiron multiple | 8 (p01–p07, p09) | 2–5 artists | 2–6 |
| Gas Station | 4 | 2–9 customers | 2–200 |
| Peterson | 1 | 2–3 tasks | 2–3 |
| Relay | 1 | 2–9 tasks | 2–8 |
| Smokers | 8 | 2–6 smokers | 2–58 |

Chiron directories are named `AAEED` — artists, event kinds, dispatchers;
every published size is `??021`, i.e. 2 event kinds, 1 dispatcher.

## Gaps and discrepancies (do not paper over)

1. **Chiron property 8 is missing from the LTSA artifact**, in both the
   single and multiple variants, at every size. The FLAVERS side does
   carry it (`p08a.qre` at all five sizes). So the LTSA corpus supplies
   **30** of the paper's 32 subjects; the two Chiron p08 subjects exist
   only as QRE + Ada. Not a fetch failure — the tarball never held them.
2. Sizes are not uniform across the two toolchains (table above); the
   FLAVERS side is swept far wider (Gas Station to 200, Smokers to 58).
3. Relay carries one `.qre` per size on the FLAVERS side
   (`always_k_between_0`, `no_k_before_1`), while the paper counts Relay
   as a single subject — the property is indexed by the system size.
4. Two stray `.lstar` files (a learned assumption dumped by their L\*
   run) survive in the FLAVERS tree; incidental, not part of the corpus.

## Why the archives are large

104 of the extracted 214 MB are `.lts` files, and almost all of that is
**one component of one system**. The Gas Station / Peterson / Relay /
Smokers FSP is parameterized and compact (Gas Station at 2 customers is
87 lines: `const CUSTOMERS = 2`, ranges, `when` guards). Chiron's FSP is
instead *pre-flattened* — one explicit `STATEn = (...)` clause per state
— and its dispatcher enumerates the registration-list state space:

| artists | Chiron single, dispatcher | Chiron multiple, largest dispatcher |
|---|---|---|
| 2 | 38 | 17 |
| 3 | 422 | 66 |
| 4 | 2 021 | 327 |
| 5 | **42 071** | 1 958 |

At 5 artists the single-dispatcher file is 220 k lines / 10.8 MB, one per
property, and the artists themselves stay at **8–10 states each**. That
asymmetry — server superexponential in k, clients flat — is exactly the
pole the evaluation is aimed at, and it is visible in the upstream
artifact before we model anything. It is also why the *multiple*
dispatcher variant exists: splitting per event kind cuts 42 071 to 1 958.

On the FLAVERS side the bulk is generated Ada (16.5 MB of `.adb`, e.g.
`smokers_58/source/smokers.adb` at 2.4 MB) plus per-task control-flow
graph dumps (`.operator`, `.dispatcher`, `.pump_1`, …).

## Licensing

Checked; the answer is uneven, so state it plainly rather than assume.

* **No licence file ships with any of the ten tarballs.**
* **Chiron** is the only subject carrying stated terms, in its Ada
  source (`chiron/*/0?021/source/disp.{ads,adb}`): a UC Regents notice,
  reproduced verbatim as `LICENSE.chiron`. It grants use, copying,
  modification and distribution "for educational, research any
  non-profit purposes, without fee", on the condition that the notice
  "appear in all copies"; commercial incorporation must be negotiated.
  Our use — academic research, redistribution inside a reproducibility
  artifact — is inside that grant, and we honour the condition by
  copying the notice into every `curated/chiron*/` folder, since the FSP
  files themselves carry it nowhere.
* **Gas Station, Peterson, Relay, Smokers** carry **no notice at all**.
  They are textbook concurrency problems whose models were authored by
  the LASER group and published for public download as the artifact of
  a TOSEM paper. There is no explicit grant to point at; we redistribute
  them on that basis. If the corpus is ever released outside the group,
  ask the authors rather than rely on this paragraph.

## Status

**Acquisition only.** Nothing here has been translated into any format of
ours yet, and no verdict has been reproduced. Translating the FSP `.lts`
side into the CEGAR package's `.cts` model format is a separate step.
