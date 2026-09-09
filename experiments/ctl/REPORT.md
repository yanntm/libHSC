# CTL thread — session report

Narrative of the unsupervised session (started after the M4 commit
2e11ecc), for the user to read at its end. Current state and next actions
stay in `handoff_ctl.md`; the numbers in `README.md` here.

## Starting point

`hsc-pn` answers CTLCardinality / CTLFireability by the forward form; 96/96
verdicts on ShieldRVt-PT-001A, AutoFlight-PT-01a, Raft-PT-02 against the
2026 oracle; Angiogenesis-PT-05 (42.7M states) answers one formula in 15 s.

## Actions

1. **Baseline benchmark** — `run_ctl_bench.sh` over the 294 instances with
   at most 10^7 states, binary of M4, 60 s cap, 8 side by side.
