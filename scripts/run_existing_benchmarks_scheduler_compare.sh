#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUT_DIR="benchmark_results_existing_${TIMESTAMP}"
mkdir -p "$OUT_DIR"

SCRIPT_VERSION="2026-03-12-macos-safe-ref"

# ------------------------------
# Config (override via env vars)
# ------------------------------
# Which benchmarks to run: comma-separated subset of: cg,jac3d,reference
# Default excludes reference (OpenMP) to stay portable on macOS/Apple Clang.
BENCHMARKS_TO_RUN="${BENCHMARKS_TO_RUN:-cg,jac3d}"
# Normalize optional spaces from user input, e.g. "cg, jac3d"
BENCHMARKS_TO_RUN="$(echo "$BENCHMARKS_TO_RUN" | tr -d "[:space:]")"

# Schedulers to compare for Argobots benchmarks.
SCHEDULERS="${SCHEDULERS:-old,new}"

# CG binaries/classes to run, e.g. "S W A"; empty => all binaries in cg_argobots_fixed/bin
CG_CLASSES="${CG_CLASSES:-}"
# Core/thread pairs for CG/Jac3D in format "x:t,x:t,..."
CORE_CONFIGS="${CORE_CONFIGS:-1:1,2:4,4:8}"
# Repeats for each (benchmark,scheduler,config)
NUM_RUNS="${NUM_RUNS:-1}"

# Jac3D build/run settings
JAC3D_BUILD_CFLAGS="${JAC3D_BUILD_CFLAGS:--O3 -Wall -Wextra}"

ABT_PREFIX=""
ABT_CFLAGS=""
ABT_LIBS=""

contains_item() {
  local needle="$1"
  local haystack_csv="$2"
  IFS=',' read -r -a items <<< "$haystack_csv"
  for it in "${items[@]}"; do
    if [ "$it" = "$needle" ]; then
      return 0
    fi
  done
  return 1
}

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

parse_time_from_log() {
  local log="$1"
  grep -E "Time in seconds|Time in seconds =|Time in seconds   =" "$log" | tail -n1 | awk '{print $NF}'
}

