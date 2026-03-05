#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUT_DIR="benchmark_results_existing_${TIMESTAMP}"
mkdir -p "$OUT_DIR"

echo "=== Existing benchmark scheduler comparison ==="
echo "Output dir: $OUT_DIR"

ABT_PREFIX=""

resolve_argobots_flags() {
  if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists argobots; then
    ABT_CFLAGS="$(pkg-config --cflags argobots)"
    ABT_LIBS="$(pkg-config --libs argobots) -lpthread"
    ABT_PREFIX="$(pkg-config --variable=prefix argobots 2>/dev/null || true)"
    echo "Using Argobots via pkg-config"
    return
  fi

  local candidates=()
  if [ -n "${ARGOBOTS_INSTALL_DIR:-}" ]; then
    candidates+=("$ARGOBOTS_INSTALL_DIR")
  fi
candidates+=("$HOME/local/argobots" "$HOME/argobots-install" "/usr/local" "/usr")

  for dir in "${candidates[@]}"; do
    if [ -f "$dir/include/abt.h" ] && [ -d "$dir/lib" ]; then
      ABT_CFLAGS="-I$dir/include"
      ABT_LIBS="-L$dir/lib -labt -lpthread"
      ABT_PREFIX="$dir"
      echo "Using Argobots from $dir"
      return
    fi
  done

  echo "Argobots not found (abt.h/libabt)." >&2
  echo "Hint: set ARGOBOTS_INSTALL_DIR, configure pkg-config for argobots, or run ./scripts/install_argobots.sh." >&2
  exit 1
}

configure_argobots_env() {
  local inc_dir=""
  local lib_dir=""

  inc_dir=$(echo "$ABT_CFLAGS" | sed -n 's/.*-I\([^ ]*\).*/\1/p')
  lib_dir=$(echo "$ABT_LIBS" | sed -n 's/.*-L\([^ ]*\).*/\1/p')

  if [ -n "$inc_dir" ]; then
    export C_INCLUDE_PATH="$inc_dir${C_INCLUDE_PATH:+:$C_INCLUDE_PATH}"
  fi
  if [ -n "$lib_dir" ]; then
    export LIBRARY_PATH="$lib_dir${LIBRARY_PATH:+:$LIBRARY_PATH}"
    export DYLD_LIBRARY_PATH="$lib_dir${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
  fi
}

run_cg_argobots_fixed() {
  echo "[cg_argobots_fixed] build + run old/new scheduler"

  make -C cg_argobots_fixed clean >/dev/null
  make -C cg_argobots_fixed veryclean >/dev/null

  if [ -n "$ABT_PREFIX" ]; then
    make -C cg_argobots_fixed ARGOBOTS_INSTALL_DIR="$ABT_PREFIX" suite >/dev/null
  else
    local cg_c_inc="-I../common ${ABT_CFLAGS}"
    local cg_cflags="-g -Wall -O3 ${ABT_CFLAGS}"
    local cg_c_lib="-lm ${ABT_LIBS}"
    local cg_clinkflags="-O3"
    make -C cg_argobots_fixed \
      C_INC="$cg_c_inc" \
      CFLAGS="$cg_cflags" \
      C_LIB="$cg_c_lib" \
      CLINKFLAGS="$cg_clinkflags" \
      suite >/dev/null
  fi

  local csv="$OUT_DIR/cg_argobots_fixed_summary.csv"
  echo "scheduler,binary,xstreams,threads,time" > "$csv"

  local bins=(cg_argobots_fixed/bin/*)
  local configs=("1 1" "2 4" "4 8")
  for scheduler in old new; do
    for b in "${bins[@]}"; do
      [ -x "$b" ] || continue
      local bn
      bn=$(basename "$b")
      for cfg in "${configs[@]}"; do
        local xs th
        xs=$(echo "$cfg" | awk '{print $1}')
        th=$(echo "$cfg" | awk '{print $2}')
        local log="$OUT_DIR/${bn}_${scheduler}_x${xs}_t${th}.txt"
        ABT_WS_SCHEDULER=$scheduler "$b" -s "$xs" -t "$th" > "$log" 2>&1 || true
        local t
        t=$(grep -E "Time in seconds|Time in seconds =|Time in seconds   =" "$log" | tail -n1 | awk '{print $NF}')
        echo "$scheduler,$bn,$xs,$th,${t:-NA}" >> "$csv"
      done
    done
  done
}

run_jac3d_argobots_fixed() {
  echo "[jac3d_argobots_fixed] run old/new scheduler"
  (cd jac3d_argobots_fixed && ./test_jac3d_argobots.sh)
  cp jac3d_argobots_fixed/results_scheduler_compare/summary.csv "$OUT_DIR/jac3d_argobots_fixed_summary.csv"
}

run_reference_non_argobots() {
  echo "[reference] running existing non-Argobots benchmarks once (no scheduler switch)"
  make -C cg_openmp_fixed clean >/dev/null || true
  make -C cg_openmp_fixed suite >/dev/null || true
  local csv="$OUT_DIR/reference_non_argobots.csv"
  echo "benchmark,variant,threads_or_cfg,time" > "$csv"

  for b in cg_openmp_fixed/bin/*; do
    [ -x "$b" ] || continue
    local bn
    bn=$(basename "$b")
    local log="$OUT_DIR/${bn}_omp_t4.txt"
    OMP_NUM_THREADS=4 "$b" > "$log" 2>&1 || true
    local t
    t=$(grep -E "Time in seconds|Time in seconds =|Time in seconds   =" "$log" | tail -n1 | awk '{print $NF}')
    echo "cg_openmp_fixed,$bn,4,${t:-NA}" >> "$csv"
  done
}

resolve_argobots_flags
configure_argobots_env
run_cg_argobots_fixed
run_jac3d_argobots_fixed
run_reference_non_argobots

echo "Done. Summaries:"
find "$OUT_DIR" -maxdepth 1 -name "*.csv" -print