#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

clang-format --dry-run --Werror \
    "$repo_root"/cpp/include/llab/*.hpp \
    "$repo_root"/cpp/src/*.cpp \
    "$repo_root"/cpp/benchmarks/*.cpp \
    "$repo_root"/cpp/tests/*.cpp

(cd "$repo_root/rust" && cargo fmt --all -- --check)
