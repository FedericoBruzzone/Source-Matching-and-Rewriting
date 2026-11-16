# Sourced by the run.sh of each experiment. Needs clang, smr and cir-translate
# in PATH (the smr-dev container has them), scripts/smr-cc comes from the repo.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SMR_CC=${SMR_CC:-$ROOT/scripts/smr-cc}
WORK=${WORK:-$(mktemp -d)}
export SMR_CC_LOG=/dev/null

# Size in bytes of the symbol $2 (default kernel) in the object file $1.
size_of() {
  local hex
  hex=$(nm -S --defined-only "$1" | awk -v s="${2:-kernel}" '$4 == s { print $2 }')
  echo $((16#$hex))
}

# Runs the binary $1 three times and prints the median line, by the second
# column (the time in seconds).
median_run() {
  for _ in 1 2 3; do "$1"; done | sort -k2 -n | sed -n 2p
}
