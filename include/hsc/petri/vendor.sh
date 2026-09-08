#!/bin/bash
# Re-vendor the PetriSpot sources this folder carries (README.md, "Vendored,
# with attribution"): exact copies, in upstream's own subfolder layout so
# their includes ("core/...", "expr/...") resolve with this folder as an
# include root. No local edit: anything the copies need is changed upstream
# first. Ends by checking that every copy is byte-identical to its source.
#
#   include/hsc/petri/vendor.sh [~/git/PetriSpot]
set -e
SRC="${1:-$HOME/git/PetriSpot}/Petri/src"
DST="$(cd "$(dirname "$0")" && pwd)"
[ -d "$SRC/core" ] || { echo "no PetriSpot sources at $SRC" >&2; exit 1; }

FILES="
core/SparseArray.h core/MatrixCol.h core/SparseBoolArray.h core/Arithmetic.hpp
core/InvariantHelpers.h core/Rational.h core/SparsePetriNet.h core/Log.h
parse/PTNetHandler.h parse/PTNetLoader.h parse/NetResolver.h parse/PropertyFile.h
parse/mcc/PropertyHandler.h parse/mcc/PropertyLoader.h parse/mcc/README.md
parse/sexpr/Sexpr.h parse/sexpr/PropertyReader.h parse/sexpr/HintReader.h parse/sexpr/README.md
expr/Expression.h expr/Property.h expr/CtlFormula.h expr/CtlSimplify.h expr/Simplify.h
expr/SexprPrinter.h expr/Hint.h expr/README.md expr/algorithm.md
io/SparseMatrixIO.h io/PNETIO.h
"
for f in $FILES; do
  mkdir -p "$DST/$(dirname "$f")"
  cp "$SRC/$f" "$DST/$f"
done
rc=0
for f in $FILES; do cmp -s "$SRC/$f" "$DST/$f" || { echo "differs: $f"; rc=1; }; done
[ $rc = 0 ] && echo "vendored $(echo $FILES | wc -w) files from $SRC into $DST, all identical"
exit $rc
