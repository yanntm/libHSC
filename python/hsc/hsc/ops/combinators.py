"""Combinators: identity, composition, sum, star.

`id` is free: it is the empty composition, and composing with it returns the
other operand unchanged rather than building a node. `0` is not represented
as a hom either — a term that annihilates simply returns `None` on data.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Tuple

from ..core.algebra import join
from ..core.stats import tick
from ..core.diagram import Diagram
from .hom import Hom
from ..core.shape import Shape


@dataclass(frozen=True)
class Identity(Hom):
    def _apply(self, shape: Shape, d: Diagram) -> Diagram:
        return d

    def support(self):
        return frozenset()

    def rerooted(self, bit: int) -> "Identity":
        return self


ID = Identity()


@dataclass(frozen=True)
class Compose(Hom):
    """`Compose(f_k, ..., f_1)` applies right to left, like the document's `o`."""

    terms: Tuple[Hom, ...]

    def support(self):
        return frozenset().union(*[t.support() for t in self.terms])

    def rerooted(self, bit: int) -> Hom:
        return type(self)(tuple(t.rerooted(bit) for t in self.terms))

    def _apply(self, shape: Shape, d: Diagram) -> Diagram:
        out = d
        for f in reversed(self.terms):
            out = f(shape, out)
            if out is None:
                return None
        return out


@dataclass(frozen=True)
class Sum(Hom):
    """Nondeterministic choice: the join of the branches' results."""

    terms: Tuple[Hom, ...]

    def support(self):
        return frozenset().union(*[t.support() for t in self.terms])

    def rerooted(self, bit: int) -> Hom:
        return type(self)(tuple(t.rerooted(bit) for t in self.terms))

    def _apply(self, shape: Shape, d: Diagram) -> Diagram:
        out: Diagram = None
        for f in self.terms:
            out = join(shape, out, f(shape, d))
        return out


@dataclass(frozen=True)
class Star(Hom):
    """Least fixpoint of `X |-> d + h(X)`.

    The per-run certificate is the pointer test: hash-consing makes the
    fixpoint check `is`, and monotonicity makes every fair schedule agree.
    Scheduling therefore lives strictly below the semantics."""

    term: Hom

    def _apply(self, shape: Shape, d: Diagram) -> Diagram:
        cur = d
        while True:
            tick("node.star_round")
            nxt = join(shape, cur, self.term(shape, cur))
            if nxt is cur:
                return cur
            cur = nxt


def compose(*terms: Hom) -> Hom:
    flat: list = []
    for t in terms:
        if isinstance(t, Identity):
            continue  # the free unit is never represented
        if isinstance(t, Compose):
            flat.extend(t.terms)
        else:
            flat.append(t)
    if not flat:
        return ID
    if len(flat) == 1:
        return flat[0]
    return Compose(tuple(flat))


def sum_of(*terms: Hom) -> Hom:
    flat: list = []
    for t in terms:
        if isinstance(t, Sum):
            flat.extend(t.terms)
        else:
            flat.append(t)
    if len(flat) == 1:
        return flat[0]
    return Sum(tuple(flat))


def star(term: Hom) -> Hom:
    return Star(term)
