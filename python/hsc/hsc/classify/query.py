"""`split_equiv`: the partition primitive, and `theta` on top of it.

    split_equiv(shape, d, expr) -> Partition of d

Partition `d` into the pieces on which `expr` has each realised residual.
Zero pieces are absent, so the label set *is* the discovered alphabet: empty
classes are never represented and pieces that agree are merged before any
client sees them. When a label is ground it is a discovered letter; when it
is not, the caller is looking at a cut the expression has not finished
crossing.

The traversal is the curried residual transport, in three movements.

**Down.** Travel to the first coordinate the classifier mentions. If it
mentions nothing here, return in one entry without descending: locality is
the distance-zero case and costs nothing.

**Across.** Meeting a coordinate substitutes its class and renormalises, so
the travelling classifier stays ground and mentions only coordinates not yet
consumed; a consumed coordinate can never be re-queried. Expressions are
interned, so grouping head-classes by residual code and keying the memo
table on that code are the same act — deduplication *is* cache sharing.

**Up.** Results federate as they return. A partition is stored as its
*kernel* — the canonical tuple of pieces — plus a labelling from residuals
into it. Two subqueries whose residuals differ but whose realised partitions
agree are recognised at this point and share one kernel object, keeping only
their two labellings apart. This is the retroactive repair for weak leaf
normalisation, and it is also where the equivariant collapse appears: it is
not a code path but what the merge finds when the structure is there.
"""

from __future__ import annotations

from typing import Any, Dict, Iterator, List, Tuple

from ..core.algebra import normalize
from ..core.diagram import Diagram, Node, Rect, duid
from .expr import Expr
from ..core.shape import LeafShape, Pair, Shape
from ..core.stats import tick

# kernel identity: (shape, canonical tuple of piece uids) -> kernel id
_KERNELS: Dict[Tuple[int, Tuple[int, ...]], int] = {}
# the representative piece tuple for a kernel, shared by every partition on it
_KERNEL_PIECES: Dict[int, Tuple[Diagram, ...]] = {}
_SPLIT: Dict[Tuple[int, int, int], "Partition"] = {}


def clear_cache() -> None:
    _SPLIT.clear()
    _KERNELS.clear()
    _KERNEL_PIECES.clear()


class Partition:
    """A kernel plus a labelling of it.

    The kernel is the partition itself — the pieces, canonically ordered and
    interned. The labelling says which residual names which piece. Splitting
    the two is what lets partitions federate: agreeing on the kernel is a
    pointer test, and disagreeing on the labelling costs a finite table."""

    __slots__ = ("shape", "pieces", "labels", "kernel")

    def __init__(
        self,
        shape: Shape,
        pieces: Tuple[Diagram, ...],
        labels: Dict[Expr, int],
        kernel: int,
    ) -> None:
        self.shape = shape
        self.pieces = pieces
        self.labels = labels
        self.kernel = kernel

    def get(self, residual: Expr) -> Diagram:
        idx = self.labels.get(residual)
        return None if idx is None else self.pieces[idx]

    def items(self) -> Iterator[Tuple[Expr, Diagram]]:
        for residual, idx in self.labels.items():
            yield residual, self.pieces[idx]

    def keys(self) -> Iterator[Expr]:
        return iter(self.labels)

    def __len__(self) -> int:
        return len(self.pieces)

    def __repr__(self) -> str:
        return f"Partition(kernel={self.kernel}, {len(self.pieces)} pieces)"


EMPTY = Partition(None, (), {}, 0)  # type: ignore[arg-type]


def federate(shape: Shape, mapping: Dict[Expr, Diagram]) -> Partition:
    """Build a partition, sharing its kernel with any partition already seen
    that has the same pieces.

    Two residuals that induce the same split of the same data meet here even
    though their codes never coincided; the labelling tables are what remains
    of their difference."""
    items = [(e, d) for e, d in mapping.items() if d is not None]
    if not items:
        return EMPTY
    order = sorted(range(len(items)), key=lambda i: duid(shape, items[i][1]))
    pieces = tuple(items[i][1] for i in order)

    key = (shape.uid, tuple(duid(shape, p) for p in pieces))
    kernel = _KERNELS.get(key)
    if kernel is None:
        kernel = _KERNELS[key] = len(_KERNELS) + 1
        _KERNEL_PIECES[kernel] = pieces
        tick("node.kernel_new")
    else:
        pieces = _KERNEL_PIECES[kernel]  # federate: one kernel object, shared
        tick("node.kernel_merged")

    labels = {items[i][0]: pos for pos, i in enumerate(order)}
    return Partition(shape, pieces, labels, kernel)


