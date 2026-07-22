"""Ex1 of the calculus document: dining philosophers on a balanced shape.

Fork F_i sits between philosopher i and i+1; philosopher i uses F_i (left)
and F_{i-1} (right), indices modulo n. The shape is balanced over the
philosophers, Ph_i = (<F_i>, <S_i>), so every cut of the tree is crossed by
exactly two forks. The document predicts four primes at every cut at any n:
representation linear, state space exponential.

Deadlock is computed without any inverse image: a word is deadlocked when
no rule's guard admits it, and guards are filters, so the deadlocked set is
`X` minus the join of the guards applied to `X`.
"""

from __future__ import annotations

from typing import Any, Dict, List, Tuple

from hsc import Model, compose, enum, star, sum_of
from hsc.core.algebra import diff, join
from hsc.ops.hom import Hom
from hsc.core.stats import bill, reset, summary

FREE, TK = "free", "tk"
THINK, HOLD, EAT = "T", "HL", "E"


def _balanced(items: List[Any]) -> Any:
    if len(items) == 1:
        return items[0]
    half = len(items) // 2
    return (_balanced(items[:half]), _balanced(items[half:]))


def build(n: int) -> Model:
    """`n` philosophers, `n` a power of two, on the balanced shape."""
    assert n >= 2 and n & (n - 1) == 0, "n must be a power of two"
    leaves: Dict[str, Any] = {}
    for i in range(1, n + 1):
        leaves[f"F{i}"] = enum(f"F{i}", FREE, TK)
        leaves[f"S{i}"] = enum(f"S{i}", THINK, HOLD, EAT)
    spec = _balanced([(f"F{i}", f"S{i}") for i in range(1, n + 1)])
    return Model(spec, leaves)


def rules(m: Model, n: int) -> List[Tuple[str, Hom, Hom]]:
    """(name, guard, whole rule) for each protocol step."""
    out: List[Tuple[str, Hom, Hom]] = []
    for i in range(1, n + 1):
        left, right = f"F{i}", f"F{i - 1 if i > 1 else n}"
        s = f"S{i}"

        g = compose(m.keep(s, THINK), m.keep(left, FREE))
        out.append((f"takeL{i}", g, compose(m.set(**{s: HOLD, left: TK}), g)))

        g = compose(m.keep(s, HOLD), m.keep(right, FREE))
        out.append((f"takeR{i}", g, compose(m.set(**{s: EAT, right: TK}), g)))

        g = m.keep(s, EAT)
        out.append(
            (f"drop{i}", g, compose(m.set(**{s: THINK, left: FREE, right: FREE}), g))
        )
    return out


def initial(m: Model, n: int):
    return m.word(**{f"F{i}": FREE for i in range(1, n + 1)},
                  **{f"S{i}": THINK for i in range(1, n + 1)})


def deadlocked(m: Model, rs: List[Tuple[str, Hom, Hom]], x):
    """X minus everything some guard admits. No inverse image is involved."""
    live = None
    for _, guard, _rule in rs:
        live = join(m.shape, live, m.apply(guard, x))
    return diff(m.shape, x, live)


def run(n: int) -> None:
    m = build(n)
    rs = rules(m, n)
    step = sum_of(*[r for _, _, r in rs])

    reset()
    x = m.apply(star(step), initial(m, n))
    invoice = summary()

    dead = deadlocked(m, rs, x)
    print(f"--- {n} philosophers ---")
    print("reachable words   :", m.count(x))
    print("primes per depth  :", m.size(x))
    print("primes at top cut :", m.root_primes(x))
    print("total primes      :", sum(m.size(x).values()))
    print("deadlocked words  :", m.count(dead))
    print("ops during star   :", invoice)
    if n == 4:
        allhold = m.word(**{f"F{i}": TK for i in range(1, 5)},
                         **{f"S{i}": HOLD for i in range(1, 5)})
        assert m.count(diff(m.shape, allhold, dead)) == 0, "all-HL must be deadlocked"


def main() -> None:
    for n in (2, 4, 8, 16):
        run(n)


if __name__ == "__main__":
    main()
