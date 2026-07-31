#!/bin/sh
# Build curated/ — the working set we actually translate — from the
# committed upstream tarballs.  Nothing here is authored by us: every file
# is copied byte-for-byte out of upstream/*.tar.gz.
#
# Kept: the LTSA side only, and of it only
#   *.lts   FSP source: component processes + the safety `property` DFA
#   *.txt   the paper's chosen decomposition (the component set S1)
# Chiron is taken at its smallest size only: its FSP is pre-flattened, so
# one property file at 4 artists is ~500 KB and at 5 artists ~10 MB.  The
# larger sizes stay in the tarball and are pulled on demand.
#
# Dropped entirely: the FLAVERS side (Ada, control-flow graphs, QRE) —
# a different toolchain, nothing there is translated.  Exception:
# chiron_p08/, the only surviving trace of the paper's Chiron property 8.
#
# Idempotent: rebuilds curated/ from scratch each run.

set -eu

here=$(dirname "$0")
up="$here/upstream"
out="$here/curated"
tmp="$out/.staging"

rm -rf "$out"
mkdir -p "$tmp"

for s in gas_station peterson relay smokers; do
  tar xzf "$up/$s-LTSA.tar.gz" -C "$tmp"
done
tar xzf "$up/chiron-LTSA.tar.gz" -C "$tmp" \
  chiron/single/02021 chiron/multiple/02021
tar xzf "$up/chiron-FLAVERS.tar.gz" -C "$tmp" \
  chiron/original/02021/properties/p08a.qre \
  chiron/decomposed/02021/properties/p08a.qre

# Per-model subfolders, .lts + .txt only.
for s in gas_station peterson relay smokers; do
  mkdir -p "$out/$s"
  (cd "$tmp/$s" && find . \( -name '*.lts' -o -name '*.txt' \) -print) \
    | while read -r f; do
        mkdir -p "$out/$s/$(dirname "$f")"
        cp "$tmp/$s/$f" "$out/$s/$f"
      done
done

for v in single multiple; do
  mkdir -p "$out/chiron_$v"
  cp "$tmp"/chiron/$v/02021/*.lts "$tmp"/chiron/$v/02021/*.txt "$out/chiron_$v/"
done

mkdir -p "$out/chiron_p08"
cp "$tmp/chiron/original/02021/properties/p08a.qre" "$out/chiron_p08/p08a-original.qre"
cp "$tmp/chiron/decomposed/02021/properties/p08a.qre" "$out/chiron_p08/p08a-decomposed.qre"

rm -rf "$tmp"

# The Chiron licence obliges the notice to appear in all copies; the FSP
# files carry it nowhere, so it is placed beside them.
cp "$here/LICENSE.chiron" "$out/chiron_single/LICENSE"
cp "$here/LICENSE.chiron" "$out/chiron_multiple/LICENSE"
cp "$here/LICENSE.chiron" "$out/chiron_p08/LICENSE"

echo "--- curated/ ---"
du -sh "$out"/*
echo "total: $(du -sh "$out" | cut -f1)   files: $(find "$out" -type f | wc -l)"
