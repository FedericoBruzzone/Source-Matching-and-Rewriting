#!/usr/bin/env bash
set -euo pipefail

# Usage: ./benchmark_darknet.sh [path/to/pat.pat]
SMR_CC="${SMR_CC:-smr-cc}"
PAT_FILE="${1:-${PAT_FILE:-./pat.pat}}"
NUM_RUNS="${NUM_RUNS:-10}"
ROOT_DIR="$(pwd)"
REPO_URL="https://github.com/pjreddie/darknet.git"
WEIGHTS_URL="https://data.pjreddie.com/files/yolov3.weights"

if ! command -v "$SMR_CC" >/dev/null 2>&1; then
  echo "Error: compiler wrapper '$SMR_CC' not found in PATH." >&2
  exit 1
fi

if [ ! -f "$PAT_FILE" ]; then
  echo "Error: pattern file not found at '$PAT_FILE'." >&2
  exit 1
fi

# Resolve absolute path before changing directories
PAT_FILE="$(realpath "$PAT_FILE")"

TMP_DIR="$(mktemp -d)"
CLONE_DIR="$TMP_DIR/darknet"

cleanup() {
  local exit_code=$?
  cd "$ROOT_DIR" 2>/dev/null || true
  if [ -n "${TMP_DIR:-}" ] && [ -d "$TMP_DIR" ]; then
    echo ""
    echo ">>> Cleaning up cloned Darknet repository and temporary files..."
    rm -rf "$TMP_DIR"
  fi
  exit "$exit_code"
}
trap cleanup EXIT

echo "=== Starting Darknet Benchmark ==="
echo "Date        : $(LC_ALL=C date)"
echo "CC / CPP    : $(command -v "$SMR_CC")"
echo "Pattern File: $PAT_FILE"
echo "Runs / Phase: $NUM_RUNS"
echo "Workspace   : $CLONE_DIR"
echo ""

# 1. Clone repository into temporary workspace
echo ">>> Cloning Darknet repository..."
git clone "$REPO_URL" "$CLONE_DIR"

cd "$CLONE_DIR"

# 2. Replace deprecated -Ofast flag with -O3 -ffast-math
echo ">>> Patching Makefile (-Ofast -> -O3 -ffast-math)..."
sed -i 's/-Ofast/-O3 -ffast-math/g' Makefile

# 3. Download YOLOv3 weights
echo ">>> Downloading yolov3.weights..."
wget -O yolov3.weights "$WEIGHTS_URL"

extract_pred_time() {
  LC_ALL=C awk '/Predicted in/ {
    for (i = 1; i <= NF; i++) {
      if ($i == "in" && (i + 1) <= NF) {
        print $(i + 1)
        exit
      }
    }
  }' "$1"
}

summarize_cc_log() {
  LC_ALL=C awk '
    /^OK / {
      ok++
      for (i = 1; i <= NF; i++) {
        if ($i ~ /^rewrites=/) {
          split($i, a, "=")
          rewrites += a[2]
        }
      }
    }
    /^FALLBACK / { fb++ }
    END {
      printf "    Translation units: %d compiled via SMR (%d rewrites), %d fallbacks\n", ok + 0, rewrites + 0, fb + 0
    }
  ' "$1"
}

