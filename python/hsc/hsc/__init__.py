"""hsc — Hierarchical Shape Calculus prototype. See ../README.md and ../algorithm.md."""

from .model import Model, enum
from .ops.combinators import ID, compose, star, sum_of
from .classify.expr import (
    TRUE, FALSE, const, eq, ind, land, lnot, lor, lt, mod, plus, var,
)

__all__ = [
    "Model", "enum", "ID", "compose", "star", "sum_of",
    "var", "const", "plus", "ind", "eq", "lt", "land", "lor", "lnot", "mod",
    "TRUE", "FALSE",
]
