"""Modelling sugar: declare named leaves, build a shape, address it by name.

Nothing here is part of the calculus. Positions are paths; names are a
decoration, consulted by exactly the constructs that address positions.
"""

from __future__ import annotations

from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple, Union

from .core.algebra import count, join, size
from .ops.branch import Case, Guard, Put
from .ops.combinators import ID, compose, star, sum_of
from .core.diagram import ONE, Diagram, Node
from .ops.hom import Hom
from .classify.expr import Expr
from .ops.local import Assign, Filter
from .classify.query import theta
from .core.shape import (
    UNIT, LeafShape, Pair, Path, Shape, Unit, leaf_shape, pair, paths_of,
)
from .leaves.enum import EnumLeaf

Spec = Union[str, int, Tuple["Spec", "Spec"], List["Spec"]]
"""A leaf name, the literal `1` for the unit sort, or a pair of specs."""


class Model:
    """A shape plus its name table plus the leaf modules behind it."""

    def __init__(self, spec: Spec, leaves: Dict[str, EnumLeaf]) -> None:
        self.leaves = leaves
        self.shape: Shape = self._build(spec)
        self.paths: Dict[str, Path] = paths_of(self.shape)

    def _build(self, spec: Spec) -> Shape:
        if spec == 1:
            return UNIT
        if isinstance(spec, str):
            return leaf_shape(spec, self.leaves[spec])
        head, tail = spec
        return pair(self._build(head), self._build(tail))

    # ---- data ---------------------------------------------------------
    def word(self, **values: Any) -> Diagram:
        """The singleton diagram at the given assignment of every leaf."""
        return self.cube(**{k: (v,) for k, v in values.items()})

    def rect(self, **codes: Any) -> Diagram:
        """A rectangle from explicit per-leaf codes, whatever the theory."""
        missing = set(self.paths) - set(codes)
        if missing:
            raise ValueError(f"rect needs every leaf; missing {sorted(missing)}")
        return self._rect(self.shape, codes)

    def _rect(self, shape: Shape, codes: Dict[str, Any]) -> Diagram:
        if isinstance(shape, Unit):
            return ONE
        if isinstance(shape, LeafShape):
            code = codes[shape.name]
            return None if shape.leaf.is_empty(code) else code
        assert isinstance(shape, Pair)
        h = self._rect(shape.head, codes)
        t = self._rect(shape.tail, codes)
        if h is None or t is None:
            return None
        from .core.diagram import mk

        return mk(shape, ((h, t),))

    def cube(self, **sets: Any) -> Diagram:
        """A rectangle: independently, each named leaf ranges over its set."""
        missing = set(self.paths) - set(sets)
        if missing:
            raise ValueError(f"cube needs every leaf; missing {sorted(missing)}")
        return self._cube(self.shape, sets)

    def _cube(self, shape: Shape, sets: Dict[str, Any]) -> Diagram:
        if isinstance(shape, Unit):
            return ONE
        if isinstance(shape, LeafShape):
            code = frozenset(sets[shape.name])
            return None if not code else code
        assert isinstance(shape, Pair)
        h = self._cube(shape.head, sets)
        t = self._cube(shape.tail, sets)
        if h is None or t is None:
            return None
        from .core.diagram import mk

        return mk(shape, ((h, t),))

    # ---- homs ---------------------------------------------------------
    def keep(self, name: str, *values: Any) -> Hom:
        """`filter(name in values)` — a guard over one variable."""
        return Filter(self.paths[name], self.leaves[name].code(*values))

    def set(self, **writes: Any) -> Hom:
        """Parallel constant assign: `assign(x := v, y := w)` simultaneously."""
        return Assign(
            tuple(
                (self.paths[n], self.leaves[n].code(v)) for n, v in sorted(writes.items())
            )
        )

    def guard(self, e: Expr) -> Hom:
        """A predicate over any number of coordinates. Over one coordinate
        prefer `keep`, which does not travel."""
        return Guard(e)

    def put(self, name: str, e: Expr) -> Hom:
        """`assign(name := e)`: a computed write."""
        return Put(self.paths[name], e)

    def case(self, e: Expr, branches: Dict[Any, Hom], default: Optional[Hom] = None) -> Hom:
        """Data-dependent control. Branches are invoked once per congruence
        class, and only for letters the data actually realises."""
        return Case(e, tuple(sorted(branches.items(), key=lambda kv: repr(kv[0]))), default)

    def apply(self, h: Hom, d: Diagram) -> Diagram:
        return h(self.shape, d)

    # ---- the quotient constructor -------------------------------------
    def theta(self, d: Diagram, e: Expr) -> Dict[Any, Diagram]:
        """Discovered letter -> the part of `d` it classifies."""
        return theta(self.shape, d, e)

    def alphabet(self, d: Diagram, e: Expr) -> List[Any]:
        return sorted(self.theta(d, e), key=repr)

    def residuals_at(self, d: Diagram, e: Expr, name: str) -> List[Any]:
        """The distinct residual codes `e` curries to at leaf `name` -- the
        first factor of the cost model, isolated and countable."""
        code = self._leaf_code(self.shape, d, self.paths[name])
        return sorted(self.leaves[name].split_equiv(code, e), key=repr)

    def _leaf_code(self, shape: Shape, d: Diagram, path: Path) -> Any:
        """The union of everything realised at `path`. A shadow convenience:
        the kernel never needs it."""
        if isinstance(shape, LeafShape):
            return d
        assert isinstance(shape, Pair)
        out = None
        for p, s in d.pairs:
            child = self._leaf_code(shape.head, p, path[1:]) if path[0] == 0 else \
                    self._leaf_code(shape.tail, s, path[1:])
            out = child if out is None else self.leaves[shape.at(path).name].join(out, child)
        return out

    # ---- measurement --------------------------------------------------
    def size(self, d: Diagram) -> Dict[int, int]:
        return size(self.shape, d)

    def count(self, d: Diagram) -> int:
        return count(self.shape, d)

    def words(self, d: Diagram) -> List[Dict[str, Any]]:
        """Brute-force expansion. The extensional shadow, for small carriers."""
        return [dict(w) for w in self._words(self.shape, d)]

    def _words(self, shape: Shape, d: Diagram) -> Iterable[Tuple[Tuple[str, Any], ...]]:
        if d is None:
            return
        if isinstance(shape, Unit):
            yield ()
            return
        if isinstance(shape, LeafShape):
            for v in shape.leaf.elements(d):
                yield ((shape.name, v),)
            return
        assert isinstance(shape, Pair) and isinstance(d, Node)
        for p, s in d.pairs:
            for wh in self._words(shape.head, p):
                for wt in self._words(shape.tail, s):
                    yield wh + wt

    def root_primes(self, d: Diagram) -> int:
        """Section rank at the top cut: the number of primes there."""
        return 0 if d is None else len(d.pairs)


def enum(name: str, *values: Any) -> EnumLeaf:
    return EnumLeaf(name, values)
