#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# FFmpeg 'arnndn' Filter Benchmark Script (SMR / ClangIR Evaluation)
#
# Purpose:
#   Automates building FFmpeg and benchmarking the RNNoise-based audio denoising
#   filter ('arnndn') under two compilation configurations:
#     1. WITHOUT_PAT: All ClangIR-compatible translation units compiled via
#        'smr-cc' with pattern rewriting disabled (SMR_PATS="").
#     2. WITH_PAT: All ClangIR-compatible translation units compiled via
#        'smr-cc' with pattern matching and rewriting enabled (SMR_PATS=<pat>).
#
# Usage:
#   ./benchmark_ffmpeg.sh [path/to/pat.pat_or_directory]
#
# Optional Environment Variables:
#   SMR_CC      - Compiler wrapper binary (default: smr-cc)
#   PAT_FILE    - Fallback path to .pat file or directory (default: ./pat.pat)
#   NUM_RUNS    - Number of benchmark iterations per phase (default: 10)
#   DURATION    - Synthetic test media duration in seconds (default: 6180 = 1h43m)
#   MODEL_FILE  - Optional path to a local RNNoise model file (std.rnnn)
# ==============================================================================

# ------------------------------------------------------------------------------
# 1. Configuration & Prerequisite Validation
# ------------------------------------------------------------------------------
SMR_CC="${SMR_CC:-smr-cc}"
PAT_INPUT="${1:-${PAT_FILE:-./pat.pat}}"
NUM_RUNS="${NUM_RUNS:-10}"
DURATION="${DURATION:-6180}"
USER_MODEL_FILE="${MODEL_FILE:-}"
ROOT_DIR="$(pwd)"
REPO_URL="https://github.com/ffmpeg/ffmpeg.git"

# Ensure the SMR compiler wrapper is available in PATH
if ! command -v "$SMR_CC" >/dev/null 2>&1; then
  echo "Error: compiler wrapper '$SMR_CC' not found in PATH." >&2
  exit 1
fi

# Ensure standard Clang is available for initial builds and ClangIR fallbacks
if ! command -v clang >/dev/null 2>&1; then
  echo "Error: standard compiler 'clang' not found in PATH." >&2
  exit 1
fi

# Ensure the provided pattern file or directory exists
if [ ! -e "$PAT_INPUT" ]; then
  echo "Error: pattern path not found at '$PAT_INPUT'." >&2
  exit 1
fi

# Resolve absolute paths before changing directories
REAL_SMR_CC="$(command -v "$SMR_CC")"
STD_CLANG="$(command -v clang)"
PAT_INPUT="$(realpath "$PAT_INPUT")"

# ------------------------------------------------------------------------------
# 2. Isolated Workspace & Automatic Cleanup
# ------------------------------------------------------------------------------
TMP_DIR="$(mktemp -d)"
CLONE_DIR="$TMP_DIR/ffmpeg"
INPUT_FILE="$CLONE_DIR/input_benchmark.mp4"
MODEL_FILE="$CLONE_DIR/std.rnnn"

# Always remove temporary clone, logs, and generated media on exit (success or error)
cleanup() {
  local exit_code=$?
  cd "$ROOT_DIR" 2>/dev/null || true
  if [ -n "${TMP_DIR:-}" ] && [ -d "$TMP_DIR" ]; then
    echo ""
    echo ">>> Cleaning up cloned FFmpeg repository and temporary files..."
    rm -rf "$TMP_DIR"
  fi
  exit "$exit_code"
}
trap cleanup EXIT

# ------------------------------------------------------------------------------
# 3. Helper Functions (Patterns, RNNoise Model, and BLAS Detection)
# ------------------------------------------------------------------------------

