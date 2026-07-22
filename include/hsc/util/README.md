# `util/` — primitives

No libHSC concept appears here. Anything in this folder could be lifted into
another project unchanged.

* `hash.hh` — the hashing customization point and the sanctioned combiners.
  - `mix64` (splitmix64 finalizer), `mix32` (Wang's 32-bit hash, kept from
    libDDD `ddd/hashfunc.hh`), `mix_ptr` (Knuth multiplicative, for
    addresses). Identifiers are dense small integers with no entropy in
    their high bits, so nothing reaches a table unmixed.
  - `combine(seed, value)` — **the** combiner, non-commutative. It
    re-avalanches the accumulated seed at each step, so a permutation of a
    composite's members changes its hash. The legacy XOR-of-hashes for pairs
    (`ddd/util/hash_support.hh:96`) made every transposition collide; it is
    banned here, and `tests/test_hash.cc` holds the guard.
  - `hash_value(x)` — the customization point. Resolution order: a member
    `hash()` (how a libHSC type opts in), then the mixers for integral,
    enumeration and pointer types, then `std::hash`. One function, no
    specialization forest.
  - `hash_combine`, `hash_range`, `hash_all` — the composite forms.
    `hash_all` seeds with the argument count so composites of different
    arity do not collide through a common prefix.
