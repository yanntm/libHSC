#!/usr/bin/env python3
"""Summarise CTL benchmark TSVs (run_ctl_bench.sh) and compare two of them.

    experiments/ctl/summarize.py results/a.tsv [results/b.tsv]

One TSV: totals (runs, answered, ok, wrong, unknown, complete files, runs over
the cap), wall time quantiles, the wrong verdicts listed. Two TSVs: per run
the answered counts side by side, the runs where they differ, the totals.
"""
from __future__ import annotations

import csv
import sys
from typing import Dict, List, Optional, Tuple

Row = Dict[str, str]


def load(path: str) -> Dict[Tuple[str, str], Row]:
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    return {(r["instance"], r["exam"]): r for r in rows if r.get("instance")}


def num(s: Optional[str]) -> float:
    try:
        return float(s) if s not in (None, "") else 0.0
    except ValueError:
        return 0.0


def totals(rows: Dict[Tuple[str, str], Row]) -> str:
    n = len(rows)
    ans = sum(int(num(r["answered"])) for r in rows.values())
    ok = sum(int(num(r["ok"])) for r in rows.values())
    wrong = sum(int(num(r["wrong"])) for r in rows.values())
    unk = sum(int(num(r["unknown"])) for r in rows.values())
    complete = sum(1 for r in rows.values() if int(num(r["unknown"])) == 0)
    timeouts = sum(1 for r in rows.values() if r["note"].strip() in ("rc=124",))
    walls = sorted(num(r["wall_s"]) for r in rows.values())
    q = lambda p: walls[min(len(walls) - 1, int(p * len(walls)))] if walls else 0.0
    lines = [
        f"runs {n}: answered {ans}, ok {ok}, wrong {wrong}, unknown {unk}; "
        f"complete files {complete}/{n}; over the cap {timeouts}",
        f"wall s: median {q(0.5):.2f}, p90 {q(0.9):.2f}, max {q(1.0):.2f}",
    ]
    for (m, e), r in sorted(rows.items()):
        if int(num(r["wrong"])) > 0:
            lines.append(f"  WRONG {m} {e}: {r['wrong']} wrong")
    return "\n".join(lines)


def compare(a: Dict[Tuple[str, str], Row], b: Dict[Tuple[str, str], Row]) -> str:
    lines: List[str] = []
    better = worse = 0
    for key in sorted(set(a) | set(b)):
        ra, rb = a.get(key), b.get(key)
        if ra is None or rb is None:
            lines.append(f"  only in one: {key}")
            continue
        xa, xb = int(num(ra["answered"])), int(num(rb["answered"]))
        if xa != xb:
            lines.append(f"  {key[0]} {key[1]}: {xa} -> {xb} answered ({ra['wall_s']} s -> {rb['wall_s']} s)")
            if xb > xa:
                better += 1
            else:
                worse += 1
    lines.insert(0, f"runs with more answers: {better}, with fewer: {worse}")
    return "\n".join(lines)


def main(argv: List[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 2
    a = load(argv[1])
    print(f"== {argv[1]}")
    print(totals(a))
    if len(argv) > 2:
        b = load(argv[2])
        print(f"== {argv[2]}")
        print(totals(b))
        print("== comparison (answered per run)")
        print(compare(a, b))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