# Resolves the pattern input: returns the file directly if a single .pat file is
# given, or concatenates all *.pat files recursively if a directory is given.
resolve_pat_file() {
  local target="$1"
  local out_pat="$TMP_DIR/combined_patterns.pat"

  if [ -f "$target" ]; then
    echo "$target"
    return 0
  fi

  if [ -d "$target" ]; then
    local pat_list=()
    while IFS= read -r -d '' f; do
      pat_list+=("$f")
    done < <(find "$target" -type f -name '*.pat' -print0 | sort -z)

    if [ "${#pat_list[@]}" -eq 0 ]; then
      echo "Error: no .pat files found in directory '$target'." >&2
      exit 1
    fi

    : > "$out_pat"
    local pf
    for pf in "${pat_list[@]}"; do
      printf '/* === %s === */\n' "$(basename "$pf")" >> "$out_pat"
      cat "$pf" >> "$out_pat"
      printf '\n\n' >> "$out_pat"
    done
    echo "$out_pat"
    return 0
  fi

  echo "Error: invalid pattern path '$target'." >&2
  exit 1
}

# Obtains a valid RNNoise weight file ('std.rnnn') required by FFmpeg's arnndn
# filter: uses a local user file if provided, downloads from upstream GitHub
# repositories, or generates deterministic synthetic weights if offline.
prepare_rnnn_model() {
  if [ -n "$USER_MODEL_FILE" ] && [ -f "$USER_MODEL_FILE" ]; then
    cp "$USER_MODEL_FILE" "$MODEL_FILE"
    echo ">>> Using user-provided RNNoise model: $USER_MODEL_FILE"
    return 0
  fi

  echo ">>> Fetching RNNoise model file (std.rnnn)..."
  local urls=(
    "https://raw.githubusercontent.com/richardpl/arnndn-models/master/std.rnnn"
    "https://raw.githubusercontent.com/richardpl/arnndn-models/master/bd.rnnn"
  )
  local u
  for u in "${urls[@]}"; do
    if command -v curl >/dev/null 2>&1 && curl -fsSL "$u" -o "$MODEL_FILE" 2>/dev/null; then
      if head -n 1 "$MODEL_FILE" | grep -q "rnnoise-nu model file version 1"; then
        echo "    Downloaded model from $u -> std.rnnn"
        return 0
      fi
    elif command -v wget >/dev/null 2>&1 && wget -qO "$MODEL_FILE" "$u" 2>/dev/null; then
      if head -n 1 "$MODEL_FILE" | grep -q "rnnoise-nu model file version 1"; then
        echo "    Downloaded model from $u -> std.rnnn"
        return 0
      fi
    fi
  done

  echo "    Offline fallback: generating valid synthetic 'rnnoise-nu model file version 1' weights..."
  LC_ALL=C awk 'BEGIN {
    print "rnnoise-nu model file version 1"
    for (i = 1; i <= 87525; i++) {
      printf "%d%s", ((i * 17) % 127) - 63, (i % 20 == 0 ? "\n" : " ")
    }
  }' > "$MODEL_FILE"
}

# Probes linker flags for OpenBLAS / CBLAS / BLAS so rewritten BLAS calls
# (e.g., cblas_sdot) resolve cleanly during FFmpeg linking.
detect_blas_libs() {
  local probe_c="$TMP_DIR/blas_probe.c"
  local probe_bin="$TMP_DIR/blas_probe"
  echo "int main(void) { return 0; }" > "$probe_c"
  local lib
  for lib in "-lopenblas" "-lcblas -lblas" "-lblas"; do
    if "$STD_CLANG" "$probe_c" $lib -o "$probe_bin" >/dev/null 2>&1; then
      echo "$lib"
      return 0
    fi
  done
  echo ""
}

# Appends common OpenBLAS/local header directories to CPATH so smr-cc can
# locate <cblas.h> without modifying FFmpeg's build flags.
setup_cpath() {
  local extra=""
  local d
  for d in "/usr/include/openblas" "/usr/local/include"; do
    if [ -d "$d" ]; then
      extra="${extra:+${extra}:}${d}"
    fi
  done
  if [ -n "$extra" ]; then
    export CPATH="${extra}${CPATH:+:${CPATH}}"
  fi
}