run_cg_argobots_fixed() {
  echo "[cg_argobots_fixed] build + run scheduler compare"

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
  echo "scheduler,binary,xstreams,threads,run,time" > "$csv"

  local bins=()
  if [ -n "$CG_CLASSES" ]; then
    # shellcheck disable=SC2206
    local classes=( $CG_CLASSES )
    for cls in "${classes[@]}"; do
      bins+=("cg_argobots_fixed/bin/cg.${cls}.x")
    done
  else
    bins=(cg_argobots_fixed/bin/*)
  fi

  IFS=',' read -r -a scheduler_list <<< "$SCHEDULERS"
  IFS=',' read -r -a cfg_list <<< "$CORE_CONFIGS"

  for scheduler in "${scheduler_list[@]}"; do
    for b in "${bins[@]}"; do
      [ -x "$b" ] || continue
      local bn
      bn=$(basename "$b")
      for cfg in "${cfg_list[@]}"; do
        local xs th
        xs="${cfg%%:*}"
        th="${cfg##*:}"
        for run in $(seq 1 "$NUM_RUNS"); do
          local log="$OUT_DIR/${bn}_${scheduler}_x${xs}_t${th}_run${run}.txt"
          # CG expects positional args: <xstreams> <threads>
          ABT_WS_SCHEDULER=$scheduler "$b" "$xs" "$th" > "$log" 2>&1 || true
          local t
          t=$(parse_time_from_log "$log")
          echo "$scheduler,$bn,$xs,$th,$run,${t:-NA}" >> "$csv"
        done
      done
    done
  done
}

build_jac3d_argobots_fixed() {
  echo "[jac3d_argobots_fixed] build"
  (
    cd jac3d_argobots_fixed
    gcc $JAC3D_BUILD_CFLAGS $ABT_CFLAGS \
      -o jac3d \
      jac3d.c abt_reduction.c \
      ../argobots_framework/examples/workstealing_scheduler/abt_workstealing_scheduler.c \
      ../argobots_framework/examples/workstealing_scheduler/abt_workstealing_scheduler_cost_aware.c \
      $ABT_LIBS -lm
  )
}

run_jac3d_argobots_fixed() {
  build_jac3d_argobots_fixed
  echo "[jac3d_argobots_fixed] run scheduler compare"

  local csv="$OUT_DIR/jac3d_argobots_fixed_summary.csv"
  echo "scheduler,xstreams,threads,run,time_seconds,real_time_nanos,verification" > "$csv"

  IFS=',' read -r -a scheduler_list <<< "$SCHEDULERS"
  IFS=',' read -r -a cfg_list <<< "$CORE_CONFIGS"

  for scheduler in "${scheduler_list[@]}"; do
    for cfg in "${cfg_list[@]}"; do
      local xs th
      xs="${cfg%%:*}"
      th="${cfg##*:}"
      for run in $(seq 1 "$NUM_RUNS"); do
        local log="$OUT_DIR/jac3d_${scheduler}_x${xs}_t${th}_run${run}.txt"
        ABT_WS_SCHEDULER=$scheduler ./jac3d_argobots_fixed/jac3d "$xs" "$th" > "$log" 2>&1 || true

        local t tn v
        t=$(grep "Time in seconds" "$log" | awk '{print $NF}' | tail -n1)
        tn=$(grep "Real time" "$log" | awk '{print $NF}' | tail -n1)
        v=$(grep "Verification" "$log" | awk '{print $NF}' | tail -n1)
        echo "$scheduler,$xs,$th,$run,${t:-NA},${tn:-NA},${v:-NA}" >> "$csv"
      done
    done
  done
}

run_reference_non_argobots() {
  local csv="$OUT_DIR/reference_non_argobots.csv"
  echo "benchmark,variant,threads_or_cfg,time,status" > "$csv"

  if [ "$(uname -s)" = "Darwin" ] && [ "${ALLOW_REFERENCE_ON_DARWIN:-0}" != "1" ]; then
    echo "[reference] skipped on macOS by default (Apple Clang lacks OpenMP flags used by cg_openmp_fixed)."
    echo "[reference] set ALLOW_REFERENCE_ON_DARWIN=1 to force attempt."
    echo "cg_openmp_fixed,NA,NA,NA,skipped_darwin_openmp" >> "$csv"
    return
  fi

  echo "[reference] running non-Argobots benchmarks once"
  make -C cg_openmp_fixed clean >/dev/null || true
  local build_log="$OUT_DIR/reference_openmp_build.log"
  if ! make -C cg_openmp_fixed suite >"$build_log" 2>&1; then
    echo "[reference] build failed; see $build_log"
    echo "cg_openmp_fixed,NA,NA,NA,build_failed" >> "$csv"
    return
  fi

  for b in cg_openmp_fixed/bin/*; do
    [ -x "$b" ] || continue
    local bn
    bn=$(basename "$b")
    local log="$OUT_DIR/${bn}_omp_t4.txt"
    OMP_NUM_THREADS=4 "$b" > "$log" 2>&1 || true
    local t
    t=$(parse_time_from_log "$log")
    echo "cg_openmp_fixed,$bn,4,${t:-NA},ok" >> "$csv"
  done
}

echo "=== Existing benchmark scheduler comparison ==="
echo "Script version: $SCRIPT_VERSION"
echo "Output dir: $OUT_DIR"
echo "Benchmarks: $BENCHMARKS_TO_RUN"
if [ "$(uname -s)" = "Darwin" ] && contains_item "reference" "$BENCHMARKS_TO_RUN" && [ "${ALLOW_REFERENCE_ON_DARWIN:-0}" != "1" ]; then
  echo "macOS detected: reference(OpenMP) will be skipped by default to avoid clang -fopenmp/-mcmodel errors."
fi
echo "Schedulers: $SCHEDULERS"
echo "Core configs: $CORE_CONFIGS"
echo "Runs per config: $NUM_RUNS"

if contains_item "cg" "$BENCHMARKS_TO_RUN" || contains_item "jac3d" "$BENCHMARKS_TO_RUN"; then
  resolve_argobots_flags
  configure_argobots_env
fi

if contains_item "cg" "$BENCHMARKS_TO_RUN"; then
  run_cg_argobots_fixed
fi
if contains_item "jac3d" "$BENCHMARKS_TO_RUN"; then
  run_jac3d_argobots_fixed
fi
if contains_item "reference" "$BENCHMARKS_TO_RUN"; then
  run_reference_non_argobots
fi

echo "Done. Summaries:"
find "$OUT_DIR" -maxdepth 1 -name "*.csv" -print