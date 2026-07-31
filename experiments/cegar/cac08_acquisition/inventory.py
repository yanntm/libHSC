#!/usr/bin/env python3
"""Inventory the CAC08 LTSA tarballs without extracting them.

Reads `examples/cac08/upstream/*-LTSA.tar.gz` and emits one TSV row per
(system, variant, size, property) subject: the `.lts` FSP model, whether the
paper's decomposition file `.txt` accompanies it, and the model's byte size.

Path shapes in the upstream archives, one per system:
  peterson/peterson<k>.lts                            size in the stem
  gas_station/gas_p2_c<kkk>/<prop>.lts                size in the dir
  relay/relay_<kk>/relay-<kk>.lts                     size in the dir
  smokers/smokers<k>/<prop>.lts                       size in the dir
  chiron/{single,multiple}/<kkkkk>/disp[-<prop>].lts  size in the dir
"""

from __future__ import annotations

import re
import sys
import tarfile
from pathlib import Path
from typing import Iterator, NamedTuple, Optional


class Subject(NamedTuple):
    system: str
    variant: str
    size: Optional[int]
    prop: str
    member: str
    nbytes: int


def classify(member: str) -> Optional[Subject]:
    """Map one `.lts` archive member onto a subject, or None if it is not one."""
    if not member.endswith(".lts"):
        return None
    parts = Path(member).parts
    system = parts[0]
    stem = Path(member).stem

    if system == "peterson":
        m = re.fullmatch(r"peterson(\d+)", stem)
        # peterson.lts is the unsized template, not a subject
        return None if not m else Subject(
            "peterson", "-", int(m.group(1)), "mutual_exclusion", member, 0)

    if system == "gas_station":
        m = re.search(r"gas_p(\d+)_c(\d+)", member)
        return None if not m else Subject(
            "gas_station", f"p{int(m.group(1))}", int(m.group(2)), stem, member, 0)

    if system == "relay":
        m = re.search(r"relay_(\d+)", member)
        return None if not m else Subject(
            "relay", "-", int(m.group(1)), "relay", member, 0)

    if system == "smokers":
        m = re.search(r"smokers(\d+)", member)
        return None if not m else Subject(
            "smokers", "-", int(m.group(1)), stem, member, 0)

    if system == "chiron":
        variant = parts[1]  # single | multiple
        m = re.fullmatch(r"(\d+)", parts[2])
        size = int(m.group(1)) if m else None
        prop = stem.split("-", 1)[1] if "-" in stem else "(base)"
        return Subject("chiron", variant, size, prop, member, 0)

    return None


def scan(tarball: Path) -> Iterator[Subject]:
    with tarfile.open(tarball, "r:gz") as tf:
        names = {ti.name: ti.size for ti in tf.getmembers()}
    for name, size in sorted(names.items()):
        s = classify(name)
        if s is not None:
            yield s._replace(nbytes=size)


def main(argv: list[str]) -> int:
    up = Path(argv[1]) if len(argv) > 1 else Path("examples/cac08/upstream")
    rows: list[Subject] = []
    for tb in sorted(up.glob("*-LTSA.tar.gz")):
        rows.extend(scan(tb))

    print("system\tvariant\tsize\tproperty\tbytes\tmember")
    for r in rows:
        print(f"{r.system}\t{r.variant}\t{r.size}\t{r.prop}\t{r.nbytes}\t{r.member}")

    # Summary to stderr so the TSV on stdout stays clean.
    from collections import defaultdict
    per: dict[tuple[str, str], set[str]] = defaultdict(set)
    sizes: dict[tuple[str, str], set[int]] = defaultdict(set)
    for r in rows:
        per[(r.system, r.variant)].add(r.prop)
        if r.size is not None:
            sizes[(r.system, r.variant)].add(r.size)
    total_props = 0
    print("\n=== distinct properties and sizes per system ===", file=sys.stderr)
    for key in sorted(per):
        props = sorted(per[key])
        ks = sorted(sizes[key])
        total_props += len(props)
        print(f"{key[0]:12s} {key[1]:9s} {len(props)} props  "
              f"k in {ks}\n{'':24s}{props}", file=sys.stderr)
    print(f"\ntotal (system,property) pairs: {total_props}", file=sys.stderr)
    print(f"total .lts subject files: {len(rows)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
