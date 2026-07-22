"""The support algebra and the canonical decomposition.

`normalize`, `join` and `diff` are mutually recursive: normalising at a cut
joins subs one level down, which normalises at the next cut. They live in
one module for that reason.

No `top` and no `complement` appear anywhere below. Every carving is a meet
or a *relative* difference against a prime already present, so no cell is
ever expressed against the ambient carrier.
"""

from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence, Tuple

from .diagram import DEBUG, ONE, Diagram, Node, Rect, duid, mk
from .stats import tick
from .shape import LeafShape, Pair, Shape, Unit

_MEET: Dict[Tuple[int, int, int], Diagram] = {}
_JOIN: Dict[Tuple[int, int, int], Diagram] = {}
_DIFF: Dict[Tuple[int, int, int], Diagram] = {}


def clear_caches() -> None:
    _MEET.clear()
    _JOIN.clear()
    _DIFF.clear()


# ---------------------------------------------------------------- meet


def meet(shape: Shape, a: Diagram, b: Diagram) -> Diagram:
    if a is None or b is None:
        return None
    if a is b:
        return a
    if isinstance(shape, Unit):
        return ONE
    if isinstance(shape, LeafShape):
        r = shape.leaf.meet(a, b)
        return None if shape.leaf.is_empty(r) else r
    tick("node.meet")
    key = (shape.uid, duid(shape, a), duid(shape, b))
    got = _MEET.get(key)
    if got is not None or key in _MEET:
        return got
    assert isinstance(shape, Pair) and isinstance(a, Node) and isinstance(b, Node)
    rects: List[Rect] = []
    for p, s in a.pairs:
        for q, t in b.pairs:
            pq = meet(shape.head, p, q)
            if pq is None:
                continue
            st = meet(shape.tail, s, t)
            if st is None:
                continue
            rects.append((pq, st))
    out = normalize(shape, rects)
    _MEET[key] = out
    return out


# ---------------------------------------------------------------- join


def join(shape: Shape, a: Diagram, b: Diagram) -> Diagram:
    if a is None:
        return b
    if b is None:
        return a
    if a is b:
        return a
    if isinstance(shape, Unit):
        return ONE
    if isinstance(shape, LeafShape):
        return shape.leaf.join(a, b)
    tick("node.join")
    key = (shape.uid, duid(shape, a), duid(shape, b))
    got = _JOIN.get(key)
    if got is not None or key in _JOIN:
        return got
    assert isinstance(shape, Pair) and isinstance(a, Node) and isinstance(b, Node)
    out = normalize(shape, list(a.pairs) + list(b.pairs))
    _JOIN[key] = out
    return out


# ---------------------------------------------------------------- diff


def diff(shape: Shape, a: Diagram, b: Diagram) -> Diagram:
    """Relative difference a \\ b."""
    if a is None:
        return None
    if b is None:
        return a
    if a is b:
        return None
    if isinstance(shape, Unit):
        return None  # 1 \ 1 = 0; there is nothing else there
    if isinstance(shape, LeafShape):
        r = shape.leaf.diff(a, b)
        return None if shape.leaf.is_empty(r) else r
    tick("node.diff")
    key = (shape.uid, duid(shape, a), duid(shape, b))
    got = _DIFF.get(key)
    if got is not None or key in _DIFF:
        return got
    assert isinstance(shape, Pair) and isinstance(a, Node) and isinstance(b, Node)
    rects: List[Rect] = []
    for p, s in a.pairs:
        rem: Diagram = p
        for q, t in b.pairs:
            if rem is None:
                break
            pq = meet(shape.head, rem, q)
            if pq is None:
                continue
            st = diff(shape.tail, s, t)
            if st is not None:
                rects.append((pq, st))
            rem = diff(shape.head, rem, pq)
        if rem is not None:
            rects.append((rem, s))
    out = normalize(shape, rects)
    _DIFF[key] = out
    return out


# ----------------------------------------------------------- normalize


def normalize(shape: Pair, rectangles: Sequence[Rect]) -> Optional[Node]:
    """Theorem 5.1, operationally: the unique canonical node denoting the sum
    of the given rectangles, which may overlap and repeat freely.

    Incremental insertion into a maintained disjoint partition — not the
    sign-pattern cell enumeration, which is exponential for the same result.
    """
    tick("node.normalize")
    head, tail = shape.head, shape.tail
    cells: List[Rect] = []
    for p, s in rectangles:
        if p is None or s is None:
            continue  # smash: no zero primes, no zero subs, before anything is written
        rem: Diagram = p
        nxt: List[Rect] = []
        for q, t in cells:
            if rem is None:
                nxt.append((q, t))
                continue
            inter = meet(head, rem, q)
            if inter is None:
                nxt.append((q, t))
                continue
            rest = diff(head, q, inter)
            if rest is not None:
                nxt.append((rest, t))
            nxt.append((inter, join(tail, t, s)))
            rem = diff(head, rem, inter)
        if rem is not None:
            nxt.append((rem, s))
        cells = nxt

    # (F)-compression: group by sub, joining their primes.
    by_sub: Dict[int, Rect] = {}
    for q, t in cells:
        k = duid(tail, t)
        prev = by_sub.get(k)
        by_sub[k] = (q, t) if prev is None else (join(head, prev[0], q), t)

    pairs = tuple(sorted(by_sub.values(), key=lambda pt: duid(tail, pt[1])))
    if DEBUG:
        _assert_ledger(shape, pairs)
    return mk(shape, pairs)


def _assert_ledger(shape: Pair, pairs: Tuple[Rect, ...]) -> None:
    """The degeneracy ledger of the calculus document, as runtime assertions.

    Debug mode checks; release mode assumes, which is exactly the promise
    that zero-freeness is self-maintaining over positive coefficients."""
    seen_subs = set()
    for i, (p, s) in enumerate(pairs):
        assert p is not None, "zero prime written"
        assert s is not None, "zero sub written"
        k = duid(shape.tail, s)
        assert k not in seen_subs, "(F) violated: repeated sub"
        seen_subs.add(k)
        for q, _ in pairs[i + 1 :]:
            assert meet(shape.head, p, q) is None, "(D) violated: overlapping primes"


# --------------------------------------------------------- measurement


def size(shape: Shape, d: Diagram) -> Dict[int, int]:
    """Prime counts per depth: the congruence tower, level by level.

    Summed, this is the size measure the compression remark reasons about."""
    acc: Dict[int, int] = {}
    seen: set = set()

    def walk(sh: Shape, x: Diagram, depth: int) -> None:
        if x is None or isinstance(sh, (LeafShape, Unit)):
            return
        assert isinstance(sh, Pair) and isinstance(x, Node)
        if x.uid in seen:
            return
        seen.add(x.uid)
        acc[depth] = acc.get(depth, 0) + len(x.pairs)
        for p, s in x.pairs:
            walk(sh.head, p, depth + 1)
            walk(sh.tail, s, depth + 1)

    walk(shape, d, 0)
    return acc


def count(shape: Shape, d: Diagram) -> int:
    """Number of words denoted. Leaf-cardinality-driven, so it needs leaves
    that can enumerate; it is a shadow operation, not a kernel one."""
    if d is None:
        return 0
    if isinstance(shape, Unit):
        return 1
    if isinstance(shape, LeafShape):
        return sum(1 for _ in shape.leaf.elements(d))
    assert isinstance(shape, Pair) and isinstance(d, Node)
    return sum(
        count(shape.head, p) * count(shape.tail, s) for p, s in d.pairs
    )
