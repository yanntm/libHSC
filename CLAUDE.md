# libHSC — Working Notes for Claude

## Project layout

Sources :

* include/hsc/ : the C++ core headers — `core/` (calculus), `mem/` (substrate),
  `leaves/` (imported theories), `util/`
* src/ : the C++ translation units
* tests/ : doctest suite, probes, support code
* examples/ : runnable programs, each checking itself against an oracle
* bench/ : benchmarks
* python/hsc/ : the Python prototype of the calculus

Reference code bases, read-only from this project; never edit them from here :

* `~/git/libDDD` : the legacy engine — DDD/SDD nodes, Hom/SHom, saturation,
  unique tables, caches. The performance baseline.
* `~/git/libITS/its/gal/` : the GAL expression engine.
* `~/git/BuchiToLTL` : aut2ltl, source of ω-word / recognizer theories.

Formal Reasoning :

* research_notes/ : holds paper drafts

Paper drafts :

* Each has 3 files in research_notes :

1. the paper itself. Written up. Depending on advancement, a rough draft to polished.
As it progresses towards a paper outline : abstract=teaser, 1 : define title, the problem, why its important, contributions, outline of paper. 2. Context : any definition we need to define ourselves but that is not original in this paper, subsections ok. 3+ contributions ~7. related work 8. Conclusion
papers/ holds our library as both pdf, and .txt extracts of it, we don't cite unless we read the paper we cite.
2. a spec or experiments file to drive engineering, and in later stages, data collection.
It is precise and contains milestones for dev, and description of tables/experiments to perform.
3. a report file with the engineering team response to the specification.
It bears the reproducibility constraints for the paper, all data in the paper comes from here.
Data in here is traceable to git tracked machine gen artefacts and reports : it will be the reproducibility artefact submitted with the paper at some point.
4. Most have a `handoff_<thread>.md` at root: the bootstrap document, **current state only** — the TODO queue per role (theory / engineering), next action first, blockers named. Rewritten, never appended to: no dates, no attribution, no session narrative. **A completed item is flushed entirely** — its result belongs in the report, its lesson in the folder's README/algorithm.md, its history in git. The handoff does not grow; length is the signal to evict.

## Orientation (don't duplicate here — follow the pointers)
- `README.md` — repo guide / quick start. STATUS/TODO are stale ignore.
- `include/hsc/core/algorithm.md` — the calculus as implemented: codes, shapes,
  the normal form, the canonicalizer, the set algebra.
- `include/hsc/mem/algorithm.md` — the substrate: interning, handles, caches.
- The `hsc_core*.md` series in research_notes is the calculus; highest number is
  current, lower numbers are history kept for the record of what moved and why.
- Every folder bears some form of README.md (source map, overview of services) and/or algorithm.md
  that provides an abstract description of algo. Always read the readme before using or editing files in a folder.
  Maintain these docs in sync with code.



Discipline (mandatory)
- One commit per file by preference if editing an existing file. Especially if its core.
  Bulk commits of several files ok in one logical commit. Single commit for moved code, new datasets...
- Use "git commit -F - heredoc" with a terse message to commit
- We are solo : we only use master, no branches.
1. Commit order is not important, ignore that intermediate states of git are unstable.
Just commit your work, regularly as it progresses and reaches new increments.
Update report and handoff as appropriate when tasks are done.
2. We are many concurrent editors on the folder, so git add/rm/mv close to git commit, don't leave index open.
Also means that history of repo wide scope is above your paygrade, don't look.
You can git diff against previous version your reference files (report, paper, spec...) that's about it.

- **Pushing** : don't ask, don't do it. User will push (or ask to do so) when overall project is stable, above your paygrade.

- Remember that git operations like `git mv` or `git rm` already populate the
  index. So always follow them with a commit *before* attempting to add any
  modified file to index.
- **Never rewrite history** — no reset/rebase/amend, even on unpushed commits.
  A garbled commit message stands; fix forward with a new commit. (To avoid
  mangled messages: don't put backticks in a double-quoted `-m`; use `-F` with
  a quoted heredoc.)
- Commit directly to `master` (we do not branch). Never run branch / cross-branch
  diagnostics.
