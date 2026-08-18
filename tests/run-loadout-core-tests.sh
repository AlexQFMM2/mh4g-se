#!/usr/bin/env bash
set -euo pipefail
test_root="$(cd "$(dirname "$0")" && pwd)"
build_root="$test_root/build-loadout-core"
mkdir -p "$build_root"
qmake_command="${QMAKE:-}"
if [[ -z "$qmake_command" ]]; then
    qmake_command="$(command -v qmake || command -v qmake-qt5)"
fi
"$qmake_command" "$test_root/loadout-core.pro" -o "$build_root/Makefile"
make_command="${MAKE:-}"
if [[ -z "$make_command" ]]; then
    make_command="$(command -v make || command -v mingw32-make)"
fi
"$make_command" -C "$build_root" -j2
"$build_root/test_loadout_core"
