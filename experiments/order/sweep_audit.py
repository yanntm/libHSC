#!/usr/bin/env python3
"""Audit a local sweep against its declared model, examination and heuristic lists."""
from __future__ import annotations
import argparse
from collections import Counter
import csv
import json
from pathlib import Path


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('folder', type=Path)
    ap.add_argument('--heuristics', type=Path, required=True)
    ap.add_argument('--models', type=Path, required=True)
    ap.add_argument('--exams', nargs='+', default=['SS', 'CTLC', 'CTLF'])
    ap.add_argument('--budget', type=float, default=300)
    args = ap.parse_args()
    names = [line.split()[0] for line in args.heuristics.read_text().splitlines() if line.strip() and not line.startswith('#')]
    models = [line.split()[0] for line in args.models.read_text().splitlines() if line.strip() and not line.startswith('#')]
    columns = (args.folder/'columns').read_text().strip().split('\t')
    totals: Counter[str] = Counter()
    issues: list[dict[str, object]] = []
    for model in models:
        for exam in args.exams:
            path = args.folder/'rows'/f'{model}-{exam}.tsv'
            rows: dict[str, dict[str, str]] = {}
            if path.exists():
                for row in csv.DictReader(path.open(), fieldnames=columns, delimiter='\t'):
                    if None in row or any(value is None for value in row.values()):
                        totals['torn_rows'] += 1
                    else:
                        rows[row['heuristic']] = row
            missing: list[str] = []
            interrupted: list[str] = []
            for name in names:
                base = args.folder/f'{model}-{exam}-{name}'
                done = Path(str(base)+'.done').exists()
                started = Path(str(base)+'.started')
                row = rows.get(name)
                if row is None or not done:
                    if started.exists():
                        interrupted.append(f'{name}: {started.read_text().strip()}')
                        totals['interrupted'] += 1
                    else:
                        missing.append(name); totals['not_started'] += 1
                    continue
                totals['accounted'] += 1
                status = row['status']; totals[status.split(':')[0]] += 1
                if status.startswith('dup:'):
                    if status[4:] not in rows:
                        issues.append({'model':model, 'exam':exam, 'heuristic':name, 'missing_duplicate_source':status[4:]})
                    continue
                required = ['.out', '.err', '.time']
                absent = [ext for ext in required if not Path(str(base)+ext).exists()]
                if absent: issues.append({'model':model, 'exam':exam, 'heuristic':name, 'missing_artifacts':absent})
                wall = float(row['wall_s'] or 0)
                if wall > args.budget + 6:
                    totals['overrun'] += 1
                    issues.append({'model':model, 'exam':exam, 'heuristic':name, 'wall_s':wall})
                if row.get('reach_weighted_states'): totals['weighted_stats'] += 1
                if row.get('reach_states'): totals['raw_stats'] += 1
                totals['wrong'] += int(row['wrong'] or 0)
            if missing or interrupted:
                issues.append({'model':model, 'exam':exam, 'accounted':len(names)-len(missing)-len(interrupted),
                               'expected':len(names), 'not_started':missing, 'interrupted':interrupted})
            if not (args.folder/f'{model}-{exam}.job.done').exists(): totals['missing_job_trailer'] += 1
    print(json.dumps({'totals':dict(totals), 'issues':issues}, indent=2))
    if issues or totals['wrong'] or totals['torn_rows'] or totals['missing_job_trailer']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
