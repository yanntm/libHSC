"""M0's gate: a deliberately wrong `normalize` must be caught by the oracle.

An oracle nobody has falsified is not yet an oracle. Each mutant below drops
one line of the degeneracy ledger; the shadow tests must fail for every one.

Run: `python -m tests.test_mutation` from `hsc/`.
"""

from __future__ import annotations

from typing import Callable, Dict, List, Optional, Sequence

from hsc.core import algebra
from hsc.core.diagram import Rect, duid, mk
from hsc.core.shape import Pair

from . import test_algebra as oracle

_GOOD = algebra.normalize


def _no_compression(shape: Pair, rectangles: Sequence[Rect]):
    """Drops (F): equal subs are left unmerged, so canonicity dies."""
    cells: List[Rect] = []
    for p, s in rectangles:
        if p is None or s is None:
            continue
        rem = p
        nxt: List[Rect] = []
        for q, t in cells:
            if rem is None:
                nxt.append((q, t))
                continue
            inter = algebra.meet(shape.head, rem, q)
            if inter is None:
                nxt.append((q, t))
                continue
            rest = algebra.diff(shape.head, q, inter)
            if rest is not None:
                nxt.append((rest, t))
            nxt.append((inter, algebra.join(shape.tail, t, s)))
            rem = algebra.diff(shape.head, rem, inter)
        if rem is not None:
            nxt.append((rem, s))
        cells = nxt
    pairs = tuple(sorted(cells, key=lambda pt: duid(shape.tail, pt[1])))
    return mk(shape, pairs)


def _no_disjointness(shape: Pair, rectangles: Sequence[Rect]):
    """Drops (D): rectangles are stored as given, overlaps and all."""
    cells = [(p, s) for p, s in rectangles if p is not None and s is not None]
    by_sub: Dict[int, Rect] = {}
    for q, t in cells:
        k = duid(shape.tail, t)
        prev = by_sub.get(k)
        by_sub[k] = (q, t) if prev is None else (algebra.join(shape.head, prev[0], q), t)
    pairs = tuple(sorted(by_sub.values(), key=lambda pt: duid(shape.tail, pt[1])))
    return mk(shape, pairs)


MUTANTS: Dict[str, Callable] = {
    "no (F)-compression": _no_compression,
    "no (D)-disjointness": _no_disjointness,
}


def main() -> None:
    algebra.DEBUG = False  # the assertions would mask the oracle's own verdict
    for name, mutant in MUTANTS.items():
        algebra.normalize = mutant  # type: ignore[assignment]
        algebra.clear_caches()
        try:
            oracle.test_algebra(trials=10)
            oracle.test_local_homs(trials=10)
        except AssertionError as exc:
            print(f"caught {name}: {exc}")
        else:
            raise SystemExit(f"MUTANT SURVIVED: {name} — the oracle is not an oracle")
        finally:
            algebra.normalize = _GOOD  # type: ignore[assignment]
            algebra.clear_caches()
    print("mutation gate: every mutant caught")


if __name__ == "__main__":
    main()
