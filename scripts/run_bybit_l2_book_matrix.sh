#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
out="$root/results/local/bybit-l2-book-$(date -u +%Y%m%dT%H%M%SZ)"
mkdir -p "$out"
cmake -S "$root/cpp" -B "$root/build/bybit-l2" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$root/build/bybit-l2" --target bybit_l2_book_benchmark --parallel >/dev/null
(cd "$root/rust" && cargo build --release --bin bybit_l2_book_benchmark >/dev/null)
for workload in replace churn wide; do
  for variant in vector dense map; do
    for round in {1..5}; do
      if (( round % 2 )); then
        taskset -c "${LLAB_CPU:-0}" "$root/build/bybit-l2/bybit_l2_book_benchmark" "$variant" "$workload" >>"$out/cpp.txt"
        taskset -c "${LLAB_CPU:-0}" "$root/rust/target/release/bybit_l2_book_benchmark" "$variant" "$workload" >>"$out/rust.txt"
      else
        taskset -c "${LLAB_CPU:-0}" "$root/rust/target/release/bybit_l2_book_benchmark" "$variant" "$workload" >>"$out/rust.txt"
        taskset -c "${LLAB_CPU:-0}" "$root/build/bybit-l2/bybit_l2_book_benchmark" "$variant" "$workload" >>"$out/cpp.txt"
      fi
    done
  done
done
printf 'results=%s\n' "$out"
