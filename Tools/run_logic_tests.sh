#!/usr/bin/env bash
# Тесты чистого ядра баланса — без Unreal, чистый C++17.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/Tools/shim/build"
mkdir -p "$BUILD"
CXX="${CXX:-clang++}"
"$CXX" -std=c++17 -Wall -Wextra -O1 -o "$BUILD/logic_tests" "$ROOT/Tools/logic_tests.cpp"
"$BUILD/logic_tests"
