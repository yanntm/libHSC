# libHSC

**libHSC** is a C++23 library and toolset for the **Hierarchical Shape
Calculus** (HSC): symbolic representation and transformation of very large
structured state spaces, with hierarchy as the primitive. It is the
concepts-first reboot of [libDDD](https://github.com/lip6/libDDD) /
[libITS](https://github.com/lip6/libITS): hierarchical decision diagrams,
saturation, and partition queries with discovered alphabets form the base
of the calculus, while flat decision diagrams and plain homomorphisms are
recognized, fast degenerations.

## Build

```sh
cmake -S . -B build && cmake --build build -j
ctest --test-dir build          # the full suite, ~25 s
```

Needs a C++23 compiler and network on first configure (google sparsehash,
doctest and CLI11 are fetched into `deps/`); the PNML importers additionally
need expat and are skipped without it.

## What you can do

**Run a model.** A `.hsc` file declares integer leaves, a tree *shape* over
them, guarded events, and the questions to ask; the `hsc` CLI runs it end
to end. Every `(expect …)` is checked, so a model file is a self-checking
test — exit code 0 means all expectations held.

```sh
./build/tools/hsc examples/models/ring4.hsc
./build/tools/hsc -DN=10 examples/param/philo.hsc   # parametric, N overridden
./build/tools/hsc --expand examples/param/hanoi.hsc # print the flat form
```

**Write a model.** The language — syntax, semantics, the parametric layer
(`param` / `forall` / `exists`), queries — is documented in
[`doc/hsc_manual.md`](doc/hsc_manual.md). Hand-written starters live in
`examples/models/` and `examples/param/`.

**Import a model.** Front ends translate existing formats to `.hsc` text:

```sh
./build/tools/dve2hsc model.dve > model.hsc     # DVE (BEEM benchmark)
./build/tools/nupn2hsc net.pnml > net.hsc       # PNML/NUPN Petri net
./build/tools/hsc-mcc net.pnml -mcc StateSpace  # MCC examinations, in-process
```

The BEEM corpus (276 models, `examples/divine/`) and an MCC NUPN set with
its oracle (`examples/mcc/`) are checked in; `tests/dve_sweep.sh`
regenerates and scores the former against `its-reach` baselines.

**Use the library.** `include/hsc/` + `src/` build the `hsc::hsc` static
library. The programs in `examples/` (`philosophers.cc`, `hanoi.cc`) show
the C++ API: build state spaces as data, compile events to operation
terms, saturate, query — each validated against an independent oracle.

## Layout

| path | what |
|---|---|
| `include/hsc/`, `src/` | the library: `core/` (calculus), `mem/` (substrate), `leaves/`, `lia/`, `surface/` (the `.hsc` pipeline), `order/`, `dve/`, `petri/` |
| `tools/` | the CLI binaries: `hsc`, `hsc-mcc`, `dve2hsc`, `nupn2hsc` |
| `examples/` | self-checking programs and the model corpora (`models/`, `param/`, `divine/`, `mcc/`) |
| `tests/` | doctest suite (differential: `saturate == naive`), sweep and baseline scripts |
| `doc/` | the `.hsc` language manual |
| `research_notes/` | the paper draft and the roadmap |
| `bench/` | benchmarks (empty until the parity milestone) |

Every folder carries a `README.md` (source map) and often an
`algorithm.md` (the algorithms, written before the code — self-contained,
no reading order dependency on the paper).

## Documentation map

* [`doc/hsc_manual.md`](doc/hsc_manual.md) — the `.hsc` language, user view.
* [`research_notes/hsc_core.md`](research_notes/hsc_core.md) — the theory:
  the shape calculus, its normal form, saturation, the crossing fragment.
* [`research_notes/roadmap.md`](research_notes/roadmap.md) — what is done,
  what is open, and the gates.
* [`HAZARDS.md`](HAZARDS.md) — known-unsafe interactions, armed or not.
* `include/hsc/core/algorithm.md`, `include/hsc/mem/algorithm.md`, … — the
  per-package algorithm descriptions.

> This material is **unpublished** work in progress. Feedback and
> collaboration are welcome — contact **Yann.Thierry-Mieg@lip6.fr** or open
> a GitHub issue.

## License

Distributed under the **GNU General Public License v3.0** (see
[`LICENSE`](LICENSE)).

© 2026 Yann Thierry-Mieg, LIP6, Sorbonne Université, CNRS.
