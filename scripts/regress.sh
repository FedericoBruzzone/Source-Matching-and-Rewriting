#!/usr/bin/env bash
#
# Check that a change does not alter what SMR produces on the SMR-artifacts
# cases: run the smr built from REF and the smr built from the working tree on
# every case, then diff the generated MLIR and logs.
#
# Usage: scripts/regress.sh [REF]
#
# Run it where the SMR toolchain is available (dev image or devcontainer).
# The working tree must already be configured in BUILD_DIR. REF defaults to
# main and its smr is built once per commit under WORK_DIR.
#
# Cases:
#   angha_rewrite   every case with its pat.pat
#   angha_match     every C file with every pattern in angha_match/pat, each
#                   pattern serialized once per smr like angha_match/script.py
#   polybench       every PolyBench/C kernel with polybench-c-3.2/pat.pat
#
# Environment:
#   BUILD_DIR  Build directory of the working tree (default: build)
#   WORK_DIR   REF builds and outputs (default: $BUILD_DIR/regress)
#   JOBS       Parallel smr runs (default: number of CPUs)
#   SKIP_PATTERNS
#              angha_match patterns to leave out (default: gemm-c, which holds
#              86016 generated variants and takes hours to serialize)
#   CC, CXX    Compilers for the REF build (default: clang-20, clang++-20)
#
# Exit status: 0 if the MLIR is identical, 1 otherwise. Log-only differences
# are reported but do not fail.

set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
REF=${1:-main}
BUILD_DIR=$ROOT/${BUILD_DIR:-build}
WORK_DIR=${WORK_DIR:-$BUILD_DIR/regress}
JOBS=${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu)}
SKIP_PATTERNS=${SKIP_PATTERNS-gemm-c}
ARTIFACTS=$ROOT/artifacts

git_() { git -c safe.directory='*' -C "$ROOT" "$@"; }

# Some cases crash smr: do not leave core files in the repo.
ulimit -c 0

if [ ! -d "$ARTIFACTS/angha_rewrite" ]; then
  echo "artifacts is not checked out, run: git submodule update --init" >&2
  exit 1
fi
if [ ! -f "$BUILD_DIR/build.ninja" ]; then
  echo "$BUILD_DIR is not configured, build the working tree first" >&2
  exit 1
fi

SHA=$(git_ rev-parse --verify "$REF^{commit}")
REF_DIR=$WORK_DIR/$SHA
HEAD_DIR=$WORK_DIR/working-tree
INPUTS=$WORK_DIR/inputs

mkdir -p "$WORK_DIR"
echo "Building smr from the working tree"
ninja -C "$BUILD_DIR" smr > "$WORK_DIR/working-tree-build.log" 2>&1 ||
  { cat "$WORK_DIR/working-tree-build.log"; exit 1; }

if [ ! -x "$REF_DIR/build/smr/smr" ]; then
  echo "Building smr at $REF (${SHA:0:7}), once per commit"
  rm -rf "$REF_DIR"
  mkdir -p "$REF_DIR/src"
  git_ archive "$SHA" | tar -x -C "$REF_DIR/src"
  {
    cmake -G Ninja -S "$REF_DIR/src" -B "$REF_DIR/build" \
      -DCMAKE_C_COMPILER="${CC:-clang-20}" -DCMAKE_CXX_COMPILER="${CXX:-clang++-20}" &&
      ninja -C "$REF_DIR/build" smr
  } > "$REF_DIR/build.log" 2>&1 || { cat "$REF_DIR/build.log"; exit 1; }
fi

# One case per line: name, input, pattern, CPATH ("-" for none).
CASES=$WORK_DIR/cases.txt
: > "$CASES"

for dir in "$ARTIFACTS"/angha_rewrite/*/; do
  case_name=$(basename "$dir")
  for input in "$dir"*.c; do
    echo "angha_rewrite/$case_name/$(basename "$input" .c) $input ${dir}pat.pat -" >> "$CASES"
  done
done

# "@opat/" is replaced by the serialized patterns of the smr that runs the case.
MATCH_PATTERNS=()
for pat in "$ARTIFACTS"/angha_match/pat/*.pat; do
  pat_name=$(basename "$pat" .pat)
  case " $SKIP_PATTERNS " in *" $pat_name "*) continue ;; esac
  MATCH_PATTERNS+=("$pat")
  for input in "$ARTIFACTS"/angha_match/*/*.c; do
    project=$(basename "$(dirname "$input")")
    echo "angha_match/$pat_name/$project/$(basename "$input" .c) $input @opat/$pat_name.opat -" >> "$CASES"
  done
done

