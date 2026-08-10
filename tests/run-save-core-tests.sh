#!/usr/bin/env bash
set -euo pipefail

test_dir="$(cd "$(dirname "$0")" && pwd)"
build_dir="$test_dir/build"
start_dir="$(pwd)"
mkdir -p "$build_dir"
cd "$build_dir"
qmake ../save-core.pro
make -j"$(nproc)"
cd "$start_dir"
exec "$build_dir/test_save_core" "$@"
