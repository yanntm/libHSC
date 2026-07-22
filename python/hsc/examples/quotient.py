"""The quotient constructor at work: discovered alphabets and curried transport.

Two runs.

`#eaters` on the philosophers' reachable set classifies against a counting
expression whose codomain is unbounded, and discovers the three letters the
system actually realises. Empty classes are never represented and parts that
agree are merged before any client sees them, so the alphabet is minimal and
forced rather than declared.

The residue example isolates currying. The classifier `(b+c) mod 3` is
supported at two leaves with junk coordinates between them; it curries at
the first, travels ground through the junk, and folds at the second. Both numbers it reports are independent of the carrier's range: the
classifier curries to three residual codes at the first coordinate whatever
the range, and the subqueries below collapse onto a single kernel, differing
only by a relabelling. That collapse is the merge finding structure, not a
mode anyone selected.
"""

from __future__ import annotations

from typing import Any, Dict, List

from hsc import Model, const, enum, eq, ind, mod, plus, star, sum_of, var
from hsc.classify.query import clear_cache, kernel_detail, kernel_report
from hsc.core.stats import bill, reset

from examples import philosophers as ph


def eaters() -> None:
    n = 4
    m = ph.build(n)
    step = sum_of(*[r for _, _, r in ph.rules(m, n)])
    x = m.apply(star(step), ph.initial(m, n))

    counter = plus(*[ind(eq(var(f"S{i}"), const(ph.EAT))) for i in range(1, n + 1)])
    parts = m.theta(x, counter)

    print("--- #eaters over the reachable set ---")
    print("codomain declared  : unbounded (a counting expression)")
    print("alphabet discovered:", sorted(parts))
    for letter in sorted(parts):
        print(f"  {letter} eaters : {m.count(parts[letter]):4d} words, "
              f"{sum(m.size(parts[letter]).values()):3d} primes")
    assert sorted(parts) == [0, 1, 2], "a ring of four seats at most two eaters"
    assert sum(m.count(p) for p in parts.values()) == m.count(x), "the parts partition"


def residue(rng: int) -> Dict[str, Any]:
    """Shape (<b>,(<j1>,(<j2>,<c>))): the classifier's two coordinates are
    separated by junk that contributes nothing and is never re-queried."""
    leaves = {
        "b": enum("b", *range(rng)),
        "j1": enum("j1", 0, 1),
        "j2": enum("j2", 0, 1),
        "c": enum("c", *range(rng)),
    }
    m = Model(("b", ("j1", ("j2", "c"))), leaves)
    d = m.cube(b=range(rng), j1=(0, 1), j2=(0, 1), c=range(rng))
    e = mod(plus(var("b"), var("c")), 3)

    reset()
    clear_cache()
    parts = m.theta(d, e)
    invoice = bill()
    harvest = kernel_report()
    groups = leaves["b"].split_equiv(frozenset(range(rng)), e)

    return {
        "alphabet": sorted(parts),
        "counts": {k: m.count(v) for k, v in sorted(parts.items())},
        "residuals_at_b": len(groups),
        "classes_at_b": sorted((sorted(v)[:4], len(v)) for v in groups.values()),
        "subqueries": invoice.get("node.split_equiv", 0),
        "harvest": harvest,
        "detail": kernel_detail(),
    }


def main() -> None:
    eaters()

    print("\n--- (b+c) mod 3, curried through junk coordinates ---")
    for rng in (10, 30, 90):
        r = residue(rng)
        h = r["harvest"]
        print(f"range {rng:2d}  alphabet={r['alphabet']} "
              f"residuals@b={r['residuals_at_b']:2d} subqueries={r['subqueries']:3d} "
              f"parts={h['partitions']:3d} kernels={h['kernels']:2d} "
              f"separating={h['separating']:2d} merged={h['merged']:3d} "
              f"counts={list(r['counts'].values())}")

    print("\nper-sort harvest, range 30 (sort, subqueries, kernels):")
    for row in residue(30)["detail"]:
        print("   ", row)

    ten = residue(10)
    print("\nclasses at <b>, range 10 (first four members, size):", ten["classes_at_b"])
    assert ten["alphabet"] == [0, 1, 2]
    assert all(residue(r)["residuals_at_b"] == 3 for r in (10, 30, 90)), \
        "the classifier must curry to three codes at any range"
    assert len({residue(r)["harvest"]["kernels"] for r in (10, 30, 90)}) == 1, \
        "the kernel count is an invariant of the classifier, not of the range"


if __name__ == "__main__":
    main()
