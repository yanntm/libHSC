#!/usr/bin/env python3
"""Amenability of BEEM models to enumeration grounding.

For every ``examples/divine/hsc/*.hsc`` model, find the constructs the
cegar bridge refuses — guard atoms whose support spans several units,
action right-hand sides reading foreign units, dynamic array indexes —
and price the enumeration grounding that would remove them: case-split
each offending event on the values of the units it must enumerate, one
event copy per value tuple.  The price is finite exactly when every
enumerated unit has a finite inferred domain (``hsc --domains``).

Output TSV, one row per model:
  model  events  crossing  max_mult  sum_mult  verdict
where ``max_mult``/``sum_mult`` are the largest and summed per-event
grounding multipliers, and verdict is ``clean`` (nothing to ground),
``amenable`` (all multipliers finite), or ``infinite`` (a needed domain
defies analysis).

Usage: tests/cegar_dve_amenable.py [out.tsv]
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path
from typing import Optional, Union

Sexpr = Union[str, list]

HSC = Path("build/tools/hsc")
CORPUS = Path("examples/divine/hsc")


def tokenize(text: str) -> list[str]:
    """Split .hsc text into parenthesis and atom tokens; ';' comments drop."""
    out: list[str] = []
    for line in text.splitlines():
        line = line.split(";", 1)[0]
        out += re.findall(r"[()]|[^\s()]+", line)
    return out


def parse_all(text: str) -> list[Sexpr]:
    """Parse every top-level s-expression of ``text``."""
    tokens = tokenize(text)
    pos = 0

    def one() -> Sexpr:
        nonlocal pos
        tok = tokens[pos]
        pos += 1
        if tok != "(":
            return tok
        items: list[Sexpr] = []
        while tokens[pos] != ")":
            items.append(one())
        pos += 1
        return items

    forms: list[Sexpr] = []
    while pos < len(tokens):
        forms.append(one())
    return forms


def domain_sizes(path: Path) -> dict[str, Optional[int]]:
    """Per-unit inferred domain size via ``hsc --domains``; None = infinite."""
    res = subprocess.run(
        [str(HSC), "--domains", str(path)], capture_output=True, text=True
    )
    sizes: dict[str, Optional[int]] = {}
    for line in res.stdout.splitlines():
        if not line.startswith("xdom "):
            continue
        name = line.split()[1]
        kind = re.search(r"kind=(\w+)", line)
        if kind is None:
            continue
        if kind.group(1) == "set":
            m = re.search(r"size=(\d+)", line)
            sizes[name] = int(m.group(1)) if m else None
        elif kind.group(1) == "interval":
            lo = re.search(r"lo=(-?\d+)", line)
            hi = re.search(r"hi=(-?\d+)", line)
            sizes[name] = (
                int(hi.group(1)) - int(lo.group(1)) + 1 if lo and hi else None
            )
        else:  # top
            sizes[name] = None
    return sizes


def support(expr: Sexpr, units: set[str]) -> set[str]:
    """Unit names read by ``expr``; an ``(at A i)`` reads A and i's support."""
    if isinstance(expr, str):
        return {expr} if expr in units else set()
    if not expr:
        return set()
    if expr[0] in ("at", "at@") and len(expr) >= 3:
        got = {expr[1]} if isinstance(expr[1], str) and expr[1] in units else set()
        return got | support(expr[2], units)
    out: set[str] = set()
    for item in expr[1:] if isinstance(expr[0], str) else expr:
        out |= support(item, units)
    return out


def event_multiplier(
    form: list, units: set[str], sizes: dict[str, Optional[int]]
) -> Optional[int]:
    """Grounding multiplier of one event: product of the domains of every
    unit that must be enumerated; 1 = already separable; None = infinite."""
    enumerate_units: set[str] = set()
    for clause in form[2:]:
        if not isinstance(clause, list) or not clause:
            continue
        if clause[0] == "when":
            for atom in clause[1:]:
                sup = support(atom, units)
                if len(sup) <= 1:
                    continue
                # one unit per atom may stay symbolic: an unbounded unit
                # must take that slot if present; otherwise, to keep the
                # union of enumerated units small, exempt a unit not yet
                # enumerated, the widest among those
                unbounded = {u for u in sup if sizes.get(u) is None}
                if unbounded:
                    exempt = next(iter(unbounded))
                else:
                    fresh = sup - enumerate_units
                    pool = fresh if fresh else sup
                    exempt = max(pool, key=lambda u: sizes.get(u) or 0)
                enumerate_units |= sup - {exempt}
        elif clause[0] == "do":
            for act in clause[1:]:
                if not isinstance(act, list) or len(act) < 3:
                    continue
                lhs, rhs = act[1], act[2]
                target = (
                    lhs if isinstance(lhs, str)
                    else lhs[1] if isinstance(lhs, list) and len(lhs) > 1
                    else ""
                )
                foreign = support(rhs, units) - {target}
                if isinstance(lhs, list):  # dynamic index enumerates too
                    foreign |= support(lhs[2], units) if len(lhs) > 2 else set()
                enumerate_units |= foreign
    mult = 1
    for unit in enumerate_units:
        size = sizes.get(unit)
        if size is None:
            return None
        mult *= size
    return mult


def analyze(path: Path) -> tuple[int, int, Optional[int], Optional[int]]:
    """(events, crossing, max_mult, sum_mult) for one model; None = infinite."""
    forms = parse_all(path.read_text())
    units: set[str] = set()
    for form in forms:
        if isinstance(form, list) and form and form[0] in ("leaf", "array"):
            units.add(form[1])
    sizes = domain_sizes(path)
    n_events = 0
    crossing = 0
    max_mult: Optional[int] = 1
    sum_mult: Optional[int] = 0
    for form in forms:
        if not (isinstance(form, list) and form and form[0] == "event"):
            continue
        n_events += 1
        mult = event_multiplier(form, units, sizes)
        if mult is None:
            crossing += 1
            max_mult = sum_mult = None
        elif mult > 1:
            crossing += 1
            if max_mult is not None:
                max_mult = max(max_mult, mult)
            if sum_mult is not None:
                sum_mult += mult
    return n_events, crossing, max_mult, sum_mult


def main() -> None:
    out_path = Path(sys.argv[1] if len(sys.argv) > 1 else
                    "tests/logs/cegar_dve_amenable.tsv")
    rows: list[str] = []
    for path in sorted(CORPUS.glob("*.hsc")):
        events, crossing, max_mult, sum_mult = analyze(path)
        if crossing == 0:
            verdict = "clean"
        elif max_mult is None:
            verdict = "infinite"
        else:
            verdict = "amenable"
        rows.append(
            f"{path.stem}\t{events}\t{crossing}\t"
            f"{'inf' if max_mult is None else max_mult}\t"
            f"{'inf' if sum_mult is None else sum_mult}\t{verdict}"
        )
    out_path.write_text("\n".join(rows) + "\n")
    counts: dict[str, int] = {}
    for row in rows:
        verdict = row.rsplit("\t", 1)[1]
        counts[verdict] = counts.get(verdict, 0) + 1
    for verdict, n in sorted(counts.items(), key=lambda kv: -kv[1]):
        print(f"{n}\t{verdict}")


if __name__ == "__main__":
    main()
