"""A 4-bit counter: the smallest end-to-end reachability run.

Four boolean leaves on a balanced shape. Increment is a sum of four guarded
parallel assigns, one per carry length. From 0000 the reachable set is all
sixteen words, which is checkable by hand, so this example exists to say
that the kernel is not lying rather than to say anything interesting.
"""

from __future__ import annotations

from hsc import Model, compose, enum, star, sum_of

BIT = ["b0", "b1", "b2", "b3"]


def build() -> Model:
    leaves = {b: enum(b, 0, 1) for b in BIT}
    shape = (("b0", "b1"), ("b2", "b3"))
    return Model(shape, leaves)


def increment(m: Model):
    """Rule k: bits 0..k-1 are 1 and bit k is 0; clear them and set bit k."""
    rules = []
    for k in range(len(BIT)):
        guard = compose(*[m.keep(BIT[j], 1) for j in range(k)], m.keep(BIT[k], 0))
        writes = {BIT[j]: 0 for j in range(k)}
        writes[BIT[k]] = 1
        rules.append(compose(m.set(**writes), guard))
    return sum_of(*rules)


def main() -> None:
    m = build()
    zero = m.word(b0=0, b1=0, b2=0, b3=0)
    reach = m.apply(star(increment(m)), zero)

    print("counter: reachable words =", m.count(reach), "(expect 16)")
    print("counter: primes per depth =", m.size(reach))
    print("counter: primes at top cut =", m.root_primes(reach))
    assert m.count(reach) == 16, "the 4-bit counter should reach every word"
    assert len(m.words(reach)) == 16


if __name__ == "__main__":
    main()
