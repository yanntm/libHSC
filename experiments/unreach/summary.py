#!/usr/bin/env python3
"""Summarise the rows.tsv of an approx sweep.

    summary.py tests/logs/unreach/TAG/rows.tsv [...]

Per engine: runs, formulas in the oracles, answered, correct, wrong,
unchecked, runs at the cap (exit 124), other non-zero exits, total and
median seconds. Then, per examination, the pairwise picture between
engines: instances where one answered strictly more formulas than another.
Wrong verdicts are listed by name: each is a soundness bug.
"""
from __future__ import annotations

import statistics
import sys
from collections import defaultdict
from typing import Dict, List, Tuple

Row = Tuple[str, str, str, str, int, int, int, int, int, float, int, str]


def read_rows(paths: List[str]) -> List[Row]:
    rows: List[Row] = []
    for path in paths:
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                p = line.rstrip("\n").split("\t")
                if len(p) < 11:
                    continue
                try:
                    rows.append((p[0], p[1], p[2], p[3], int(p[4]), int(p[5]), int(p[6]), int(p[7]), int(p[8]),
                                 float(p[9]), int(p[10]), p[11] if len(p) > 11 else ""))
                except ValueError:
                    continue
    return rows


def main(argv: List[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 2
    rows = read_rows(argv[1:])
    by_engine: Dict[str, List[Row]] = defaultdict(list)
    for r in rows:
        by_engine[r[3]].append(r)
    print("engine\truns\toracle\tanswered\tcorrect\twrong\tunchecked\tcap\tfail\ttotal_s\tmedian_s")
    for eng in sorted(by_engine):
        rs = by_engine[eng]
        secs = [r[9] for r in rs]
        print("\t".join(str(x) for x in (
            eng, len(rs), sum(r[4] for r in rs), sum(r[5] for r in rs), sum(r[6] for r in rs), sum(r[7] for r in rs),
            sum(r[8] for r in rs), sum(1 for r in rs if r[10] == 124), sum(1 for r in rs if r[10] not in (0, 124)),
            round(sum(secs), 1), round(statistics.median(secs), 2) if secs else 0)))
    wrong = [(r[3], r[1], r[2], r[11]) for r in rows if r[7] > 0]
    if wrong:
        print("\nWRONG verdicts (soundness bugs):")
        for w in wrong:
            print("  " + "\t".join(w))
    # pairwise: answered per (model, exam) per engine
    answered: Dict[Tuple[str, str], Dict[str, int]] = defaultdict(dict)
    for r in rows:
        answered[(r[1], r[2])][r[3]] = r[5]
    engines = sorted(by_engine)
    print("\npairwise: instances where the row engine answered strictly more than the column engine")
    print("\t" + "\t".join(engines))
    for a in engines:
        cells = []
        for b in engines:
            n = sum(1 for k, d in answered.items() if a in d and b in d and d[a] > d[b])
            cells.append(str(n) if a != b else "-")
        print(a + "\t" + "\t".join(cells))
    for ex in ("RC", "RF"):
        tot = {e: sum(d.get(e, 0) for k, d in answered.items() if k[1] == ex) for e in engines}
        orc = {e: sum(r[4] for r in rows if r[2] == ex and r[3] == e) for e in engines}
        print(f"\n{ex}: " + ", ".join(f"{e} {tot[e]}/{orc[e]}" for e in engines))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
