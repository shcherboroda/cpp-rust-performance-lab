#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

# Runs alternating C++/Rust process rounds. Raw output stays local and ignored by Git.
repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
rounds=${LLAB_ROUNDS:-15}
cpu=${LLAB_CPU:-}
benchmark=${LLAB_BENCHMARK:-order_book_lifecycle}
stamp=$(date -u +%Y%m%dT%H%M%SZ)
output_dir="$repo_root/results/local/$benchmark-$stamp"
cpp_build_dir="$repo_root/build/cpp-release"

if [[ ! "$rounds" =~ ^[1-9][0-9]*$ ]]; then
  echo "LLAB_ROUNDS must be a positive integer" >&2
  exit 2
fi
case "$benchmark" in
  order_book_lifecycle|parity_order_book_lifecycle|dense_ladder_order_book_lifecycle|dense_ladder_best_level_churn|bitmap_ladder_best_level_churn|native_mixed_lifecycle|parity_mixed_lifecycle|dense_mixed_lifecycle|bitmap_mixed_lifecycle|bitmap_backshift_mixed_lifecycle|bitmap_packed_mixed_lifecycle|native_mixed_lifecycle_with_bbo|parity_mixed_lifecycle_with_bbo|dense_mixed_lifecycle_with_bbo|bitmap_mixed_lifecycle_with_bbo|bitmap_backshift_mixed_lifecycle_with_bbo|bitmap_packed_mixed_lifecycle_with_bbo|tombstone_order_index_churn|backshift_order_index_churn|identity_tombstone_order_index_churn|multiplicative_tombstone_order_index_churn|feed_sequence_recovery) ;;
  *) echo "LLAB_BENCHMARK must name a supported benchmark executable" >&2; exit 2 ;;
esac
if [[ -n "$cpu" ]] && ! command -v taskset >/dev/null; then
  echo "LLAB_CPU is set but taskset is unavailable" >&2
  exit 2
fi

mkdir -p "$output_dir"
{
  echo "timestamp_utc=$stamp"
  echo "rounds=$rounds"
  echo "benchmark=$benchmark"
  echo "LLAB_CPU=${cpu:-unset}"
  uname -a
  command -v lscpu >/dev/null && lscpu || true
  c++ --version | head -n 1
  rustc --version
  cargo --version
} >"$output_dir/environment.txt"

cmake -S "$repo_root/cpp" -B "$cpp_build_dir" -DCMAKE_BUILD_TYPE=Release
cmake --build "$cpp_build_dir" --parallel
(cd "$repo_root/rust" && cargo build --release --bin "$benchmark")
{
  echo "CMAKE_BUILD_TYPE=Release"
  grep -E '^CMAKE_CXX_FLAGS(_RELEASE)?:' "$cpp_build_dir/CMakeCache.txt" || true
  echo "CARGO_ENCODED_RUSTFLAGS=${CARGO_ENCODED_RUSTFLAGS:-unset}"
  echo "RUSTFLAGS=${RUSTFLAGS:-unset}"
} >"$output_dir/build-flags.txt"

run_one() {
  local language=$1
  local round=$2
  local raw="$output_dir/${round}-${language}-samples.csv"
  local summary="$output_dir/${round}-${language}-summary.txt"
  local -a command
  if [[ "$language" == cpp ]]; then
    command=("$cpp_build_dir/$benchmark" --raw "$raw")
  else
    command=("$repo_root/rust/target/release/$benchmark" --raw "$raw")
  fi
  if [[ -n "$cpu" ]]; then
    taskset -c "$cpu" "${command[@]}" >"$summary"
  else
    "${command[@]}" >"$summary"
  fi
}

echo "Results will be written to: $output_dir"
echo "This is a WSL2 desktop baseline. For controlled measurements, follow specs/measurement_protocol.md before continuing."
for ((round = 1; round <= rounds; ++round)); do
  if (( round % 2 == 1 )); then
    run_one cpp "$round"
    run_one rust "$round"
  else
    run_one rust "$round"
    run_one cpp "$round"
  fi
done

echo "Completed $rounds alternating process rounds per language. Preserve this directory with its raw CSV files."