def split_equiv(shape: Shape, d: Diagram, expr: Expr) -> Partition:
    if d is None:
        return EMPTY

    key = (shape.uid, duid(shape, d), expr.uid)
    got = _SPLIT.get(key)
    if got is not None:
        return got

    if expr.is_ground() or not (expr.vars & _names(shape)):
        # Distance zero: nothing here can separate anything, so do not
        # descend -- locality is free. But *do* federate. A level that
        # splits into one piece has still produced a partition, and it must
        # carry the same kernel as any other partition with that piece,
        # whether that one came from a skipped level or from a leaf on which
        # the classifier turned out to be constant. Declining to register it
        # would make equal partitions carry different kernels, and the merge
        # would stop being an equivalence.
        out = federate(shape, {expr: d})
    elif isinstance(shape, LeafShape):
        tick("node.split_equiv")
        out = federate(shape, dict(shape.leaf.split_equiv(d, expr)))
    else:
        tick("node.split_equiv")
        assert isinstance(shape, Pair) and isinstance(d, Node)
        rects: Dict[Expr, List[Rect]] = {}
        for p, s in d.pairs:
            for residual, piece in split_equiv(shape.head, p, expr).items():
                for final, tail_piece in split_equiv(shape.tail, s, residual).items():
                    rects.setdefault(final, []).append((piece, tail_piece))
        out = federate(shape, {e: normalize(shape, rs) for e, rs in rects.items()})

    _SPLIT[key] = out
    return out


_NAME_CACHE: Dict[int, frozenset] = {}


def _names(shape: Shape) -> frozenset:
    got = _NAME_CACHE.get(shape.uid)
    if got is None:
        got = _NAME_CACHE[shape.uid] = frozenset(lf.name for _, lf in shape.leaves())
    return got


def theta(shape: Shape, d: Diagram, expr: Expr) -> Dict[Any, Diagram]:
    """The quotient constructor: discovered letter -> the part it classifies.

    Every returned class is nonempty and no two classify the same part, so
    the alphabet is the coarsest refinement compatible with `expr` on `d` —
    minimal and forced, inherited from the decomposition theorem rather than
    arranged here."""
    out: Dict[Any, Diagram] = {}
    for residual, piece in split_equiv(shape, d, expr).items():
        if not residual.is_ground():
            raise ValueError(
                f"classifier mentions coordinates outside the shape: {residual!r}"
            )
        out[residual.value] = piece  # type: ignore[attr-defined]
    return out


def kernel_report() -> Dict[str, int]:
    """The equivariant harvest, made countable, over everything computed
    since the last `clear_cache`.

    `partitions` counts every partition built; `kernels` counts the distinct
    ones they turned out to be, and their difference is what the merge
    recovered — residuals whose codes never coincided but whose partitions
    did, now sharing one kernel and differing only by a finite relabelling.
    `separating` counts the kernels with more than one piece, since a level
    that split into one piece federates like any other but did not, in the
    end, separate anything."""
    seen: Dict[int, int] = {}
    sizes: Dict[int, int] = {}
    for part in _SPLIT.values():
        if part is EMPTY:
            continue
        seen[part.kernel] = seen.get(part.kernel, 0) + 1
        sizes[part.kernel] = len(part.pieces)
    total = sum(seen.values())
    return {
        "partitions": total,
        "kernels": len(seen),
        "merged": total - len(seen),
        "separating": sum(1 for k in seen if sizes[k] > 1),
    }


def kernel_detail() -> List[Tuple[str, int, int]]:
    """The same harvest, per sort: (shape, subqueries issued, kernels found).

    Read down the list to see where a classifier's residuals genuinely
    separate and where they only appeared to."""
    per: Dict[int, Tuple[Shape, set, int]] = {}
    for part in _SPLIT.values():
        if part is EMPTY:
            continue
        sh = part.shape
        prev = per.get(sh.uid)
        if prev is None:
            per[sh.uid] = (sh, {part.kernel}, 1)
        else:
            per[sh.uid] = (sh, prev[1] | {part.kernel}, prev[2] + 1)
    return [(repr(sh), n, len(ks)) for sh, ks, n in per.values()]
