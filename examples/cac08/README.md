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

`upstream/` holds the ten tarballs (~14 MB) and their checksums. The
extracted trees `upstream/ext_LTSA/` and `upstream/ext_FLAVERS/`
(~214 MB) are **not committed** — regenerate with

```sh
sh examples/cac08/fetch_upstream.sh
cd examples/cac08/upstream && for f in *.tar.gz; do
  d=ext_${f##*-}; d=${d%.tar.gz}; mkdir -p "$d"; tar xzf "$f" -C "$d"; done
```

Two toolchains, same systems:

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

## Status

**Acquisition only.** Nothing here has been translated into any format of
ours yet, and no verdict has been reproduced. Translating the FSP `.lts`
side into the CEGAR package's `.cts` model format is a separate step.
