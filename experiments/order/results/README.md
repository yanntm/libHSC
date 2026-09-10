# The order sweep's record

`sweep1.tsv.gz` — the merged rows of the first sweep, 40 614 of them: 1692
of 4344 jobs ran (981 SS, 711 CTLC, 0 CTLF) before `tall%` filled with
other users' whole-node jobs and the rest were cancelled. One row per
(instance, examination, heuristic); the columns are those of
`sweep_job.sh`, and mind that the `columns` header names 31 of the 33 —
`reach_states` and `partial` are the last two and unnamed (`SWEEP.md`
§9.3).

`sweep1_summary.txt` — `sweep_summary.py` over it, the table §9 is read
from.

The per-run logs (`.out`, `.err`, `.shape`, 615 MB) are not here; they are
in `/data/ythierry/MCC26logs/hsc/sweep1/sweep1/`, reaped from
`cluster.lip6.fr:~/MCC26/hsc-sweep/results/sweep1/`.

Regenerate the summary with:

```
experiments/order/sweep_summary.py <(zcat experiments/order/results/sweep1.tsv.gz)
```
