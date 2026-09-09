#!/usr/bin/env python3
"""Score FORMULA lines against a contest oracle file.

    score.py ORACLE.out RUN.log [RUN.log ...]

Every `FORMULA NAME VERDICT ...` line of the runs is compared with the
oracle's line for NAME (`?` in the oracle constrains nothing). Prints one
line: formulas in the oracle, answered, correct, wrong, unchecked (oracle ?),
then the names of the wrong ones. With `--tsv KEY` the line is tab-separated
and prefixed with KEY, for a merger.
"""
from __future__ import annotations

import sys
from typing import Dict, List, Tuple


def read_formulas(path: str) -> Dict[str, str]:
    """NAME -> last verdict of the FORMULA lines of a file."""
    out: Dict[str, str] = {}
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                parts = line.split()
                if len(parts) >= 3 and parts[0] == "FORMULA":
                    out[parts[1]] = parts[2]
    except OSError:
        pass
    return out


def score(oracle: Dict[str, str], run: Dict[str, str]) -> Tuple[int, int, int, int, int, List[str]]:
    """(total, answered, correct, wrong, unchecked, wrong names)."""
    answered = correct = wrong = unchecked = 0
    bad: List[str] = []
    for name, v in run.items():
        if name not in oracle:
            continue
        answered += 1
        o = oracle[name]
        if o == "?":
            unchecked += 1
        elif o == v:
            correct += 1
        else:
            wrong += 1
            bad.append(name)
    return len(oracle), answered, correct, wrong, unchecked, bad


def main(argv: List[str]) -> int:
    tsv_key = None
    if "--tsv" in argv:
        i = argv.index("--tsv")
        tsv_key = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
    if len(argv) < 3:
        print(__doc__)
        return 2
    oracle = read_formulas(argv[1])
    run: Dict[str, str] = {}
    for path in argv[2:]:
        run.update(read_formulas(path))
    total, answered, correct, wrong, unchecked, bad = score(oracle, run)
    if tsv_key is not None:
        print("\t".join(str(x) for x in (tsv_key, total, answered, correct, wrong, unchecked)) + "\t" + " ".join(bad))
    else:
        print(f"oracle {total} answered {answered} correct {correct} wrong {wrong} unchecked {unchecked}"
              + (" WRONG: " + " ".join(bad) if bad else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
