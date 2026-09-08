#!/usr/bin/env python3
"""Append a named KERS block to a PNET file (INTEROP.md section 3).

Used to exercise the optional blocks without a producer: it writes the block
framing (8-byte name, uint32 length) followed by a one-column KERS payload
holding one value per row.

    tests/pnet_block.py in.pnet out.pnet TMULT 52 --all 1
    tests/pnet_block.py in.pnet out.pnet TMULT 52 --at 3 7 --at 5 2
"""
from __future__ import annotations

import argparse
import struct
from typing import Dict, List


def kers_column(rows: int, values: Dict[int, int]) -> bytes:
    """One KERS block: a rows x 1 matrix whose non-zero entries are `values`."""
    out = bytearray()
    out += b"KERS" + bytes([1, 0]) + struct.pack("<II", rows, 1) + bytes([0, 0])
    entries = sorted((r, v) for r, v in values.items() if v != 0)
    if entries:
        out += struct.pack("<II", 0, len(entries))          # column 0, nnz
        out += b"".join(struct.pack("<I", r) for r, _ in entries)
        out += b"".join(struct.pack("<q", v) for _, v in entries)
    out += struct.pack("<I", 0xFFFFFFFF)                     # terminator
    return bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source")
    ap.add_argument("dest")
    ap.add_argument("name")
    ap.add_argument("rows", type=int)
    ap.add_argument("--all", type=int, default=None,
                    help="value for every row (stored as given: TMULT holds weight - 1)")
    ap.add_argument("--at", nargs=2, type=int, action="append", metavar=("ROW", "VALUE"),
                    default=[], help="value for one row, repeatable")
    args = ap.parse_args()

    values: Dict[int, int] = {}
    if args.all is not None:
        values = {r: args.all for r in range(args.rows)}
    for row, value in args.at:
        values[row] = value

    payload = kers_column(args.rows, values)
    name = args.name.encode()
    if not 1 <= len(name) <= 8:
        raise SystemExit("a block name is 1 to 8 characters")
    with open(args.source, "rb") as f:
        net = f.read()
    with open(args.dest, "wb") as f:
        f.write(net)
        f.write(name.ljust(8, b"\0"))
        f.write(struct.pack("<I", len(payload)))
        f.write(payload)
    print(f"{args.dest}: {args.name} block of {len(payload)} bytes, "
          f"{len(values)} non-default rows")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
