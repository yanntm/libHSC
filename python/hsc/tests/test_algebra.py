"""The shadow oracle: the calculus is the contestant, brute force the referee.

Over tiny carriers a diagram is just a set of words, so every kernel
operation has an independent judge. Checked here: agreement of meet/join/
relative difference with set operations, the degeneracy ledger on every
constructed node, canonicity (equal denotation implies pointer equality,
which is the strongest possible statement of Theorem 5.1), and that filters
and constant assigns act as their extensional kernels say they should.

No dependency beyond the standard library; `random` with a fixed seed.
Run: `python -m tests.test_algebra` from `hsc/`.
"""

from __future__ import annotations

import itertools
import random
from typing import Any, Dict, FrozenSet, List, Set, Tuple

from hsc import Model, compose, enum, star, sum_of
from hsc.core.algebra import diff, join, meet
from hsc.core.diagram import Node
from hsc.core.shape import LeafShape, Pair, Shape

Word = Tuple[Tuple[str, Any], ...]

SHAPES = [
    ("a", ("b", "c")),
    (("a", "b"), ("c", "d")),
    ("a", ("b", ("c", "d"))),
]


def make(spec: Any) -> Model:
    names = sorted({n for n in _names(spec)})
    leaves = {n: enum(n, 0, 1, 2) for n in names}
    return Model(spec, leaves)


def _names(spec: Any) -> List[str]:
    if isinstance(spec, str):
        return [spec]
    return _names(spec[0]) + _names(spec[1])


def all_words(m: Model) -> List[Word]:
    names = sorted(m.paths)
    order = [n for n, _ in sorted(m.paths.items(), key=lambda kv: kv[1])]
    return [tuple(zip(order, vals)) for vals in itertools.product((0, 1, 2), repeat=len(order))]


def to_diagram(m: Model, words: Set[Word]):
    """Sum of singletons — the least clever construction, hence a fair judge."""
    out = None
    for w in words:
        out = join(m.shape, out, m.word(**dict(w)))
    return out


def to_words(m: Model, d: Any) -> Set[Word]:
    order = [n for n, _ in sorted(m.paths.items(), key=lambda kv: kv[1])]
    return {tuple((n, w[n]) for n in order) for w in m.words(d)}


def check_ledger(shape: Shape, d: Any) -> None:
    if d is None or isinstance(shape, LeafShape):
        return
    assert isinstance(shape, Pair) and isinstance(d, Node)
    subs = []
    for i, (p, s) in enumerate(d.pairs):
        assert p is not None and s is not None, "zero written into a node"
        subs.append(s)
        for q, _ in d.pairs[i + 1 :]:
            assert meet(shape.head, p, q) is None, "(D) violated"
        check_ledger(shape.head, p)
        check_ledger(shape.tail, s)
    ids = [id(s) for s in subs]
    assert len(set(ids)) == len(ids), "(F) violated"


def test_algebra(trials: int = 40) -> None:
    rng = random.Random(20260721)
    for spec in SHAPES:
        m = make(spec)
        universe = all_words(m)
        for _ in range(trials):
            k = rng.randint(0, 8)
            wa: Set[Word] = set(rng.sample(universe, k))
            wb: Set[Word] = set(rng.sample(universe, rng.randint(0, 8)))
            da, db = to_diagram(m, wa), to_diagram(m, wb)

            assert to_words(m, da) == wa, "round trip"
            check_ledger(m.shape, da)

            assert to_words(m, meet(m.shape, da, db)) == wa & wb
            assert to_words(m, join(m.shape, da, db)) == wa | wb
            assert to_words(m, diff(m.shape, da, db)) == wa - wb

            # Canonicity: same denotation, same object. Insertion order must
            # not matter, which is the whole content of Theorem 5.1.
            shuffled = list(wa)
            rng.shuffle(shuffled)
            assert to_diagram(m, set(shuffled)) is da, "canonicity violated"
            assert m.count(da) == len(wa)


def test_local_homs(trials: int = 40) -> None:
    rng = random.Random(1789)
    for spec in SHAPES:
        m = make(spec)
        universe = all_words(m)
        names = sorted(m.paths)
        for _ in range(trials):
            wa = set(rng.sample(universe, rng.randint(0, 10)))
            da = to_diagram(m, wa)

            n = rng.choice(names)
            vals = tuple(rng.sample((0, 1, 2), rng.randint(1, 3)))
            got = to_words(m, m.apply(m.keep(n, *vals), da))
            assert got == {w for w in wa if dict(w)[n] in vals}, "filter"

            n2, v = rng.choice(names), rng.choice((0, 1, 2))
            got = to_words(m, m.apply(m.set(**{n2: v}), da))
            want = {tuple((k, v if k == n2 else x) for k, x in w) for w in wa}
            assert got == want, "constant assign"


def test_star_reaches_closure() -> None:
    """Two spellings of the same system must agree: scheduling is below
    the semantics, so `(f+g)*` and iterating `f*` and `g*` to a fixpoint
    reach the same set."""
    m = make(("a", ("b", "c")))
    f = compose(m.set(a=1), m.keep("a", 0))
    g = compose(m.set(b=2), m.keep("a", 1))
    x0 = m.word(a=0, b=0, c=0)
    one = m.apply(star(sum_of(f, g)), x0)
    cur = x0
    while True:
        nxt = m.apply(star(g), m.apply(star(f), cur))
        if nxt is cur:
            break
        cur = nxt
    assert one is cur, "two fair schedules disagree"


def main() -> None:
    test_algebra()
    test_local_homs()
    test_star_reaches_closure()
    print("shadow oracle: all checks pass")


if __name__ == "__main__":
    main()
