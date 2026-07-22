"""The invoice: call counters for every operation that costs something.

The cost claim of the calculus document is that a query's bill factors as
(distinct realised residuals) x (levels crossed) x (class-code size). That
is something a prototype must *measure*, not assume, so the counters exist
from the first commit and every claim about cost is expected to cite them.

Counters are named `<sort>.<op>`, where the sort is `node` for operations
the kernel performs at a cut and the leaf's name for operations that cross
the leaf interface. Keeping the two apart is the point: at composite shapes
most of the work is the kernel's own recursion, and a bill that reports only
leaf traffic under-reports it.
"""

from __future__ import annotations

from typing import Dict

COUNTERS: Dict[str, int] = {}


def tick(name: str) -> None:
    COUNTERS[name] = COUNTERS.get(name, 0) + 1


def bill() -> Dict[str, int]:
    """All counters since the last reset."""
    return dict(COUNTERS)


def summary() -> Dict[str, int]:
    """Totals split by side of the leaf interface."""
    node = sum(v for k, v in COUNTERS.items() if k.startswith("node."))
    return {"node": node, "leaf": sum(COUNTERS.values()) - node}


def reset() -> None:
    COUNTERS.clear()
