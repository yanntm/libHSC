"""Homomorphisms driven by a classifier: guards, computed assigns, `case`.

Each is `split_equiv` plus a decision about what to do with the pieces, and
the difference between them is exactly the codomain of the classifier and
the action on the pieces. Nothing here re-implements the traversal.

`case` invokes each branch **once per congruence class**, not once per
element and not once per declared letter — the branch set is discovered,
minimal and canonical, which is the whole content of running control flow
through the quotient constructor.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, Optional, Tuple

from ..core.algebra import join
from ..core.diagram import Diagram
from ..classify.expr import TRUE, Expr
from .hom import Hom
from .local import Write
from ..classify.query import split_equiv, theta
from ..core.shape import Path, Shape


@dataclass(frozen=True)
class Guard(Hom):
    """A predicate over any number of coordinates: keep the accepting piece.

    Over one coordinate this does not travel and is the plain filter. Over
    several it curries, which is why writing a conjunction as a composition
    of one-variable guards is cheaper: those never travel at all."""

    expr: Expr

    def _apply(self, shape: Shape, d: Diagram) -> Diagram:
        return split_equiv(shape, d, self.expr).get(TRUE)


@dataclass(frozen=True)
class Put(Hom):
    """`assign(path := expr)`: a computed write.

    Split by the value of the right-hand side, write the constant that value
    is on each piece, and re-fuse. Destructive, so pieces may collide on the
    way back; the join is what merges them."""

    path: Path
    expr: Expr

    def _apply(self, shape: Shape, d: Diagram) -> Diagram:
        out: Diagram = None
        for value, piece in theta(shape, d, self.expr).items():
            written = Write(self.path, frozenset((value,)))(shape, piece)
            out = join(shape, out, written)
        return out


@dataclass(frozen=True)
class Case(Hom):
    """Data-dependent control: quotient, act per class, re-fuse.

    Letters with no branch are dropped unless `default` is given; a branch
    with no realised letter is never invoked, because the alphabet is
    discovered from the data rather than declared by the term."""

    expr: Expr
    branches: Tuple[Tuple[Any, Hom], ...]
    default: Optional[Hom] = None

    def _apply(self, shape: Shape, d: Diagram) -> Diagram:
        table: Dict[Any, Hom] = dict(self.branches)
        out: Diagram = None
        for value, piece in theta(shape, d, self.expr).items():
            branch = table.get(value, self.default)
            if branch is None:
                continue
            out = join(shape, out, branch(shape, piece))
        return out
