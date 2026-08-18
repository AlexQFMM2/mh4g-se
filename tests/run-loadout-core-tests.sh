#!/usr/bin/env bash
set -euo pipefail
test_root="$(cd "$(dirname "$0")" && pwd)"
build_root="$test_root/build-loadout-core"
mkdir -p "$build_root"
qmake "$test_root/loadout-core.pro" -o "$build_root/Makefile"
make -C "$build_root" -j2
"$build_root/test_loadout_core"
