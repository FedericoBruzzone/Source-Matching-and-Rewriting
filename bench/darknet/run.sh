#!/bin/bash
# gemm_nn of darknet in three builds, all through smr-cc: no pattern, tiling(64)
# as a $GLOBAL pipeline, and the hand tiled version of Pedro (64 by
# 64 by 64, i0 j0 k0 i k j). The shapes are the largest of yolov3. BIG=1 adds
# large square matrices (M=N=K), where the cache matters most.
cd "$(dirname "$0")" || exit 1
SRC=$PWD
. ../common.sh
cd "$WORK" || exit 1

SMR_PATS="" "$SMR_CC" -O3 -DNAME=nn_base -c "$SRC/kernel.c" -o base.o || exit 1
SMR_PATS="$SRC/tile64.pat" "$SMR_CC" -O3 -DNAME=nn_ours -c "$SRC/kernel.c" -o ours.o || exit 1
SMR_PATS="" "$SMR_CC" -O3 -c "$SRC/hand.c" -o hand.o || exit 1
clang -O3 -c "$SRC/gemm.c" -o gemm.o && clang gemm.o base.o ours.o hand.o -o gemm || exit 1

shapes=("27 32 369664" "288 64 92416" "576 128 23104" "1152 256 5776"
        "2304 512 1444" "4608 1024 361")
[ -n "$BIG" ] && shapes+=("1024 1024 1024" "2048 2048 2048" "4096 4096 4096")

printf "%5s %5s %7s %9s %9s %9s %9s %9s  %s\n" K M N base ours hand ours/base hand/base equal
for s in "${shapes[@]}"; do ./gemm $s; done
