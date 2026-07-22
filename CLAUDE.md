# libHSC — Working Notes for Claude

libHSC is the reboot of libDDD/libITS around the **Hierarchical Shape
Calculus** (HSC): hierarchy and `split_equiv` as the base, flat decision
diagrams and plain homomorphisms as recognized degenerations. The project
runs concepts-first: the calculus drafts drive the design, the design
documents drive the code.

## Project layout (source map)

* `research_notes/` : the calculus drafts and design documents
  - `hsc_core*.md` : the calculus series; **highest number is current**,
    lower numbers are history kept for the record of what moved and why.
  - `ddd_to_hsc.md` : the bridge document — goals of the reboot, the
    concept↔code correspondence with the legacy code bases, dispositions
    for legacy features, milestones. Read it first when engineering.
  - `hsc_spec.md` : spec of the first prototype iteration (drives
    `python/hsc/`; predates draft 3).
* `python/hsc/` : the Python prototype of the calculus (moved from
  BuchiToLTL). Performance is not its goal; its products are the API
  shape, the measured cost model, and pressure on the theory's open
  obligations. It has its own `README.md` and `algorithm.md` — read those
  before touching it. It currently tracks `hsc_core.md` (draft 1); part of
  its job is to be re-aligned draft by draft.
* (future) `include/hsc/`, `src/`, `tests/`, `bench/` : the C++ core
  (C++20/23, CMake). Not yet created; gated on the theory-interface
  design in `ddd_to_hsc.md`.

Reference code bases (read-only from this project; never edit them from
here):

* `~/git/libDDD` : the legacy engine — DDD/SDD nodes, Hom/SHom,
  saturation, unique tables, caches. The performance baseline.
* `~/git/libITS/its/gal/` : the GAL expression engine (`ExprHom.cpp`,
  `IntExpression.*`) — the equiv-split prototype over flat DDD, i.e. the
  LIA theory before it knew it was one.
* `~/git/BuchiToLTL` : aut2ltl — source of ω-word / recognizer theories
  to import later, and where the hsc drafts were first written.

## Documents discipline

* Paper drafts follow the three-file convention: the paper itself
  (`research_notes/<name>.md`), a spec file to drive engineering, a report
  file with the engineering response. All data in a paper comes from its
  report; reports trace to git-tracked machine-generated artefacts.
* The `hsc_core` series is iterated by **whole drafts**: a new draft is a
  new file (`hsc_coreN.md`) with a "what moved since draft N−1" preamble;
  earlier drafts are never edited.
* Every code folder bears a `README.md` (source map) and/or `algorithm.md`
  (abstract description of the algorithms). Always read the README before
  using or editing files in a folder; keep these in sync with the code.
* Doc first: when starting new code or an algorithm implementation, write
  the algorithm/README before the code, then transcribe.

## Discipline (mandatory)

* One commit per file by preference if editing an existing file,
  especially core files. Bulk commits ok for one logical change (moved
  code, new datasets, a document plus its index entry).
* Use `git commit -F -` with a heredoc and a terse message.
* We are solo: master only, no branches. Never rewrite history — no
  reset/rebase/amend, even unpushed; fix forward.
* git add/rm/mv populate the index: always follow them with a commit
  before staging anything else. Don't leave the index open.
* **Pushing**: don't ask, don't do it. User pushes when the project is
  stable.
* No persistent memory files: capture anything durable in documentation.
  Never edit this file unless prompted by the user.
* Work in folder: avoid /tmp and the scratchpad; a crashed session should
  leave all traces of work *in* the folder.
* Keep files roughly under 500 LOC; small single-responsibility files
  organized into folders, documented by their README/algorithm.md.
* Comments are context-free: describe the code in that file, current
  state only — not callers, not history (git holds history).

## Working style

* Diagnostics self-bound, ≤15s per example; a blown timeout IS a finding.
  Redirect long output to logs inside the folder, never /tmp.
* No manual process management: no kill/pkill, no `&`/nohup. Long
  self-terminating runs go through the harness's background facilities.
* Honest failure attribution: distinguish what WE failed at from what a
  reference tool or the legacy baseline can't handle.
* Present intermediate results; do not start a new direction without user
  validation.
* Python code: explicit type annotations on new/touched functions (the
  user comes from Java/C++). When the C++ core starts: C++20/23, no
  compiler-specific extensions, benchmarks against libDDD are part of the
  definition of done for performance-sensitive layers (the parity gate in
  `ddd_to_hsc.md`).
* We are scientists; we don't cheat or misrepresent data. If you find
  issues or bias in an experiment, report it.

## Roles

A session starts in one of two roles:

1. **Theory**: reads the drafts and markdown only; solves issues on
   paper, hand-works examples, writes/revises specs; reads engineering
   reports and feeds back. Never looks at code; preserves context for the
   math. May read the legacy *papers*, not the legacy sources.
2. **Engineering**: implements the spec, tests, reports in the report
   file. Reads the spec (and the paper if needed), and the legacy sources
   in `~/git/libDDD` / `~/git/libITS` when the bridge document points
   there. If stuck or surprised, report and ask for Theory feedback
   rather than improvising the calculus.

Theory threads use the strongest model available, engineering a lighter
one: Theory writes precise specs with concrete steps and expected
outcomes; engineering trusts its spec and asks when in doubt.
