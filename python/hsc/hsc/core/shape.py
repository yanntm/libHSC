"""Shapes: interned binary trees over leaf modules.

    V ::= <A> | (V_h, V_t)

A position in a shape is a path: a tuple of bits, 0 = head, 1 = tail, read
from the root. Structurally equal shapes are one object, so `is` decides
shape equality and `uid` orders shapes.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Dict, Iterator, List, Optional, Tuple

if TYPE_CHECKING:
    from .leaf import Leaf

Path = Tuple[int, ...]

_TABLE: Dict[object, "Shape"] = {}
_NEXT_UID: List[int] = [1]


class Shape:
    """Base class. Instances are interned; never construct directly."""

    __slots__ = ("uid",)

    uid: int

    def _init_uid(self) -> None:
        self.uid = _NEXT_UID[0]
        _NEXT_UID[0] += 1

    def leaves(self) -> Iterator[Tuple[Path, "LeafShape"]]:
        """Frontier positions, left to right."""
        raise NotImplementedError

    def at(self, path: Path) -> "Shape":
        """The subshape reached by following `path` from here."""
        raise NotImplementedError


class Unit(Shape):
    """The unit sort `1`, at one shape only.

    It has no frontier, so no position addresses it and no leaf backs it.
    It is where classification values live: the codomain of the default
    classifier, which is acceptance -- the value a word earns by reaching
    the terminal."""

    __slots__ = ()

    def __init__(self) -> None:
        self._init_uid()

    def leaves(self):
        return iter(())

    def at(self, path: Path) -> Shape:
        if path:
            raise KeyError("path runs past the unit sort")
        return self

    def __repr__(self) -> str:
        return "1"


class LeafShape(Shape):
    """An imported support algebra, under a name unique within a shape."""

    __slots__ = ("name", "leaf")

    def __init__(self, name: str, leaf: "Leaf") -> None:
        self.name = name
        self.leaf = leaf
        self._init_uid()

    def leaves(self) -> Iterator[Tuple[Path, "LeafShape"]]:
        yield ((), self)

    def at(self, path: Path) -> Shape:
        if path:
            raise KeyError(f"path runs past leaf {self.name}")
        return self

    def __repr__(self) -> str:
        return f"<{self.name}>"


class Pair(Shape):
    """A cut: head over `V_h`, tail over `V_t`."""

    __slots__ = ("head", "tail")

    def __init__(self, head: Shape, tail: Shape) -> None:
        self.head = head
        self.tail = tail
        self._init_uid()

    def leaves(self) -> Iterator[Tuple[Path, "LeafShape"]]:
        for p, lf in self.head.leaves():
            yield ((0,) + p, lf)
        for p, lf in self.tail.leaves():
            yield ((1,) + p, lf)

    def at(self, path: Path) -> Shape:
        if not path:
            return self
        child = self.head if path[0] == 0 else self.tail
        return child.at(path[1:])

    def __repr__(self) -> str:
        return f"({self.head!r},{self.tail!r})"


UNIT = Unit()


def leaf_shape(name: str, leaf: "Leaf") -> LeafShape:
    key = ("leaf", name, id(leaf))
    got = _TABLE.get(key)
    if got is None:
        got = _TABLE[key] = LeafShape(name, leaf)
    return got


def pair(head: Shape, tail: Shape) -> Pair:
    key = ("pair", head.uid, tail.uid)
    got = _TABLE.get(key)
    if got is None:
        got = _TABLE[key] = Pair(head, tail)
    return got  # type: ignore[return-value]


def paths_of(shape: Shape) -> Dict[str, Path]:
    """Leaf name -> frontier path. Names must be unique within the shape."""
    out: Dict[str, Path] = {}
    for path, lf in shape.leaves():
        if lf.name in out:
            raise ValueError(f"duplicate leaf name {lf.name!r} in {shape!r}")
        out[lf.name] = path
    return out


def leaf_at(shape: Shape, path: Path) -> LeafShape:
    got = shape.at(path)
    if not isinstance(got, LeafShape):
        raise TypeError(f"path {path} does not reach a leaf of {shape!r}")
    return got
