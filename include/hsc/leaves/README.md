# `leaves/` — imported theories

A leaf imports an external domain through a finite window. The window,
not the domain, is what the calculus sees: `core/support.hh` is that window.

* `int_set.hh` + `src/int_set.cc` — finite sets of integers. Sorted,
  duplicate-free runs, interned with the values trailing the header. The
  simplest thing satisfying tier G, deliberately: a theory earns its keep by
  returning few structured codes where enumeration would return many
  singletons, and this one does not try — which is what makes it the
  reference implementation and the permanent differential oracle for the ones
  that do.

  The empty set is `none`. It is never interned, because absence is not a
  citizen.

  Local terms are a guard followed by an action — `keep`,
  `assign`, `shift` — handed over whole and fused by the theory rather than
  split from outside. Small and closed on purpose: it covers a Petri
  transition (`m >= w` then `m -= w`) and a Hanoi move (`pos == a` then
  `pos := b`), which is the whole non-crossing fragment. Only pushforwards
  appear; the theory contract exports no preimage and none is wanted.

## Pending

The FDD / bounded-integer theory backed by the legacy flat engine's storage
(M6, the parity gate). LIA + arrays as the first interchange theory (M4).
ω-word recognizers (M8).
