# `lia/` — the interchange theory of linear integer arithmetic

Interned integer and boolean expressions over *positions* — the language in
which cross-level criteria and updates (§7) are written, curried, and cached.
A `split_equiv` label, a residual after substitution, a guard on a term: all
are codes of this package.

Adapted from the GAL expression engine (`libITS/its/gal`, the
`PIntExpression` / `PBoolExpression` "positional" layer), which is refined
where it matters: hash-consed nodes, constants hidden inside the handle (no
node, no memory traffic for the overwhelmingly common case), n-ary operands
in canonical order, substitution that renormalizes. The refactoring:

* libDDD `UniqueTable` + refcounted pointers → `mem::intern` and 32-bit
  codes, like every other value in libHSC. The tag moves from the pointer's
  low bit to the code's low bit.
* `d3::multiset` operand storage → a sorted trailing array in the node.
* Normalization moves **into the factory**: construction itself flattens,
  folds and sorts, so every held code is in normal form and `eval()` as a
  separate pass disappears. (The reference's `createNary` computed a
  flattened operand list and then interned the unflattened original — the
  flattening was dead code. Here it is the contract.)
* Arithmetic folds are overflow-checked and loud, as in `int_set`; GAL's
  silent int wrap is not kept. Division and modulo by zero fold to the
  undefined expression, like an out-of-bounds array access.
* The named layer (`IntExpression`, `Variable`, `Environment`) is not
  ported: the surface already resolves flat names to frontier positions, and
  positions are what expressions speak.

## Files

* `expr.hh` — codes, node kinds, and the two factories (int and bool);
  the whole public surface.
* `algorithm.md` — the representation, the normal form, substitution.
* `src/lia_int.cc` — integer expressions: construction, normalization,
  substitution, support.
* `src/lia_bool.cc` — boolean expressions: comparisons, and/or/not,
  negation normal form, substitution.
* `tests/test_lia.cc` — doctests: canonicity, folding, currying residuals.
