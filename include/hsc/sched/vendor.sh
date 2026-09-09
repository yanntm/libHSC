#!/usr/bin/env bash
# vendor.sh: copy PetriSpot's scheduler types (Petri/src/sched/) here, the
# only edit being the include path ("sched/Task.h" -> "hsc/sched/Task.h").
# Usage: include/hsc/sched/vendor.sh [~/git/PetriSpot]; exits 1 when a copy
# differs from its source beyond that edit.
set -e
SRC="${1:-$HOME/git/PetriSpot}/Petri/src/sched"
DST="$(cd "$(dirname "$0")" && pwd)"
[ -d "$SRC" ] || { echo "no PetriSpot sched sources at $SRC" >&2; exit 1; }
rc=0
for f in Task.h Scheduler.h; do
  sed 's#"sched/Task.h"#"hsc/sched/Task.h"#' "$SRC/$f" > "$DST/$f"
  cmp -s <(sed 's#"sched/Task.h"#"hsc/sched/Task.h"#' "$SRC/$f") "$DST/$f" || { echo "differs: $f"; rc=1; }
done
[ $rc = 0 ] && echo "vendored Task.h Scheduler.h from $SRC into $DST"
exit $rc