PAT_FILE="$(resolve_pat_file "$PAT_INPUT")"
setup_cpath

echo "=== Starting FFmpeg 'arnndn' Filter Benchmark ==="
echo "Date          : $(LC_ALL=C date)"
echo "SMR Compiler  : $REAL_SMR_CC (used for ALL compatible TUs in BOTH phases)"
echo "Fallback CC   : $STD_CLANG (used only when ClangIR fails / NYI)"
echo "Pattern File  : $PAT_FILE"
echo "Input Duration: ${DURATION} s"
echo "Runs / Phase  : $NUM_RUNS"
echo "Workspace     : $CLONE_DIR"
echo ""

EXTRA_BLAS_LIBS="$(detect_blas_libs)"

# ------------------------------------------------------------------------------
# 4. Clone & Minimal Pure-C FFmpeg Configuration
# ------------------------------------------------------------------------------
echo ">>> Cloning FFmpeg repository..."
git clone --depth 1 "$REPO_URL" "$CLONE_DIR" >/dev/null 2>&1

cd "$CLONE_DIR"
prepare_rnnn_model

EXTRA_CFG_FLAGS=()
if [ -n "$EXTRA_BLAS_LIBS" ]; then
  EXTRA_CFG_FLAGS+=(--extra-libs="$EXTRA_BLAS_LIBS")
fi

# Configure a minimal FFmpeg build enabling only the components required to:
#   (a) generate the synthetic test video/audio (testsrc, sine, scale, mpeg4, aac)
#   (b) execute the target audio denoising filter (arnndn, aresample, aformat)
# Assembly optimizations (--disable-x86asm, --disable-inline-asm) are disabled so
# that C loops remain exposed for source-level pattern matching by SMR.
echo ">>> Configuring FFmpeg..."
CFG_OUT="$TMP_DIR/configure.log"
if ! ./configure \
  --cc="$STD_CLANG" \
  --cxx="$STD_CLANG" \
  --disable-everything \
  --enable-ffmpeg \
  --disable-ffprobe \
  --enable-protocol=file,pipe \
  --enable-indev=lavfi \
  --enable-demuxer=mov,matroska,lavfi \
  --enable-muxer=mp4,null \
  --enable-decoder=aac,mpeg4,pcm_s16le,pcm_f32le,pcm_f64le,rawvideo,wrapped_avframe \
  --enable-encoder=aac,mpeg4,pcm_s16le \
  --enable-filter=arnndn,scale,format,aresample,aformat,anull,abuffer,abuffersink,sine,testsrc,color,buffer,buffersink,null \
  --disable-extra-warnings \
  --disable-x86asm \
  --disable-inline-asm \
  --disable-doc \
  --disable-debug \
  "${EXTRA_CFG_FLAGS[@]}" >"$CFG_OUT" 2>&1; then
  echo "Error: FFmpeg configure failed:" >&2
  cat "$CFG_OUT" >&2
  exit 1
fi

# ------------------------------------------------------------------------------
# 5. Initial Baseline Build with Standard Clang
# ------------------------------------------------------------------------------
# Produces a working 'ffmpeg' binary to generate the deterministic benchmark
# media and populates baseline .o files for any translation units that ClangIR
# does not yet support.
echo ">>> Building initial FFmpeg with standard clang (fallback objects & media generator)..."
BASE_BUILD_OUT="$TMP_DIR/build_base.log"
if ! make -j"$(nproc)" >"$BASE_BUILD_OUT" 2>&1; then
  echo "Error: initial FFmpeg build failed:" >&2
  cat "$BASE_BUILD_OUT" >&2
  exit 1
fi

