"""Ex1 again, with BDD leaves: one leaf per philosopher, three bits each.

Each philosopher is a single leaf carrying its own local state as a BDD over
three bits -- `f` (the fork on my left is taken), `hl` (I hold my left fork),
`e` (I am eating, holding both). The fork on my right is the *neighbour's*
`f`, so the protocol's cross-cut steps guard and write two leaves.

The point of the run is the currification. Every leaf uses the same three
BuDDy variables, so a local state is one node whatever position holds it:
four philosophers in the same local state share one node, and the manager
never learns there are four of them. Compare with the enum encoding, which
spreads the same system over eight leaves and cannot share anything between
positions.

Cross-check: the state space is the one `philosophers.py` builds, so the
reachable count must agree with it exactly, computed through a different
theory over a different shape.
"""

from __future__ import annotations

from typing import Any, Dict, List, Tuple

from hsc import Model, compose, star, sum_of
from hsc.core.algebra import diff, join
from hsc.ops.hom import Hom
from hsc.core.stats import reset, summary
from hsc.leaves.bdd import BddLeaf, manager_nodes
from hsc.ops.local import Act, Filter

BITS = ("f", "hl", "e")


def build(n: int) -> Model:
    """`n` philosophers, `n` a power of two, on the balanced shape."""
    assert n >= 2 and n & (n - 1) == 0
    leaves = {f"p{i}": BddLeaf(f"p{i}", BITS) for i in range(1, n + 1)}
    return Model(_balanced([f"p{i}" for i in range(1, n + 1)]), leaves)


def _balanced(items: List[Any]) -> Any:
    if len(items) == 1:
        return items[0]
    half = len(items) // 2
    return (_balanced(items[:half]), _balanced(items[half:]))


def rules(m: Model, n: int) -> List[Tuple[str, Hom, Hom]]:
    """(name, guard, whole rule). Right fork of i is the neighbour's `f`."""
    out: List[Tuple[str, Hom, Hom]] = []
    for i in range(1, n + 1):
        me, nb = f"p{i}", f"p{i - 1 if i > 1 else n}"
        pm, pn = m.paths[me], m.paths[nb]
        leaf = m.leaves[me]

        g = Filter(pm, leaf.cube(hl=False, e=False, f=False))
        out.append((f"takeL{i}", g, compose(
            Act(pm, ("set", "f", True)), Act(pm, ("set", "hl", True)), g)))

        g = compose(Filter(pn, m.leaves[nb].cube(f=False)),
                    Filter(pm, leaf.cube(hl=True, e=False)))
        out.append((f"takeR{i}", g, compose(
            Act(pn, ("set", "f", True)), Act(pm, ("set", "e", True)), g)))

        g = Filter(pm, leaf.cube(e=True))
        out.append((f"drop{i}", g, compose(
            Act(pn, ("set", "f", False)),
            Act(pm, ("set", "f", False)),
            Act(pm, ("set", "hl", False)),
            Act(pm, ("set", "e", False)), g)))
    return out


def initial(m: Model, n: int):
    return m.rect(**{f"p{i}": m.leaves[f"p{i}"].cube(f=False, hl=False, e=False)
                     for i in range(1, n + 1)})


def deadlocked(m: Model, rs, x):
    live = None
    for _, guard, _r in rs:
        live = join(m.shape, live, m.apply(guard, x))
    return diff(m.shape, x, live)


def run(n: int) -> None:
    m = build(n)
    rs = rules(m, n)
    before = manager_nodes()
    reset()
    x = m.apply(star(sum_of(*[r for _, _, r in rs])), initial(m, n))
    invoice = summary()
    print(f"--- {n} philosophers, BDD leaves ---")
    print("shape             :", m.shape)
    print("bdd variables used:", m.leaves["p1"].vars, "(shared by every leaf)")
    print("reachable words   :", m.count(x))
    print("primes per depth  :", m.size(x))
    print("total primes      :", sum(m.size(x).values()))
    print("deadlocked words  :", m.count(deadlocked(m, rs, x)))
    print("buddy nodes       :", manager_nodes(), f"(was {before})")
    print("ops during star   :", invoice)


def main() -> None:
    for n in (2, 4, 8):
        run(n)


if __name__ == "__main__":
    main()
