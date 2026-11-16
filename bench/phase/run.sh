#!/bin/bash
# Four kernels, each under eight global pipelines: licm, unroll and tiling in
# both orders. One row per (kernel, pipeline): time of the best run in
# milliseconds and size of the symbol kernel.
cd "$(dirname "$0")" || exit 1
SRC=$PWD
. ../common.sh
cd "$WORK" || exit 1

declare -A LICM=([k8a]="licm(exp, log)" [k8b]="licm" [k10a]="licm" [k10b]="licm(calculate_factor)")
KERNELS=${KERNELS:-"k8a k8b k10a k10b"}

pipelines=(
  "none|"
  "licm|LICM"
  "unroll4|unroll(4)"
  "tiling32|tiling(32)"
  "licm,unroll4|LICM, unroll(4)"
  "unroll4,licm|unroll(4), LICM"
  "licm,tiling32|LICM, tiling(32)"
  "tiling32,licm|tiling(32), LICM"
)

echo "kernel pipeline time_ms bytes sum"
for k in $KERNELS; do
  clang -O3 "$SRC/$k.c" -lm -o "$k.clang" 2> /dev/null
  clang -O3 -c "$SRC/$k.c" -o "$k.clang.o" 2> /dev/null
  out=$(median_run "./$k.clang")
  echo "$k clang-O3 $(echo "$out" | awk '{printf "%.3f", $2*1000}') $(size_of "$k.clang.o") $(echo "$out" | awk '{print $1}')"
  for spec in "${pipelines[@]}"; do
    name=${spec%%|*}
    body=${spec#*|}
    body=${body//LICM/${LICM[$k]}}
    pats=""
    if [ -n "$body" ]; then
      echo "\$GLOBAL = [$body]" > "$k.$name.pat"
      pats="$k.$name.pat"
    fi
    SMR_PATS="$pats" "$SMR_CC" -O3 -c "$SRC/$k.c" -o "$k.$name.o" || { echo "$k $name BUILD-FAILED"; continue; }
    clang "$k.$name.o" -lm -o "$k.$name" || { echo "$k $name LINK-FAILED"; continue; }
    out=$(median_run "./$k.$name")
    echo "$k $name $(echo "$out" | awk '{printf "%.3f", $2*1000}') $(size_of "$k.$name.o") $(echo "$out" | awk '{print $1}')"
  done
done
