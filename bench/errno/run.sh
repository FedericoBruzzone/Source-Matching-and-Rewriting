#!/bin/bash
# Region A hoists exp and log out of a loop, region B reads errno after log.
# Prints the time of A and the number of domain errors B counts (4 is right).
cd "$(dirname "$0")" || exit 1
SRC=$PWD
. ../common.sh
cd "$WORK" || exit 1

run() {
  local name=$1 bin=$2
  for _ in 1 2 3; do "./$bin"; done | sort -t= -k4 -n | sed -n 2p | sed "s/^/$name  /"
}

clang -O3 "$SRC/two.c" -lm -o b_clang && run "clang -O3                 " b_clang
clang -O3 -fno-math-errno "$SRC/two.c" -lm -o b_clangne &&
  run "clang -O3 -fno-math-errno " b_clangne
SMR_PATS="" "$SMR_CC" -O3 -c "$SRC/two.c" -o none.o && clang none.o -lm -o b_none &&
  run "smr-cc, no pattern        " b_none
SMR_PATS="$SRC/a.pat" "$SMR_CC" -O3 -c "$SRC/two.c" -o pat.o && clang pat.o -lm -o b_pat &&
  run "smr-cc, pattern on A      " b_pat