run_experiment() {
  local label="$1"
  local pats="$2"
  local cc_log="$TMP_DIR/cc_${label}.log"
  local build_out="$TMP_DIR/build_${label}.log"
  local run_out="$TMP_DIR/run_${label}.log"

  : > "$cc_log"

  echo ""
  echo "=========================================="
  echo ">>> Phase: $label (SMR_PATS='$pats')"
  echo "=========================================="

  echo ">>> Building Darknet ($label)..."
  if ! make clean >"$build_out" 2>&1; then
    echo "Error: 'make clean' failed ($label):" >&2
    cat "$build_out" >&2
    exit 1
  fi

  if ! SMR_PATS="$pats" SMR_CC_LOG="$cc_log" make -j"$(nproc)" CC="$SMR_CC" CPP="$SMR_CC" OPTS="-O3 -ffast-math" >"$build_out" 2>&1; then
    echo "Error: Darknet build failed ($label):" >&2
    cat "$build_out" >&2
    if [ -s "$cc_log" ]; then
      echo "--- Translation Unit Log ($label) ---" >&2
      cat "$cc_log" >&2
    fi
    exit 1
  fi
  summarize_cc_log "$cc_log"

  echo ">>> Running detection $NUM_RUNS times ($label)..."
  local sum_pred="0"
  local sum_wall="0"
  local run start_t end_t wall_t pred_t avg_pred avg_wall

  for run in $(seq 1 "$NUM_RUNS"); do
    start_t=$(LC_ALL=C date +%s.%N)
    if ! ./darknet detect cfg/yolov3.cfg yolov3.weights data/dog.jpg >"$run_out" 2>&1; then
      echo "Error: Darknet execution failed ($label, run $run/$NUM_RUNS):" >&2
      cat "$run_out" >&2
      exit 1
    fi
    end_t=$(LC_ALL=C date +%s.%N)

    wall_t=$(LC_ALL=C awk -v s="$start_t" -v e="$end_t" 'BEGIN { printf "%.6f", e - s }')
    pred_t=$(extract_pred_time "$run_out")

    if [ -z "$pred_t" ]; then
      echo "Error: Could not parse prediction time from Darknet output ($label, run $run/$NUM_RUNS):" >&2
      cat "$run_out" >&2
      exit 1
    fi

    echo "    Run ${run}/${NUM_RUNS}: prediction=${pred_t} s, wall-clock=${wall_t} s"

    sum_pred=$(LC_ALL=C awk -v a="$sum_pred" -v b="$pred_t" 'BEGIN { printf "%.9f", a + b }')
    sum_wall=$(LC_ALL=C awk -v a="$sum_wall" -v b="$wall_t" 'BEGIN { printf "%.9f", a + b }')
  done

  avg_pred=$(LC_ALL=C awk -v s="$sum_pred" -v n="$NUM_RUNS" 'BEGIN { printf "%.6f", s / n }')
  avg_wall=$(LC_ALL=C awk -v s="$sum_wall" -v n="$NUM_RUNS" 'BEGIN { printf "%.6f", s / n }')

  echo "    Average (${NUM_RUNS} runs): prediction=${avg_pred} s, wall-clock=${avg_wall} s"

  eval "AVG_WALL_${label}='$avg_wall'"
  eval "AVG_PRED_${label}='$avg_pred'"
}

# 4. Build and run WITHOUT pat.pat (baseline/control)
run_experiment "WITHOUT_PAT" ""

# 5. Build and run WITH pat.pat
run_experiment "WITH_PAT" "$PAT_FILE"

# 6. Compute Speedup from averages
SPEEDUP_PRED=$(LC_ALL=C awk -v base="$AVG_PRED_WITHOUT_PAT" -v opt="$AVG_PRED_WITH_PAT" 'BEGIN {
  if (opt > 0) printf "%.4fx", base / opt; else print "N/A"
}')

SPEEDUP_WALL=$(LC_ALL=C awk -v base="$AVG_WALL_WITHOUT_PAT" -v opt="$AVG_WALL_WITH_PAT" 'BEGIN {
  if (opt > 0) printf "%.4fx", base / opt; else print "N/A"
}')

echo ""
echo "=========================================="
echo "=== RESULTS & SPEEDUP (AVG OF ${NUM_RUNS} RUNS) ==="
echo "=========================================="
echo "Avg prediction time (Darknet) WITHOUT pat.pat : ${AVG_PRED_WITHOUT_PAT} s"
echo "Avg prediction time (Darknet) WITH pat.pat    : ${AVG_PRED_WITH_PAT} s"
echo "Speedup (avg prediction)                      : ${SPEEDUP_PRED}"
echo "------------------------------------------"
echo "Avg wall-clock time WITHOUT pat.pat           : ${AVG_WALL_WITHOUT_PAT} s"
echo "Avg wall-clock time WITH pat.pat              : ${AVG_WALL_WITH_PAT} s"
echo "Speedup (avg wall-clock)                      : ${SPEEDUP_WALL}"
echo "=========================================="