# Same preparation as polybench-c-3.2/test_smr.sh: timing and array dumps on.
POLYBENCH=$ARTIFACTS/polybench-c-3.2
mkdir -p "$INPUTS/polybench"
while read -r bench; do
  bench=${bench#./}
  name=$(basename "$bench" .c)
  input=$INPUTS/polybench/$name.c
  { echo "#define POLYBENCH_TIME 1"; echo "#define POLYBENCH_DUMP_ARRAYS 1"; cat "$POLYBENCH/$bench"; } > "$input"
  echo "polybench/$name $input $POLYBENCH/pat.pat $POLYBENCH/utilities:$POLYBENCH/$(dirname "$bench")" >> "$CASES"
done < "$POLYBENCH/utilities/benchmark_list"

run_case() {
  local smr=$1 out=$2 name=$3 input=$4 pat=$5 cpath=$6 status
  local side
  side=$(dirname "$out")
  [ "$cpath" = - ] && cpath=
  pat=${pat/#@opat\//$side/opat/}
  mkdir -p "$(dirname "$out/$name")"
  # stdout and stderr go to separate files: interleaved, their order changes
  # from run to run.
  # The subshell keeps bash's "Aborted (core dumped)" notice out of the output.
  status=$( (CPATH=$cpath "$smr" "$input" "$pat" -o "$out/$name.mlir" \
    > "$out/$name.log" 2> "$out/$name.err"; echo $?) 2> /dev/null)
  echo "exit status: $status" >> "$out/$name.log"
  # smr compiles each input through a random /tmp file that ends up in the
  # locations, the logs mention the output and pattern directories, and crash
  # stack frames hold addresses and paths that change between builds.
  for f in "$out/$name.mlir" "$out/$name.log" "$out/$name.err"; do
    [ -f "$f" ] && sed -i -E -e 's#/tmp/frontend_[A-Za-z0-9]+#<tmp>#g' -e "s#$side/#<side>/#g" \
      -e '/^ *#[0-9]+ 0x[0-9a-f]+ /d' "$f"
  done
  return 0
}
export -f run_case

NUM_CASES=$(wc -l < "$CASES")
for side in "$REF_DIR/build/smr/smr:$REF_DIR/out" "$BUILD_DIR/smr/smr:$HEAD_DIR/out"; do
  smr=${side%%:*}
  out=${side#*:}
  opat=$(dirname "$out")/opat
  rm -rf "$out" "$opat"
  mkdir -p "$out" "$opat"
  echo "Serializing ${#MATCH_PATTERNS[@]} angha_match patterns with $smr"
  printf '%s\n' "${MATCH_PATTERNS[@]}" |
    xargs -P "$JOBS" -I {} bash -c \
      'name=$(basename "$1" .pat); "$0" "$1" --serialize="$2/$name.opat" > "$2/$name.log" 2>&1' \
      "$smr" {} "$opat" ||
    { echo "Serialization failed, see $opat/*.log" >&2; exit 1; }
  echo "Running $NUM_CASES cases with $smr"
  xargs -P "$JOBS" -L 1 bash -c 'run_case "$0" "$1" "$2" "$3" "$4" "$5"' "$smr" "$out" < "$CASES"
done

# A file missing on both sides (no rewrite, so no output) counts as identical.
same() {
  [ ! -e "$1" ] && [ ! -e "$2" ] && return 0
  [ -e "$1" ] && [ -e "$2" ] && cmp -s "$1" "$2"
}

MLIR_DIFFS=()
LOG_DIFFS=()
while read -r name _; do
  ref=$REF_DIR/out/$name
  head=$HEAD_DIR/out/$name
  if ! same "$ref.mlir" "$head.mlir"; then
    MLIR_DIFFS+=("$name")
  elif ! same "$ref.log" "$head.log" || ! same "$ref.err" "$head.err"; then
    LOG_DIFFS+=("$name")
  fi
done < "$CASES"

echo
echo "Compared $NUM_CASES cases: $REF (${SHA:0:7}) vs working tree"
echo "  identical:          $((NUM_CASES - ${#MLIR_DIFFS[@]} - ${#LOG_DIFFS[@]}))"
echo "  different MLIR:     ${#MLIR_DIFFS[@]}"
echo "  different log only: ${#LOG_DIFFS[@]}"
echo "  smr crashes:        $(grep -rlE "^exit status: (13[0-9]|1[4-9][0-9])$" "$HEAD_DIR/out" | wc -l) in the working tree, $(grep -rlE "^exit status: (13[0-9]|1[4-9][0-9])$" "$REF_DIR/out" | wc -l) at $REF"
for name in ${MLIR_DIFFS[@]+"${MLIR_DIFFS[@]}"}; do
  echo "  MLIR  $name"
done
for name in ${LOG_DIFFS[@]+"${LOG_DIFFS[@]}"}; do
  echo "  log   $name"
done
if [ ${#MLIR_DIFFS[@]} -gt 0 ] || [ ${#LOG_DIFFS[@]} -gt 0 ]; then
  echo
  echo "Inspect with: diff -r $REF_DIR/out $HEAD_DIR/out"
fi

[ ${#MLIR_DIFFS[@]} -eq 0 ]
