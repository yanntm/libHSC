#!/bin/bash
# The CI build: a static libexpat, then libHSC configured with HSC_STATIC,
# its test suite as a gate, and the stripped tool binaries in website/.
# Run it locally to reproduce what the CI publishes; it uses its own build
# tree (build-static/) and prefix (usr/local/), both git-ignored.
#
#   ./build_hsc.sh            # build, test, package
#   ./build_hsc.sh --no-test  # skip the suite
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PREFIX="$SCRIPT_DIR/usr/local"
BUILD="$SCRIPT_DIR/build-static"
EXPAT_VERSION=2.6.4
JOBS=$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
RUN_TESTS=1
[ "$1" = "--no-test" ] && RUN_TESTS=0

mkdir -p "$PREFIX"

# --- libexpat (static) ---
echo "=== Building libexpat $EXPAT_VERSION ==="
cd "$SCRIPT_DIR"
EXPAT_TAG="R_${EXPAT_VERSION//./_}"
if [ ! -f "$PREFIX/lib/libexpat.a" ]; then
  if [ ! -f "expat-$EXPAT_VERSION.tar.gz" ]; then
    wget -q --tries=5 --waitretry=5 "https://github.com/libexpat/libexpat/releases/download/$EXPAT_TAG/expat-$EXPAT_VERSION.tar.gz"
  fi
  rm -rf "expat-$EXPAT_VERSION"
  tar xzf "expat-$EXPAT_VERSION.tar.gz"
  cmake -S "expat-$EXPAT_VERSION" -B "expat-$EXPAT_VERSION/build" \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
      -DEXPAT_SHARED_LIBS=OFF -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_EXAMPLES=OFF \
      -DEXPAT_BUILD_TESTS=OFF -DEXPAT_BUILD_DOCS=OFF -DEXPAT_BUILD_PKGCONFIG=OFF
  cmake --build "expat-$EXPAT_VERSION/build" -j"$JOBS"
  cmake --install "expat-$EXPAT_VERSION/build"
  rm -rf "expat-$EXPAT_VERSION" "expat-$EXPAT_VERSION.tar.gz"
fi

# --- libHSC ---
echo "=== Building libHSC ==="
cd "$SCRIPT_DIR"
cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DHSC_STATIC=ON -DHSC_BUILD_BENCH=OFF -DHSC_BUILD_TESTS=ON
cmake --build "$BUILD" -j"$JOBS"

if [ "$RUN_TESTS" = 1 ]; then
  echo "=== Running the test suite ==="
  ctest --test-dir "$BUILD" --output-on-failure -j"$JOBS"
fi

# --- Package binaries ---
echo "=== Packaging binaries ==="
mkdir -p website
for t in hsc hsc-mcc nupn2hsc dve2hsc fsp2hsc; do
  cp "$BUILD/tools/$t" website/
  strip website/$t
done
ls -la website/
echo "=== Done. Binaries in $SCRIPT_DIR/website/ ==="
