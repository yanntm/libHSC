# Conjunction evaluator implementation

`filter.cc` implements the specialised API in
`include/hsc/linear/conjunction/filter.hh`. Its
[algorithm](../../../include/hsc/linear/conjunction/algorithm.md) defines the
support schedule, filtering of existing arcs by residual sums, memo keys, and
transactional interruption contract. Arithmetic is checked; all memo tables
are local to the invocation and share the manager's deadline.
