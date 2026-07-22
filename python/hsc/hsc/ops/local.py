"""Local homomorphisms: filters and parallel constant assigns.

Both address frontier positions. The recursion is the point: a local hom at
a cut *is the same hom one level down*, applied to the primes when the path
turns into the head and to the subs when it turns into the tail. Recursing
by re-invoking the hom rather than by calling a helper is what keeps the
traversal shared — the diagram is a DAG, and a hom that recursed through
plain functions would walk its tree unfolding instead, which costs the
representation's sharing exactly when the sharing is the point.

No cylinder is ever built. A filter is not materialised as data over the
whole shape, which is why no top is needed anywhere: the two faces of a
support are kept apart on purpose, and the bridge between them is exactly
the tier-B purchase this kernel declines to make.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, List, Tuple

from ..core.algebra import normalize
from ..core.diagram import Diagram, Node, Rect
from .hom import Hom
from ..core.shape import LeafShape, Pair, Path, Shape


class _Descending(Hom):
    """Shared recursion for homs that address one frontier position."""

    path: Path

    def _at_leaf(self, shape: LeafShape, code: Any) -> Diagram:
        raise NotImplementedError

    def _below(self) -> "_Descending":
        """The same hom, one level down."""
        raise NotImplementedError

    def _apply(self, shape: Shape, d: Diagram) -> Diagram:
        if not self.path:
            assert isinstance(shape, LeafShape)
            return self._at_leaf(shape, d)
        assert isinstance(shape, Pair) and isinstance(d, Node)
        below = self._below()
        rects: List[Rect] = []
        if self.path[0] == 0:
            for p, s in d.pairs:
                q = below(shape.head, p)
                if q is not None:
                    rects.append((q, s))
        else:
            for p, s in d.pairs:
                t = below(shape.tail, s)
                if t is not None:
                    rects.append((p, t))
        return normalize(shape, rects)


@dataclass(frozen=True)
class Filter(_Descending):
    """Keep the words whose coordinate at `path` lies in `code`.

    A guard over one variable. Guards over several variables are written as
    a composition of these, one per variable; that is cheaper than their
    meet, which would have to travel."""

    path: Path
    code: Any

    def _at_leaf(self, shape: LeafShape, code: Any) -> Diagram:
        r = shape.leaf.meet(code, self.code)
        return None if shape.leaf.is_empty(r) else r

    def _below(self) -> "Filter":
        return Filter(self.path[1:], self.code)

    def support(self):
        return frozenset((self.path,))

    def rerooted(self, bit: int) -> "Filter":
        assert self.path and self.path[0] == bit
        return Filter(self.path[1:], self.code)


@dataclass(frozen=True)
class Write(_Descending):
    """Overwrite the coordinate at `path` with the constant `code`.

    Memory-destructive: previously distinct primes may collide, and the
    collision is precisely what re-normalisation merges."""

    path: Path
    code: Any

    def _at_leaf(self, shape: LeafShape, code: Any) -> Diagram:
        return None if shape.leaf.is_empty(self.code) else self.code

    def _below(self) -> "Write":
        return Write(self.path[1:], self.code)

    def support(self):
        return frozenset((self.path,))

    def rerooted(self, bit: int) -> "Write":
        assert self.path and self.path[0] == bit
        return Write(self.path[1:], self.code)


@dataclass(frozen=True)
class Act(_Descending):
    """Apply a map the leaf exports, at one position.

    `Write` is the special case whose map is a constant. A leaf whose
    carrier is structured -- a block of bits, a bounded integer -- writes
    part of its state and keeps the rest, which is a leaf-exported map and
    not a constant, so the general form is the one an assign needs."""

    path: Path
    op: Any

    def _at_leaf(self, shape: LeafShape, code: Any) -> Diagram:
        r = shape.leaf.act(code, self.op)
        return None if shape.leaf.is_empty(r) else r

    def _below(self) -> "Act":
        return Act(self.path[1:], self.op)

    def support(self):
        return frozenset((self.path,))

    def rerooted(self, bit: int) -> "Act":
        assert self.path and self.path[0] == bit
        return Act(self.path[1:], self.op)


@dataclass(frozen=True)
class Assign(Hom):
    """Parallel assign: write constants at several positions simultaneously.

    The vector form is the primitive; sequential writes cannot express a
    simultaneous exchange without temporaries. With constant right-hand
    sides the writes commute, so the order of `writes` is immaterial."""

    writes: Tuple[Tuple[Path, Any], ...]

    def support(self):
        return frozenset(p for p, _ in self.writes)

    def rerooted(self, bit: int) -> "Assign":
        assert all(p and p[0] == bit for p, _ in self.writes)
        return Assign(tuple((p[1:], c) for p, c in self.writes))

    def _apply(self, shape: Shape, d: Diagram) -> Diagram:
        out = d
        for path, code in self.writes:
            out = Write(path, code)(shape, out)
            if out is None:
                return None
        return out

