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
io/SparseMatrixIO.h io/PNETIO.h io/PNET.md
invariants/Heuristic.h invariants/InvariantCalculator.h invariants/InvariantMiddle.h
invariants/InvariantsTrivial.h invariants/MixedSignsUniqueTable.h invariants/RowSignDomination.h
invariants/RowSigns.h
reduction/Configuration.h reduction/Counting.h reduction/Workspace.h reduction/Coordinator.h
reduction/Reduce.h reduction/Composition.h reduction/TransitionAlgebra.h
reduction/graph/Dependency.h reduction/graph/Graph.h reduction/graph/Stabilizing.h
reduction/rules/BoundsDominance.h reduction/rules/ConstantPlace.h reduction/rules/DeadTransition.h
reduction/rules/DuplicatePlace.h reduction/rules/DuplicateTransition.h reduction/rules/EmptySiphon.h
reduction/rules/FreeAgglo.h reduction/rules/FreeSCC.h reduction/rules/FutureEquivalent.h
reduction/rules/ImplicitForkJoin.h reduction/rules/InitialTokenMove.h reduction/rules/LoopBack.h
reduction/rules/NoEffect.h reduction/rules/PartialFreeAgglo.h reduction/rules/PartialPostAgglo.h
reduction/rules/PostAgglo.h reduction/rules/PreAgglo.h reduction/rules/PrefixOfInterest.h
reduction/rules/RedundantComposition.h reduction/rules/ScalarTransition.h reduction/rules/SinkPlace.h
reduction/rules/SinkTransition.h reduction/rules/SourceTransition.h reduction/rules/TrivialPost.h
reduction/cli/CountingBlocks.h reduction/README.md reduction/algorithm.md
reduction/Pipeline.h reduction/Properties.h reduction/PropertyFacts.h reduction/cli/PropertyResults.h expr/InitialState.h
lp/LpProblem.h lp/Basis.h lp/Simplex.h lp/DeadTransitions.h
"
for f in $FILES; do
  mkdir -p "$DST/$(dirname "$f")"
  cp "$SRC/$f" "$DST/$f"
done
rc=0
for f in $FILES; do cmp -s "$SRC/$f" "$DST/$f" || { echo "differs: $f"; rc=1; }; done
[ $rc = 0 ] && echo "vendored $(echo $FILES | wc -w) files from $SRC into $DST, all identical"
exit $rc
