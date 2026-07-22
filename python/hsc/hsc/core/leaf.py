"""The leaf interface: an external support algebra, seen through a finite window.

A leaf exports canonical hashable *codes* for the elements of a family of
subsets of its carrier, plus the operations of its declared tier:

    E : codes, equality, emptiness
    J : E + finite joins
    G : J + finite meets and *relative* differences (no top required)
    B : G + top and complement

The kernel calls only what the operation at hand consumes, and never calls
above the declared tier. There is no absolute or relative preimage in this
interface and none is planned: undoing a destructive assignment needs a
reachable set to move within, which is a different problem.

`split_equiv` is the partition primitive the elementary layer and the
quotient constructor share; it is not yet called by the kernel.
"""

from __future__ import annotations

from typing import Any, Dict, Iterable

Code = Any


class TierError(NotImplementedError):
    """Raised when an operation demands a tier the leaf did not declare."""


class Leaf:
    """Base class for leaf modules. Subclasses declare `tier` and implement up to it."""

    tier: str = "E"
    name: str = "leaf"

    # ---- tier E -------------------------------------------------------
    def empty(self) -> Code:
        """The canonical code of the empty set."""
        raise TierError(f"{self.name}: empty (tier E)")

    def is_empty(self, a: Code) -> bool:
        raise TierError(f"{self.name}: is_empty (tier E)")

    def eq(self, a: Code, b: Code) -> bool:
        """Codes are canonical, so equality is code equality unless a leaf says otherwise."""
        return a == b

    # ---- tier J -------------------------------------------------------
    def join(self, a: Code, b: Code) -> Code:
        raise TierError(f"{self.name}: join (tier J)")

    # ---- tier G -------------------------------------------------------
    def meet(self, a: Code, b: Code) -> Code:
        raise TierError(f"{self.name}: meet (tier G)")

    def diff(self, a: Code, b: Code) -> Code:
        """Relative difference a \\ b. No top is involved and none may be assumed."""
        raise TierError(f"{self.name}: diff (tier G)")

    # ---- tier B (optional; the kernel never calls these) ---------------
    def top(self) -> Code:
        raise TierError(f"{self.name}: top (tier B)")

    def complement(self, a: Code) -> Code:
        raise TierError(f"{self.name}: complement (tier B)")

    # ---- the partition primitive (not yet used by the kernel) ----------
    def split_equiv(self, a: Code, expr: Any) -> Dict[Any, Code]:
        """Partition `a` by the value of `expr` on this leaf's coordinate.

        Returns realised value -> nonempty piece. Empty classes are absent.
        Where the residual is not ground, the value is a residual expression
        code and the caller curries on it.
        """
        raise TierError(f"{self.name}: split_equiv")

    # ---- extensional shadow (small leaves only; the oracle's door) -----
    def elements(self, a: Code) -> Iterable[Any]:
        raise TierError(f"{self.name}: elements (shadow only)")

    def __repr__(self) -> str:
        return f"{type(self).__name__}({self.name!r}, tier {self.tier})"
