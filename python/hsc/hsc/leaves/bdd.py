"""BddLeaf: a block of Boolean bits, backed by BuDDy.

**Currified variables.** Every leaf of this theory uses the *same* BuDDy
variables, indexed from the terminal upward: bit 0 nearest the terminal, bit
1 above it, and so on. One set of variables is ever instantiated in the
manager, whatever the shape and however many positions use it. A leaf of
width k uses the first k of them.

That is not a space optimisation, it is the "no names" discipline: positions
are paths, and two equal subterms are one object. Because codes are relative
rather than absolute, a code built for one position *is* the code for every
other position with the same local structure -- four philosophers holding
the same local state share one BuDDy node, and the manager never learns that
there are four of them. A fixed global variable order cannot express that;
this is where the calculus parts company with vtree-based forms, in which
every variable occurs at exactly one place.

**Codes are BuDDy `bdd` objects directly.** They are value-equal and
value-hashable (`__hash__` tracks the canonical node index, `false` = 0,
`true` = 1), which is exactly the code contract. Holding the Python proxy
holds a reference on the node, so codes the kernel stores cannot be
collected under it.

**Manager hazard.** `bdd_init` called twice aborts the process rather than
raising, and importing Spot initialises BuDDy. Everything here goes through
`ensure_manager`, which checks `bdd_isrunning` first; nothing in this module
may call `bdd_init` unguarded.

Not implemented yet: `split_equiv`. For this theory it is co-factoring --
partition a code by the distinct residuals of a classifier over the block,
which is a decomposition of the classifier at the cut (block | rest) and so
is the calculus's own theorem one level down. It needs the classifier
language to admit BDD-valued terms first, which is a design step rather than
a wrapper step.
"""

from __future__ import annotations

import itertools
from typing import Any, Dict, Iterable, List, Sequence, Tuple

import buddy

from ..core.leaf import Leaf, TierError
from ..core.stats import tick

Code = Any  # a buddy.bdd

_VARS: List[int] = []


def ensure_manager(nodes: int = 1 << 20, cache: int = 1 << 16) -> None:
    """Initialise BuDDy unless someone already did. Never call `bdd_init`
    directly: a second call aborts the process instead of raising."""
    if not buddy.bdd_isrunning():
        buddy.bdd_init(nodes, cache)


def _shared_vars(width: int) -> Tuple[int, ...]:
    """The one variable block, extended on demand and shared by every leaf."""
    ensure_manager()
    while len(_VARS) < width:
        _VARS.append(buddy.bdd_extvarnum(1))
    return tuple(_VARS[:width])


class BddLeaf(Leaf):
    """A finite Boolean carrier of `bits`, coded by BDDs over the shared block."""

    tier = "B"

    def __init__(self, name: str, bits: Sequence[str]) -> None:
        self.name = name
        self.bits: Tuple[str, ...] = tuple(bits)
        self.vars: Tuple[int, ...] = _shared_vars(len(self.bits))
        self._index: Dict[str, int] = {b: i for i, b in enumerate(self.bits)}

    # ---- tier E
    def empty(self) -> Code:
        return buddy.bddfalse

    def is_empty(self, a: Code) -> bool:
        tick(f"{self.name}.is_empty")
        return a == buddy.bddfalse

    # ---- tier J
    def join(self, a: Code, b: Code) -> Code:
        tick(f"{self.name}.join")
        return a | b

    # ---- tier G
    def meet(self, a: Code, b: Code) -> Code:
        tick(f"{self.name}.meet")
        return a & b

    def diff(self, a: Code, b: Code) -> Code:
        tick(f"{self.name}.diff")
        return a & buddy.bdd_not(b)

    # ---- tier B (present because BuDDy has it; the kernel never calls these)
    def top(self) -> Code:
        return buddy.bddtrue

    def complement(self, a: Code) -> Code:
        return buddy.bdd_not(a)

    # ---- code construction
    def lit(self, bit: str, value: bool = True) -> Code:
        v = self.vars[self._index[bit]]
        return buddy.bdd_ithvar(v) if value else buddy.bdd_nithvar(v)

    def cube(self, **bits: bool) -> Code:
        """The conjunction of the named bits; unnamed bits are unconstrained."""
        out = buddy.bddtrue
        for bit, value in bits.items():
            out = out & self.lit(bit, value)
        return out

    # ---- exported maps (the assigns of the elementary layer)
    def act(self, code: Code, op: Tuple[Any, ...]) -> Code:
        """Apply an exported map. `("set", bit, value)` writes one bit.

        Writing a bit is `exists bit . code`, then conjoin the new value: the
        pushforward of a partial assignment, which is memory-destructive
        exactly as an assign should be."""
        tick(f"{self.name}.act")
        kind = op[0]
        if kind == "set":
            _, bit, value = op
            v = self.vars[self._index[bit]]
            return buddy.bdd_exist(code, buddy.bdd_ithvar(v)) & self.lit(bit, value)
        raise TierError(f"{self.name}: unknown exported map {op!r}")

    # ---- the partition primitive
    def split_equiv(self, a: Code, expr: Any) -> Dict[Any, Code]:
        raise TierError(
            f"{self.name}: split_equiv -- co-factoring against a classifier over "
            "this block is the next step; it needs BDD-valued classifier terms"
        )

    # ---- shadow (small blocks only; nothing in the kernel calls this)
    def elements(self, a: Code) -> Iterable[Tuple[bool, ...]]:
        out: List[Tuple[bool, ...]] = []
        for combo in itertools.product((False, True), repeat=len(self.bits)):
            if (a & self.cube(**dict(zip(self.bits, combo)))) != buddy.bddfalse:
                out.append(combo)
        return out

    def __repr__(self) -> str:
        return f"BddLeaf({self.name!r}, bits={self.bits}, vars={self.vars})"


def manager_nodes() -> int:
    """Live nodes in the shared manager: the sharing, made visible."""
    return buddy.bdd_getnodenum()