# ------------------------------------------------------------------------------
# 6. Parallel Selective Compilation with smr-cc (With Fallback to Standard Clang)
# ------------------------------------------------------------------------------
# Extracts all C compilation commands from FFmpeg's Makefile via a dry-run,
# compiles every compatible translation unit in parallel using smr-cc (both
# rewritten and non-rewritten units), falls back to standard clang when ClangIR
# encounters unimplemented features (NYI), and relinks the final binary.
compile_all_possible_with_smr() {
  local label="$1"
  local pat_path="$2"
  local work_dir="$TMP_DIR/smr_jobs_${label}"
  local dry_run_log="$TMP_DIR/make_dry_run_${label}.log"
  local worker_sh="$work_dir/run_one.sh"

  rm -rf "$work_dir"
  mkdir -p "$work_dir"

  # Query Makefile for exact compiler invocations without executing them
  make -B -n V=1 >"$dry_run_log" 2>&1

  # Parse 'obj_rel|src_rel|flags' tuples from compiler commands, filtering out
  # dependency-generation flags (-MMD/-MF/...) and auxiliary build/test directories.
  LC_ALL=C awk '
    / -c / && / -o / {
      n = split($0, tok, /[[:space:]]+/)
      src = ""
      obj = ""
      flags = ""
      for (i = 1; i <= n; i++) {
        t = tok[i]
        if (t == "") continue
        if (t == "-o" && i < n) {
          obj = tok[++i]
        } else if (t ~ /^-o.+/) {
          obj = substr(t, 3)
        } else if (t == "-c") {
          continue
        } else if (t == "-MMD" || t == "-MD") {
          continue
        } else if ((t == "-MF" || t == "-MT" || t == "-MQ") && i < n) {
          i++
          continue
        } else if (t ~ /\.c$/ && t !~ /^-/) {
          src = t
        } else if (i > 1 && t != "&&" && t != ";") {
          flags = (flags == "" ? t : flags "\t" t)
        }
      }
      if (src != "" && obj != "" && !seen[obj]++ && src !~ /^(ffbuild|doc|tests)\//) {
        print obj "|" src "|" flags
      }
    }
  ' "$dry_run_log" > "$work_dir/targets.txt"

  local total_targets
  total_targets=$(wc -l < "$work_dir/targets.txt" | tr -d ' ')
  echo ">>> Compiling all compatible C translation units ($total_targets total) via smr-cc ($label)..."

  # Worker script executed per C translation unit
  cat > "$worker_sh" <<'EOF'
#!/usr/bin/env bash
set -u
idx="$1"
line="$2"
work_dir="$3"
pat_path="$4"
real_smr_cc="$5"
std_clang="$6"

IFS='|' read -r obj_rel src_rel flags_tsv <<< "$line"
flags=()
if [ -n "$flags_tsv" ]; then
  IFS=$'\t' read -r -a flags <<< "$flags_tsv"
fi

# Explicitly compile format-negotiation files with standard clang to avoid a
# known ClangIR -O3 codegen bug that triggers 'Duplicated pixel format' at runtime.
case "$src_rel" in
  *libavfilter/formats.c|*libavfilter/vf_format.c)
    "$std_clang" "${flags[@]}" -c "$src_rel" -o "$obj_rel" >/dev/null 2>&1
    printf 'FALLBACK_CLANG|%s\n' "$src_rel" > "$work_dir/status_${idx}.txt"
    exit 0
    ;;
esac

tmp_obj="$work_dir/obj_${idx}.o"
job_log="$work_dir/job_${idx}.log"
: > "$job_log"

