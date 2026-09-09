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
  appear; the theory contract exports no preimage. The theory does offer
  `invert_local` (`core/algorithm.md` §9): the converse of a term restricted
  to a finite domain, spelled in the same language plus the one action a
  converse needs and a model does not — `choose(S)`, `x := any value of a
  set`, of which the range havoc is the interval case.
  It also composes two of its terms into one (`term_compose`,
  `core/algorithm.md` §11): guards conjoin, the second read after the
  first action, actions compose — what lets a constrained closure keep its
  product events and saturate.

## Pending

The FDD / bounded-integer theory backed by the legacy flat engine's storage
(M6, the parity gate). LIA + arrays as the first interchange theory (M4).
ω-word recognizers (M8).
