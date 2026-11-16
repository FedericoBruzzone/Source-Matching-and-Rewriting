#!/bin/bash
# vectorize, fast_math, unroll and tiling as global pipelines on four kernels,
# next to clang -O3 and clang -O3 -ffast-math. Time per call in microseconds,
# size of the symbol kernel and the result (hex float, so reassociation shows).
cd "$(dirname "$0")" || exit 1
SRC=$PWD
. ../common.sh
cd "$WORK" || exit 1

KERNELS=${KERNELS:-"dot saxpy k8b k10a"}
pipelines=(
  "none|"
  "vec|vectorize"
  "fm|fast_math"
  "fm,vec|fast_math, vectorize"
  "unroll4|unroll(4)"
  "tiling32|tiling(32)"
)

row() {
  local k=$1 name=$2 obj=$3 out
  clang "$obj" -lm -o "$k.$name" || { echo "$k $name LINK-FAILED"; return; }
  out=$(median_run "./$k.$name")
  echo "$k $name $(echo "$out" | awk '{printf "%.2f", $2*1e6}') $(size_of "$obj") $(echo "$out" | awk '{print $1}')"
}

echo "kernel build time_us bytes result"
for k in $KERNELS; do
  src=$SRC/$k.c
  [ -f "$src" ] || src=$SRC/../phase/$k.c
  clang -O3 -c "$src" -o "$k.clang.o" && row "$k" clang-O3 "$k.clang.o"
  clang -O3 -ffast-math -c "$src" -o "$k.clangfm.o" && row "$k" clang-O3-ffast-math "$k.clangfm.o"
  for spec in "${pipelines[@]}"; do
    name=${spec%%|*}
    body=${spec#*|}
    pats=""
    if [ -n "$body" ]; then
      echo "\$GLOBAL = [$body]" > "$k.$name.pat"
      pats="$k.$name.pat"
    fi
    SMR_PATS="$pats" "$SMR_CC" -O3 -c "$src" -o "$k.$name.o" 2> /dev/null ||
      { echo "$k $name BUILD-FAILED"; continue; }
    row "$k" "smr-$name" "$k.$name.o"
  done
done