# Attempt compilation via smr-cc:
#   -fno-builtin               : prevents builtin lowering unsupported by ClangIR
#   -Wframe-larger-than=...    : suppresses stack frame warnings from CIR lowering
#   -fno-omit-frame-pointer    : ensures valid frame pointer generation in CIR
#   -Dcold=                    : neutralizes __attribute__((cold)) for ClangIR
if SMR_PATS="$pat_path" \
   SMR_CC_LOG="$job_log" \
   CCC_OVERRIDE_OPTIONS="+-fno-builtin +-Wframe-larger-than=4294967295 +-fno-omit-frame-pointer" \
   "$real_smr_cc" -Dcold= "${flags[@]}" -c "$src_rel" -o "$tmp_obj" >/dev/null 2>&1 && [ -s "$tmp_obj" ]; then
  # Install the smr-cc generated .o for EVERY successfully compiled unit
  # (both rewritten and non-rewritten) and update its timestamp for 'make'.
  cp "$tmp_obj" "$obj_rel"
  touch "$obj_rel"

  # Extract the number of applied pattern rewrites from smr-cc's status log
  rw=$(LC_ALL=C awk '/^OK / {
    for (i = 1; i <= NF; i++) {
      if ($i ~ /^rewrites=/) {
        split($i, a, "=")
        print a[2] + 0
        exit
      }
    }
  }' "$job_log")

  one_line_log=$(tr '\n' ' ' < "$job_log" | sed 's/[[:space:]]*$//')
  if [ "${rw:-0}" -gt 0 ]; then
    printf 'SMR_REWRITTEN|%s|%s|%s|%s\n' "$src_rel" "$obj_rel" "$rw" "$one_line_log" > "$work_dir/status_${idx}.txt"
  else
    printf 'SMR_OK|%s\n' "$src_rel" > "$work_dir/status_${idx}.txt"
  fi
else
  # Fall back to standard clang if smr-cc/ClangIR fails on this translation unit
  "$std_clang" "${flags[@]}" -c "$src_rel" -o "$obj_rel" >/dev/null 2>&1
  printf 'FALLBACK_CLANG|%s\n' "$src_rel" > "$work_dir/status_${idx}.txt"
fi
EOF
  chmod +x "$worker_sh"

  # Dispatch compilation jobs across all available CPU cores
  local jobs
  jobs="$(nproc)"
  local idx=0
  while IFS= read -r line; do
    idx=$((idx + 1))
    "$worker_sh" "$idx" "$line" "$work_dir" "$pat_path" "$REAL_SMR_CC" "$STD_CLANG" &
    while [ "$(jobs -rp | wc -l)" -ge "$jobs" ]; do
      wait -n 2>/dev/null || wait
    done
  done < "$work_dir/targets.txt"
  wait

  # Aggregate compilation statistics and report rewritten files
  local smr_ok=0
  local smr_rw=0
  local fb_clang=0
  local total_rw=0
  local st_file kind src_rel obj_rel rw_cnt log_line

  if [ -n "$pat_path" ]; then
    echo "    Files with SMR pattern rewrites:"
  fi

  for st_file in "$work_dir"/status_*.txt; do
    [ -f "$st_file" ] || continue
    IFS='|' read -r kind src_rel obj_rel rw_cnt log_line < "$st_file"
    case "$kind" in
      SMR_REWRITTEN)
        smr_rw=$((smr_rw + 1))
        total_rw=$((total_rw + rw_cnt))
        if [ -n "$pat_path" ]; then
          echo "      - $src_rel -> $obj_rel (rewrites=$rw_cnt) [$log_line]"
        fi
        ;;
      SMR_OK)
        smr_ok=$((smr_ok + 1))
        ;;
      FALLBACK_CLANG)
        fb_clang=$((fb_clang + 1))
        ;;
    esac
  done

  if [ -n "$pat_path" ] && [ "$smr_rw" -eq 0 ]; then
    echo "      (none)"
  fi

  local total_smr=$((smr_rw + smr_ok))
  echo "    Summary ($label): $total_smr/$total_targets TUs compiled and linked via smr-cc ($smr_rw with rewrites [$total_rw total], $smr_ok without rewrites); $fb_clang TUs via clang fallback"

  # Relink the FFmpeg executable using the newly installed object files
  local link_out="$TMP_DIR/relink_${label}.log"
  if ! make -j"$jobs" >"$link_out" 2>&1; then
    echo "Error: FFmpeg relink for $label failed:" >&2
    cat "$link_out" >&2
    exit 1
  fi
}

