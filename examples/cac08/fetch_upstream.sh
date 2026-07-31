#!/bin/sh
# Fetch the original CAC08 experimental subjects from the Wayback Machine.
#
# Cobleigh, Clarke, Avrunin, "Breaking Up is Hard to Do: An Evaluation of
# Automated Assume-Guarantee Reasoning", TOSEM 17(2), 2008, published the
# subjects at a URL that is dead today.  The URL printed in the paper (its
# p. 7:14) is itself wrong: it reads
#     http://laser.cs.umass.edu/~jcobleig/breakingup-examples/
# but no such path was ever served.  The archived location is
#     http://laser.cs.umass.edu/breakingup-examples/
# crawled 2010-06-27 (tarballs) and 2011-08-19 (index).
#
# Writes into upstream/.  Idempotent: existing files are kept.
# Checksums of what this fetched on 2026-07-31 are in upstream/SHA256SUMS.

set -eu

here=$(dirname "$0")
dest="$here/upstream"
mkdir -p "$dest"

base=https://web.archive.org/web
snap=20100627000000id_
src=http://laser.cs.umass.edu/breakingup-examples

for f in \
  chiron-FLAVERS.tar.gz chiron-LTSA.tar.gz \
  gas_station-FLAVERS.tar.gz gas_station-LTSA.tar.gz \
  peterson-FLAVERS.tar.gz peterson-LTSA.tar.gz \
  relay-FLAVERS.tar.gz relay-LTSA.tar.gz \
  smokers-FLAVERS.tar.gz smokers-LTSA.tar.gz
do
  if [ -s "$dest/$f" ]; then
    echo "have $f"
    continue
  fi
  echo "get  $f"
  curl -fsSL --retry 3 --retry-delay 2 -m 300 "$base/$snap/$src/$f" -o "$dest/$f"
done

echo "--- sizes ---"
ls -l "$dest"
