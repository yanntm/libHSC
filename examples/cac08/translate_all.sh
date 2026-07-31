#!/bin/sh
# Regenerate examples/cac08/hsc/ — the .hsc translation of every LTSA
# subject in curated/ — with tools/fsp2hsc. One model-only file plus one
# driver per (system, size, property); the companion .txt (the paper's S1
# decomposition) is recorded in the model header when present.
# Usage: sh examples/cac08/translate_all.sh [path/to/fsp2hsc]
# Variant knobs (for encoding experiments):
#   OUT_DIR   destination under this folder        (default hsc)
#   FSP_FLAGS extra fsp2hsc flags, e.g. --pinned   (default none)
#   CHAIN     rewrite directives inserted into each driver after the
#             (input …) line, e.g. '(hotbit 17 100000)'  (default none)
set -eu
cd "$(dirname "$0")"
FSP2HSC=${1:-../../build/tools/fsp2hsc}
OUT_DIR=${OUT_DIR:-hsc}
FSP_FLAGS=${FSP_FLAGS:-}
CHAIN=${CHAIN:-}

emit() { # emit SYSTEM NAME LTSFILE
  sys=$1; name=$2; lts=$3
  txt="${lts%.lts}.txt"
  mkdir -p "$OUT_DIR/$sys"
  if [ -f "$txt" ]; then s1="--s1 $txt"; else s1=""; fi
  # shellcheck disable=SC2086
  "$FSP2HSC" "$lts" $s1 $FSP_FLAGS -o "$OUT_DIR/$sys/${name}_model.hsc" \
    --driver "$OUT_DIR/$sys/${name}.hsc"
  if [ -n "$CHAIN" ]; then
    drv="$OUT_DIR/$sys/${name}.hsc"
    awk -v chain="$CHAIN" '{print} /^\(input / {print chain}' \
      "$drv" > "$drv.tmp" && mv "$drv.tmp" "$drv"
  fi
  echo "$OUT_DIR/$sys/${name}_model.hsc"
}

for d in curated/gas_station/gas_p2_c*/; do
  n=${d%/}; n=${n##*_c}
  for lts in "$d"gas-*.lts; do
    p=$(basename "$lts" .lts); p=${p#gas-}; p=$(echo "$p" | tr '-' '_')
    emit gas_station "gas_c${n}_${p}" "$lts"
  done
done

for lts in curated/peterson/peterson[0-9].lts; do
  emit peterson "$(basename "$lts" .lts)" "$lts"
done

for d in curated/relay/relay_*/; do
  n=${d%/}; n=${n##*relay_}
  emit relay "relay_${n}" "$d"relay-*.lts
done

for d in curated/smokers/smokers[0-9]/; do
  n=${d%/}; n=${n##*smokers}
  for lts in "$d"*.lts; do
    p=$(basename "$lts" .lts)
    emit smokers "smokers${n}_${p}" "$lts"
  done
done

# Chiron: curated holds the 2-artist size only (see README); larger sizes
# come from the tarballs on demand.
for v in single multiple; do
  for lts in curated/chiron_$v/disp-p*.lts; do
    p=$(basename "$lts" .lts); p=${p#disp-}; p=${p%a}
    emit chiron_$v "chiron_${v}_a2_${p}" "$lts"
  done
done