# ------------------------------------------------------------------------------
# 7. Benchmark Media Generation & Execution Helpers
# ------------------------------------------------------------------------------

# Parses FFmpeg's internal '-benchmark' real-time metric (rtime=X.XXXs)
extract_rtime() {
  LC_ALL=C awk '/bench:.*rtime=/ {
    for (i = 1; i <= NF; i++) {
      if ($i ~ /^rtime=/) {
        sub(/^rtime=/, "", $i)
        sub(/s$/, "", $i)
        print $i
        exit
      }
    }
  }' "$1"
}

# Generates the synthetic MP4 input:
# 1920x1080 @ 30fps MPEG-4 video + 44.1 kHz stereo 127 kbps AAC audio.
generate_input() {
  local gen_out="$TMP_DIR/gen_input.log"
  echo ">>> Generating input_benchmark.mp4 (testsrc 1920x1080@30 + sine 1000Hz AAC 44100Hz stereo 127k, ${DURATION}s)..."
  if ! ./ffmpeg -y -nostdin -hide_banner \
    -f lavfi -i "testsrc=size=1920x1080:rate=30:duration=${DURATION}" \
    -f lavfi -i "sine=frequency=1000:sample_rate=44100:duration=${DURATION}" \
    -c:v mpeg4 -q:v 6 \
    -c:a aac -ac 2 -ar 44100 -b:a 127k \
    input_benchmark.mp4 >"$gen_out" 2>&1; then
    echo "Error: failed to generate input_benchmark.mp4:" >&2
    cat "$gen_out" >&2
    exit 1
  fi
}

# Executes the 'arnndn' audio filter benchmark NUM_RUNS times (-vn discards
# video decoding/filtering overhead) and computes average rtime and wall-clock time.
run_benchmark_phase() {
  local label="$1"
  local run_out="$TMP_DIR/run_${label}.log"

  echo ">>> Running 'arnndn' filter benchmark $NUM_RUNS times ($label)..."
  local sum_rtime="0"
  local sum_wall="0"
  local run start_t end_t wall_t rtime_t avg_rtime avg_wall

  for run in $(seq 1 "$NUM_RUNS"); do
    start_t=$(LC_ALL=C date +%s.%N)
    if ! ./ffmpeg -nostdin -hide_banner -benchmark \
      -i input_benchmark.mp4 \
      -vn -af arnndn=m=std.rnnn \
      -f null - >"$run_out" 2>&1; then
      echo "Error: FFmpeg 'arnndn' execution failed ($label, run $run/$NUM_RUNS):" >&2
      cat "$run_out" >&2
      exit 1
    fi
    end_t=$(LC_ALL=C date +%s.%N)

    wall_t=$(LC_ALL=C awk -v s="$start_t" -v e="$end_t" 'BEGIN { printf "%.6f", e - s }')
    rtime_t=$(extract_rtime "$run_out")

    if [ -z "$rtime_t" ]; then
      echo "Error: Could not parse benchmark rtime from FFmpeg output ($label, run $run/$NUM_RUNS):" >&2
      cat "$run_out" >&2
      exit 1
    fi

    echo "    Run ${run}/${NUM_RUNS}: rtime=${rtime_t} s, wall-clock=${wall_t} s"

    sum_rtime=$(LC_ALL=C awk -v a="$sum_rtime" -v b="$rtime_t" 'BEGIN { printf "%.9f", a + b }')
    sum_wall=$(LC_ALL=C awk -v a="$sum_wall" -v b="$wall_t" 'BEGIN { printf "%.9f", a + b }')
  done

  avg_rtime=$(LC_ALL=C awk -v s="$sum_rtime" -v n="$NUM_RUNS" 'BEGIN { printf "%.6f", s / n }')
  avg_wall=$(LC_ALL=C awk -v s="$sum_wall" -v n="$NUM_RUNS" 'BEGIN { printf "%.6f", s / n }')

  echo "    Average (${NUM_RUNS} runs): rtime=${avg_rtime} s, wall-clock=${avg_wall} s"

  # Store phase averages dynamically in AVG_WALL_<label> and AVG_RTIME_<label>
  eval "AVG_WALL_${label}='$avg_wall'"
  eval "AVG_RTIME_${label}='$avg_rtime'"
}

