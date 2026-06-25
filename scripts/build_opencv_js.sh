#!/usr/bin/env bash
# build_opencv_js.sh
# Script to compile a custom opencv.js for WebAssembly containing FaceDetectorYN.

set -euo pipefail

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OPENCV_SRC="${BASE_DIR}/thirdparty/opencv"
EMSDK_DIR="/Users/acunningham/.asdf/installs/emsdk/4.0.20"

if [ ! -d "${OPENCV_SRC}" ]; then
    echo "Error: OpenCV source directory not found at ${OPENCV_SRC}."
    exit 1
fi

echo "=== Activating Emscripten SDK ==="
if [ -f "${EMSDK_DIR}/emsdk_env.sh" ]; then
    source "${EMSDK_DIR}/emsdk_env.sh"
else
    echo "Error: emsdk_env.sh not found at ${EMSDK_DIR}."
    exit 1
fi

# Determine if compiling with threads
WITH_THREADS=false
BUILD_DIR="${BASE_DIR}/build/opencv/web_nothreads"

for arg in "$@"; do
    if [ "${arg}" == "--threads" ]; then
        WITH_THREADS=true
        BUILD_DIR="${BASE_DIR}/build/opencv/web_threads"
    fi
done

echo "=== Building OpenCV.js ==="
echo "Build Dir: ${BUILD_DIR}"
echo "Threads: ${WITH_THREADS}"

cd "${OPENCV_SRC}"

# Run OpenCV build script
# - We enable WebAssembly (--build_wasm)
# - We enable SIMD optimization (--simd)
# - We enable exceptions (--enable_exception) so C++ errors are thrown to JS
# - We enforce C++17 standard for Embind compatibility with Emscripten 4.0.20+
# - We pass --threads if requested
BUILD_CMD=(
    python3 platforms/js/build_js.py
    "${BUILD_DIR}"
    --build_wasm
    --simd
    --enable_exception
    --cmake_option="-DCMAKE_CXX_STANDARD=17"
)

if [ "${WITH_THREADS}" == "true" ]; then
    BUILD_CMD+=(--threads)
fi

# Run build
"${BUILD_CMD[@]}"

echo "=== Successfully built Custom OpenCV.js in: ${BUILD_DIR} ==="

