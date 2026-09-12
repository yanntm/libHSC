#!/usr/bin/env python3
"""Compare original per-transition enablings, one real PNML model per invocation."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time
from typing import TypedDict


class RunRecord(TypedDict):
    command: list[str]
    exit: int
    seconds: float
    metrics: dict[str, str]
    enabled: dict[str, str]


def values(text: str, pattern: str) -> dict[str, str]:
    return dict(re.findall(pattern, text, re.MULTILINE))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('model', type=Path)
    parser.add_argument('--binary', type=Path, default=Path('build/tools/hsc-pn'))
    parser.add_argument('--oracle', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    deadline = time.monotonic() + 14
    records: dict[str, RunRecord] = {}
    for setting in ('plain', 'reduce'):
        remaining = deadline - time.monotonic()
        if remaining < 1:
            raise SystemExit('diagnostic budget exhausted before ' + setting)
        command = [str(args.binary.resolve()), '-i', str(args.model.resolve()),
                   '--states', '--totalTime', str(max(1, int(remaining))), '-v']
        if setting == 'reduce':
            command.append('--reduce')
        before = time.monotonic()
        with (args.output / (setting + '.txt')).open('w') as log:
            try:
                result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                        env={**os.environ, 'HSC_ENABLING_TRACE': '1'},
                                        timeout=remaining, check=False)
            except subprocess.TimeoutExpired:
                raise SystemExit(setting + ': diagnostic timeout; see saved log')
        text = (args.output / (setting + '.txt')).read_text()
        records[setting] = {'command': command, 'exit': result.returncode,
                            'seconds': time.monotonic() - before,
                            'metrics': values(text, r'^STATE_SPACE (\S+) (\S+)'),
                            'enabled': values(text, r'^hsc-pn: enabling (\S+) (\d+)$')}
    expected = values(args.oracle.read_text(), r'^STATE_SPACE (\S+) (\S+)')
    plain, reduced = records['plain'], records['reduce']
    differences = {name: [plain['enabled'].get(name), reduced['enabled'].get(name)]
                   for name in set(plain['enabled']) | set(reduced['enabled'])
                   if plain['enabled'].get(name) != reduced['enabled'].get(name)}
    ok = (plain['exit'] == reduced['exit'] == 0 and len(expected) == 4
          and plain['metrics'] == reduced['metrics'] == expected
          and bool(plain['enabled']) and not differences)
    report = {'ok': ok, 'oracle': str(args.oracle), 'records': records,
              'differences': differences}
    (args.output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    print(args.model.parent.name, 'PASS' if ok else 'FAIL',
          len(plain['enabled']), 'original transitions;',
          'plain', round(plain['seconds'], 3), 's; reduced', round(reduced['seconds'], 3), 's')
    if not ok:
        print('metrics:', plain['metrics'], reduced['metrics'], 'oracle:', expected)
        print('first differences:', list(differences.items())[:10])
        raise SystemExit(1)


if __name__ == '__main__':
    main()