# ------------------------------------------------------------------------------
# 8. Main Benchmark Execution & Speedup Summary
# ------------------------------------------------------------------------------

# Generate the shared deterministic input file once using the initial build
generate_input

# Phase 1: Baseline run using smr-cc without any rewrite patterns
echo ""
echo "=========================================="
echo ">>> Phase 1: WITHOUT_PAT (all compatible TUs via smr-cc, SMR_PATS='')"
echo "=========================================="
compile_all_possible_with_smr "WITHOUT_PAT" ""
run_benchmark_phase "WITHOUT_PAT"

# Phase 2: Optimized run using smr-cc with the target .pat pattern rules
echo ""
echo "=========================================="
echo ">>> Phase 2: WITH_PAT (all compatible TUs via smr-cc, SMR_PATS='$PAT_FILE')"
echo "=========================================="
compile_all_possible_with_smr "WITH_PAT" "$PAT_FILE"
run_benchmark_phase "WITH_PAT"

# Compute speedup ratios (baseline / optimized) and relative percentage improvements
SPEEDUP_RTIME_RATIO=$(LC_ALL=C awk -v base="$AVG_RTIME_WITHOUT_PAT" -v opt="$AVG_RTIME_WITH_PAT" 'BEGIN {
  if (opt > 0) printf "%.4fx", base / opt; else print "N/A"
}')
SPEEDUP_RTIME_PCT=$(LC_ALL=C awk -v base="$AVG_RTIME_WITHOUT_PAT" -v opt="$AVG_RTIME_WITH_PAT" 'BEGIN {
  if (base > 0) printf "%.2f%%", ((base - opt) / base) * 100.0; else print "N/A"
}')

SPEEDUP_WALL_RATIO=$(LC_ALL=C awk -v base="$AVG_WALL_WITHOUT_PAT" -v opt="$AVG_WALL_WITH_PAT" 'BEGIN {
  if (opt > 0) printf "%.4fx", base / opt; else print "N/A"
}')
SPEEDUP_WALL_PCT=$(LC_ALL=C awk -v base="$AVG_WALL_WITHOUT_PAT" -v opt="$AVG_WALL_WITH_PAT" 'BEGIN {
  if (base > 0) printf "%.2f%%", ((base - opt) / base) * 100.0; else print "N/A"
}')

echo ""
echo "=========================================="
echo "=== ARNNDN BENCHMARK RESULTS (${NUM_RUNS} RUNS) ==="
echo "=========================================="
echo "Avg rtime (FFmpeg -benchmark) WITHOUT pat.pat (smr-cc) : ${AVG_RTIME_WITHOUT_PAT} s"
echo "Avg rtime (FFmpeg -benchmark) WITH pat.pat    (smr-cc) : ${AVG_RTIME_WITH_PAT} s"
echo "Speedup (avg rtime)                                    : ${SPEEDUP_RTIME_RATIO} (${SPEEDUP_RTIME_PCT} improvement)"
echo "------------------------------------------"
echo "Avg wall-clock time WITHOUT pat.pat (smr-cc)           : ${AVG_WALL_WITHOUT_PAT} s"
echo "Avg wall-clock time WITH pat.pat    (smr-cc)           : ${AVG_WALL_WITH_PAT} s"
echo "Speedup (avg wall-clock)                               : ${SPEEDUP_WALL_RATIO} (${SPEEDUP_WALL_PCT} improvement)"
echo "=========================================="