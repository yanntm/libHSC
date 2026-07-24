# `examples/param/` — parametric surface samples

Each file is one template over a size `param`, self-checking via `expect`;
`hsc --expand FILE` prints the flat `.hsc` it denotes. The parametric
forms are documented in `doc/hsc_manual.md` (user view) and
`include/hsc/surface/algorithm.md` (compile map).

* `hanoi.hsc` — N rings, P poles. Nested `exists` (a 3-index event family),
  a dependent-range `forall` guard (`j` below `k`), the shape laid by a
  binder. Oracles: `P^N` states, matching `tools/hanoi.py 7` independently;
  the goal state (`forall` in a select) reached exactly once.
* `philo.hsc` — N dining philosophers over two parallel arrays with
  `(% (+ i 1) N)` fork addressing. Oracles: saturate vs naive agree; a
  crossing quantified predicate (neighbours eating) has an empty image.
* `mutex.hsc` — after `~/git/ITS-Exercise/MutexParam.gal`. "All j except i"
  as two dependent ranges; the init event states the real initial marking
  (the GAL original must bootstrap it through a pseudo-state). Oracles:
  saturate vs naive; mutual-exclusion image empty.
* `hanoipole.hsc` — Hanoi again, as a P×N *2-D* occupancy-bit array, after
  `~/git/ITS-Exercise/hanoipole.gal` (which linearizes indices by hand and
  boots through an init flag dragged into every guard). Oracle: the same
  `3^N` as hanoi.hsc — two encodings, one answer; goal reached once.
* `rotate.hsc` — indexed init (`a[i] := i`) and a *simultaneous* rotation:
  the binder inside one `do` clause is a synchronous multi-assign. Oracle:
  exactly the N cyclic shifts.

The GAL counterparts in `~/git/ITS-Exercise` (read-only reference) show
what the binders replace: statement-level `for`/`if`/`abort` where a
quantified guard suffices, and per-cell spellings of quantified properties.
