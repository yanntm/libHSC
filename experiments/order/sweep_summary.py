#!/usr/bin/env python3
"""Summarise an order-sweep TSV (sweep_job.sh) — the "heuristics against each
other" table of SWEEP.md §2.1, in text.

    experiments/order/sweep_summary.py results/sweep1.tsv [--exam SS] [--top N]

Per heuristic: runs, answered, complete runs, wrong, statuses (timeout /
memory / crash / noreach), median and p90 wall and peak RSS, unique wins
(instances where it alone answers the most), marginal contribution (answers
the virtual best loses without it), and the heuristics that dominate it (at
least as many answers on every instance, strictly more on one). Then the
instances with the largest spread across heuristics.
"""
from __future__ import annotations

import argparse
import csv
import statistics
from collections import defaultdict
from typing import Dict, List, Optional, Tuple

Row = Dict[str, str]
Key = Tuple[str, str]


def num(s: Optional[str]) -> float:
    try:
        return float(s) if s not in (None, "") else 0.0
    except ValueError:
        return 0.0


def load(path: str, exam: Optional[str]) -> List[Row]:
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    rows = [r for r in rows if r.get("heuristic") and r["heuristic"] != "-" and r.get("instance")
            and r.get("exam") in ("SS", "CTLC", "CTLF", "RC", "RF", "RD") and r.get("status")]  # torn lines dropped
    if exam:
        rows = [r for r in rows if r["exam"] == exam]
    return rows


def resolve_dups(rows: List[Row]) -> List[Row]:
    """A dup:<name> row takes the answers of the heuristic it duplicates."""
    by: Dict[Tuple[Key, str], Row] = {((r["instance"], r["exam"]), r["heuristic"]): r for r in rows}
    out: List[Row] = []
    for r in rows:
        if r["status"].startswith("dup:"):
            src = by.get(((r["instance"], r["exam"]), r["status"][4:]))
            if src is not None:
                r = dict(r)
                for k in ("answered", "ok", "wrong", "unknown", "complete", "wall_s", "maxrss_kb"):
                    r[k] = src.get(k, "")
                r["status"] = "dup"
        out.append(r)
    return out


def quantile(xs: List[float], p: float) -> float:
    if not xs:
        return 0.0
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(p * len(xs)))]


def summarise(rows: List[Row], top: int) -> str:
    rows = resolve_dups(rows)
    heur = sorted({r["heuristic"] for r in rows})
    inst = sorted({(r["instance"], r["exam"]) for r in rows})
    ans: Dict[Key, Dict[str, int]] = defaultdict(dict)
    for r in rows:
        ans[(r["instance"], r["exam"])][r["heuristic"]] = int(num(r["answered"]))
    # only instances every heuristic has run, for the comparisons
    full = [k for k in inst if len(ans[k]) == len(heur)]
    best = {k: max(ans[k].values()) for k in full}
    vbest = sum(best.values())
    lines = [f"{len(rows)} runs, {len(inst)} (instance, exam) pairs, {len(full)} with every heuristic; "
             f"virtual best over those: {vbest} answered"]
    hdr = f"{'heuristic':18} {'runs':>5} {'answ':>6} {'compl':>5} {'wrong':>5} {'tmo':>4} {'mem':>4} {'crash':>5} {'norch':>5} " \
          f"{'wall50':>7} {'wall90':>7} {'rss50G':>6} {'rss90G':>6} {'uniq':>4} {'margin':>6}  dominated by"
    lines.append(hdr)
    for h in heur:
        hr = [r for r in rows if r["heuristic"] == h]
        st = defaultdict(int)
        for r in hr:
            st[r["status"]] += 1
        walls = [num(r["wall_s"]) for r in hr if r["wall_s"]]
        rss = [num(r["maxrss_kb"]) / 1e6 for r in hr if r["maxrss_kb"]]
        uniq = sum(1 for k in full if ans[k][h] == best[k] and sum(1 for g in heur if ans[k][g] == best[k]) == 1)
        without = sum(max(v for g, v in ans[k].items() if g != h) for k in full) if len(heur) > 1 else 0
        dominated = [g for g in heur if g != h and all(ans[k][g] >= ans[k][h] for k in full)
                     and any(ans[k][g] > ans[k][h] for k in full)]
        lines.append(f"{h:18} {len(hr):5} {sum(int(num(r['answered'])) for r in hr):6} "
                     f"{sum(1 for r in hr if r['complete'] == '1'):5} {sum(int(num(r['wrong'])) for r in hr):5} "
                     f"{st['timeout']:4} {st['memory']:4} {st['crash']:5} {st['noreach']:5} "
                     f"{quantile(walls, .5):7.1f} {quantile(walls, .9):7.1f} {quantile(rss, .5):6.2f} {quantile(rss, .9):6.2f} "
                     f"{uniq:4} {vbest - without:6}  {' '.join(dominated)}")
    spread = sorted(((max(ans[k].values()) - min(ans[k].values()), k) for k in full), reverse=True)[:top]
    if spread:
        lines.append(f"largest spread (max - min answered across heuristics), top {top}:")
        for d, k in spread:
            cells = " ".join(f"{h}={ans[k][h]}" for h in heur if ans[k][h] == best[k])
            lines.append(f"  {d:3} {k[0]} {k[1]}: best {cells}")
    return "\n".join(lines)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("tsv")
    ap.add_argument("--exam", default=None)
    ap.add_argument("--top", type=int, default=12)
    a = ap.parse_args()
    print(summarise(load(a.tsv, a.exam), a.top))


if __name__ == "__main__":
    main()
