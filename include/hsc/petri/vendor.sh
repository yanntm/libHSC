#!/bin/bash
# Re-vendor the PetriSpot sources this folder carries (see README.md, section
# "Vendored, with attribution"). Copies the listed files from a PetriSpot
# checkout, prepends the provenance banner, rewrites the include paths to
# this folder's layout (core/ and the PNML loader flat, the rest in their
# upstream subfolders) and re-applies the documented local edits. Idempotent.
#
#   include/hsc/petri/vendor.sh [~/git/PetriSpot]
set -e
SRC="${1:-$HOME/git/PetriSpot}/Petri/src"
DST="$(cd "$(dirname "$0")" && pwd)"
[ -d "$SRC/core" ] || { echo "no PetriSpot sources at $SRC" >&2; exit 1; }

BANNER='// Vendored into libHSC from PetriSpot (https://github.com/yanntm/PetriSpot),
// (C) Yann Thierry-Mieg, GPL-3.0-or-later. Copied with minimal edits; provenance
// and the list of local changes are in include/hsc/petri/README.md.
'

# upstream path -> destination relative to this folder
vendor() {
  local up="$1" rel="$2"
  local out="$DST/$rel"
  mkdir -p "$(dirname "$out")"
  case "$up" in
    *.h|*.hpp)
      { printf '%s\n' "$BANNER"; cat "$SRC/$up"; } \
        | sed -E 's|#include "core/([^"]+)"|#include "\1"|; s|#include "parse/(PTNetHandler\|PTNetLoader)\.h"|#include "\1.h"|' \
        > "$out" ;;
    *) cp "$SRC/$up" "$out" ;;
  esac
}

# core, flat
for f in SparseArray.h MatrixCol.h SparseBoolArray.h Arithmetic.hpp InvariantHelpers.h Rational.h SparsePetriNet.h Log.h; do
  vendor "core/$f" "$f"
done
# the PNML loader, flat
vendor parse/PTNetHandler.h PTNetHandler.h
vendor parse/PTNetLoader.h PTNetLoader.h
# property tree, parsers, printers, binary IO: upstream subfolders
for f in Expression.h Property.h CtlFormula.h CtlSimplify.h Simplify.h SexprPrinter.h Hint.h README.md algorithm.md; do
  vendor "expr/$f" "expr/$f"
done
for f in NetResolver.h PropertyFile.h; do vendor "parse/$f" "parse/$f"; done
for f in PropertyHandler.h PropertyLoader.h README.md; do vendor "parse/mcc/$f" "parse/mcc/$f"; done
for f in Sexpr.h PropertyReader.h HintReader.h README.md; do vendor "parse/sexpr/$f" "parse/sexpr/$f"; done
for f in SparseMatrixIO.h PNETIO.h; do vendor "io/$f" "io/$f"; done

# --- local edits (each a no-op once upstream carries it) ---
# Rational.h: <numeric> for std::gcd, transitively satisfied upstream only.
grep -q '#include <numeric>' "$DST/Rational.h" || sed -i '0,/^#include </s//#include <numeric>\n#include </' "$DST/Rational.h"
# Arithmetic.hpp: the __uint128_t printer is a definition in a header; inline
# it, libHSC links it from several translation units.
sed -i -E 's|^std::ostream& operator<<\(std::ostream& out, __uint128_t n\)|inline std::ostream\& operator<<(std::ostream\& out, __uint128_t n)|' "$DST/Arithmetic.hpp"

# Log.h: upstream logs on stdout; here stdout carries the emitted model or the
# answer protocol, so the log goes to stderr.
sed -i 's|std::cout << "\[" << (parts->tm_year|std::cerr << "[" << (parts->tm_year|' "$DST/Log.h"

echo "vendored from $SRC into $DST"
