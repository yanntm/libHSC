"""Diagrams: the hash-consed canonical representation.

A diagram over a shape is, by cases on the shape:

    None            the adjoined zero, at every shape; never stored in a
                    node, never traversed, never a letter
    at <A>          a canonical nonempty leaf code
    at (V_h,V_t)    a Node: a nonempty tuple of (prime, sub) pairs, primes
                    pairwise disjoint (D), subs pairwise distinct (F),
                    sorted by the sub's uid

A prime over a composite head is itself a diagram over that head: there is
one diagram type, not two, and the calculus eating its own output is the
class hierarchy rather than a feature.

Nodes are unique-tabled, so equality is `is` and zero is absence. Leaf
codes are given uids from a side table; that uid is the only thing the
kernel asks of a code besides hashability, and it exists to give the
canonical sort a total order.
"""

from __future__ import annotations

from typing import Any, Dict, List, Optional, Tuple

from .shape import LeafShape, Pair, Shape, Unit

Diagram = Any  # None | Terminal | leaf code | Node
Rect = Tuple[Diagram, Diagram]

DEBUG: bool = True

ZERO_UID = 0


class Terminal:
    """The value `1` at the unit sort: acceptance, reached.

    A singleton, so equality is `is` like every other diagram. Over the
    Boolean coefficients it is the only nonzero value there is at that sort;
    a weighted pass replaces it by a coefficient without touching anything
    that refers to it."""

    __slots__ = ()

    def __repr__(self) -> str:
        return "1"


ONE = Terminal()
ONE_UID = 1


class Node:
    """A cut's canonical decomposition: sum of rank-one terms `prime (x) sub`."""

    __slots__ = ("shape", "pairs", "uid")

    def __init__(self, shape: Pair, pairs: Tuple[Rect, ...], uid: int) -> None:
        self.shape = shape
        self.pairs = pairs
        self.uid = uid

    def __len__(self) -> int:
        """Number of primes at this cut: the section rank."""
        return len(self.pairs)

    def __repr__(self) -> str:
        return f"Node#{self.uid}[{len(self.pairs)}]"


_NODES: Dict[Tuple[int, Tuple[Tuple[int, int], ...]], Node] = {}
_LEAF_UIDS: Dict[Tuple[int, Any], int] = {}
_NEXT_UID: List[int] = [2]


def _fresh() -> int:
    uid = _NEXT_UID[0]
    _NEXT_UID[0] += 1
    return uid


def duid(shape: Shape, d: Diagram) -> int:
    """Total-order key for a diagram at `shape`. Zero is 0; nodes carry theirs."""
    if d is None:
        return ZERO_UID
    if d is ONE:
        return ONE_UID
    if isinstance(d, Node):
        return d.uid
    key = (shape.uid, d)
    got = _LEAF_UIDS.get(key)
    if got is None:
        got = _LEAF_UIDS[key] = _fresh()
    return got


def is_zero(d: Diagram) -> bool:
    """Zero test at any shape. At composite shapes this is a pointer test,
    which is what the self-maintaining zero-freeness invariant buys."""
    return d is None


def mk(shape: Pair, pairs: Tuple[Rect, ...]) -> Optional[Node]:
    """Intern a node from pairs already in canonical form (D, F, sorted)."""
    if not pairs:
        return None
    key = (
        shape.uid,
        tuple((duid(shape.head, p), duid(shape.tail, s)) for p, s in pairs),
    )
    got = _NODES.get(key)
    if got is None:
        got = _NODES[key] = Node(shape, pairs, _fresh())
    return got


def stats() -> Dict[str, int]:
    return {"nodes": len(_NODES), "leaf_codes": len(_LEAF_UIDS)}