- NO persistent "memory" files: the user does not use them (not inspectable in the
  repo, they bloat unseen). Capture anything durable in documentation.
  Never edit this file unless prompted by user.
- Test often, but let user guide how much. Too many unit level tests can be churn on rapidly evolving code base.
- Avoid one-off inline probes for more than about 5 lines of test/probe. Above that materialize a file in tests/.
If its really one shot ok, rm after use. Else add/commit. This saves tokens among other things.
- Work in folder : avoid /tmp, avoid placing files in your scratchpad. If your session crashes, it should leave all traces of work *in* the folder.


- Keep files roughly under 500 LOC (a technical core may occasionally exceed this
  where a split would be artificial). We like our files small, single responsibility,
  organized into folders for better documentation.
- When documenting code, make comments that are context free: this means
  describing the code *in that file*, not its callers, not a specific use case.
  Source documentation is also not a log. Describe code *current state* not its history, that is for git only.

- When starting new code or an algorithm implementation in a new package : the process is doc first.
  Write the algorithm/Readme before the code. Then use those documents to drive the coding.
  Algorithm.md are higher level descriptions of the strategy, once written well, the code is simply its transcription to code.

## Working style (how the user wants me to operate)
- **Diagnostics self-bound, ≤15s PER EXAMPLE.** Hard cap on any test/diagnostic
  run; a blown timeout IS a finding, report it. The cap is per example — do NOT
  batch many cases into one run to dodge it (give probes a single-input argv and
  invoke once per case). Redirect long output to `tests/**/logs/` (never /tmp),
  don't pipe long runs to `tail`.
- **No manual process management.** Never `kill`/`pkill`, no `&`/`nohup`/`$!`, no
  inspecting pids. For long self-terminating runs use the Monitor tool or Bash
  `run_in_background` (the harness tracks the task and re-invokes on completion);
  wait on completion events, never sleep/poll loops.
- **Legacy tools are bounded-or-skipped, never waited on** in the construction/test
  path — libDDD and ITS-tools run as external baselines for comparison, not as
  dependencies. A stall is reported, not blocked on.
- **Honest failure attribution.** Distinguish what WE failed at (a crash, a
  construction timeout with no diagram) from what a reference tool or the legacy
  baseline can't handle. A model we built that libDDD can't verify is NOT our
  failure.
- **Present intermediate results.** Stop and show results after each step; do not
  start a new direction without user validation.
- **Trust the scripts.** Existing scripts the user points me at and tells me to
  run are meant to be run, not to be read. Trust that the default flags do the
  job, at most `head` the script to see its pydoc. Avoid poisoning context by
  reading code we are not editing.
- **Type the signatures.** C++20/23, no compiler-specific extensions. In Python,
  add explicit type annotations (params + return) on new/touched functions — the
  user comes from Java/C++. Use `typing` (`Optional`/`Callable`/`Protocol`/
  forward-ref strings), `TYPE_CHECKING` for annotation-only imports; `Protocol`
  for behavioral contracts.

We are scientists, we don't cheat or misrepresent data.
If you find issues or bias in an experiment report it to user.

A session will start with one of two roles :

1. Theory : you will read papers/ and only markdown in research_notes essentially.
You solve issues on paper, prove, hand work examples, predict outcomes, write the spec that drives engineering.
Never looks at actual code base, preserves its context from technical detail to do better math.
Reads engineering reports on its spec, feedbacks on them by appending a response paragraph where necessary, and by revising spec.
Integrate the report results to the paper. Drive experimentation.

2. Engineering : you will implement the specification, test it, set up experiment campaigns.
You report in a report file. You can read the paper you are implementing, but in most cases spec should suffice.
You develop solutions incrementally; if meeting some issue with spec or an unexpected find (error/misprediction...)
that you are not sure how to deal with, you report it and ask for Theory feedback or spec edit as relevant.

Theory threads typically use the best model we have, engineering a slightly less powerful one.
So mostly, engineering should trust its specs, and ask if stuck/in doubt.
And Theory should make allowance for weaker models by being precise in specification, with concrete steps and expected outcomes.
