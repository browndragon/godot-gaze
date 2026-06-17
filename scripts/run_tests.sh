#!/usr/bin/env bash
# scripts/run_tests.sh
# Compiles and runs the standalone C++ core unit tests.

set -euo pipefail

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BASE_DIR}/build/tests"

echo "=== Gaze Tracker C++ Unit Test Runner ==="
mkdir -p "${BUILD_DIR}"

echo "Compiling tests..."
g++ -std=c++17 "${BASE_DIR}/tests/test_main.cpp" "${BASE_DIR}/src/core/projection_engine.cpp" \
    -I"${BASE_DIR}/src/core" \
    -I"${BASE_DIR}/thirdparty/doctest" \
    -I"${BASE_DIR}/thirdparty/one_euro_filter" \
    -o "${BUILD_DIR}/run_tests"

echo "Compilation successful. Compiled binary is at: build/tests/run_tests"
echo "Executing test suite..."
"${BUILD_DIR}/run_tests"
