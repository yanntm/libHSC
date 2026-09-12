# Original transition enabling over a reduced diagram

An in-memory consumer of PetriSpot's `reduction/counting/Enabling.h`. The
specification is maintained in PetriSpot's `reduction/counting/algorithm.md`
and vendored beside the record. No PNET block or general transformation trace
is introduced. Only directly loaded PNML has the complete baseline ancestry.

`count.hh` compiles original presets to sparse component demands, accounts for
fixed groups, and asks the surface's `count-enabled` query for live groups.
The existing PCONST factor is not applied a second time. Identical queries
share exact results. The optional HSC_ENABLING_TRACE diagnostic prints counts
by original transition name for comparison with an unreduced run.

The surface query is implemented in `src/surface_enabled_count.cc`. Its DD
memo is scoped to one demand/weight environment and keyed by shape, offset
and canonical handle. It counts existing correlated arcs, not marginal domains.

## Real-model validation

The bounded comparison script `tests/enabling_models.py` checks each original
transition against the unreduced selector/count implementation and checks all
four StateSpace values against the existing MCC oracle. Five formerly
three-answer campaign instances pass: AutonomousCar-PT-01b (132 transitions),
FlexibleBarrier-PT-14a (578), DLCround-PT-04a (823), HealthRecord-PT-03 (232),
and GPPP-PT-C0001N0000000001 (22). All 1787 individual counts agree.

Diffusion2D-PT-D05N010 reduces to an empty net with one fixed free component.
The reconstructed TRANSITIONS value is 5553662400 and all four values match
the oracle. Its unreduced run exhausts the short comparison budget, so no
per-transition differential claim is made for that input.

Existing exact-count, leaf-weight and constant-component checks also pass.
Logs and full per-transition comparisons are archived under
`/data/ythierry/MCC26logs/local/enabling/`. No cluster or campaign deployment
is part of this prototype.
