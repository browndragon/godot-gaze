#!/usr/bin/env bash
# scripts/run_tests.sh
# Compiles and runs the C++ unit and integration tests.

set -eo pipefail

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BASE_DIR}/build/tests"

FORWARD_ARGS=()
for arg in "$@"; do
    FORWARD_ARGS+=("${arg}")
done

echo "=== Gaze Tracker C++ Test Runner ==="
mkdir -p "${BUILD_DIR}"

# Run Web GDExtension toolchain verification check
python3 "${BASE_DIR}/scripts/verify_web_toolchain.py"
echo ""

# 1. Detect if OpenCV SDK is available
OPENCV_SDK=""
for hb_path in "${BASE_DIR}/thirdparty/opencv-brew/macos" "/opt/homebrew/opt/opencv" "/usr/local/opt/opencv"; do
    if [ -d "${hb_path}" ]; then
        OPENCV_SDK="${hb_path}"
        break
    fi
done

if [ -n "${OPENCV_SDK}" ]; then
    echo "Using OpenCV SDK at: ${OPENCV_SDK}"
    echo "Compiling Core and Native integration tests..."
    
    g++ -std=c++17 \
        "${BASE_DIR}/tests/test_main.cpp" \
        "${BASE_DIR}/tests/test_native.cpp" \
        "${BASE_DIR}/src/core/projection_engine.cpp" \
        "${BASE_DIR}/src/native/yunet_pipeline.cpp" \
        "${BASE_DIR}/src/native/opencv_gaze_model.cpp" \
        -I"${BASE_DIR}/src/core" \
        -I"${BASE_DIR}/src/native" \
        -I"${BASE_DIR}/thirdparty/doctest" \
        -I"${BASE_DIR}/thirdparty/one_euro_filter" \
        -I"${OPENCV_SDK}/include" \
        -I"${OPENCV_SDK}/include/opencv4" \
        -L"${OPENCV_SDK}/lib" \
        -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_objdetect -lopencv_videoio -lopencv_dnn -lopencv_calib3d \
        -o "${BUILD_DIR}/run_tests"
        
    echo "Compilation successful. Executing C++ test suite..."
    cd "${BASE_DIR}"
    
    set +e
    "${BUILD_DIR}/run_tests" ${FORWARD_ARGS[@]+"${FORWARD_ARGS[@]}"}
    NATIVE_EXIT_CODE=$?
    set -e
    
    echo ""
    WEB_EXIT_CODE=0
    if command -v node >/dev/null 2>&1; then
        echo "Executing Web Sidecar integration tests..."
        set +e
        node "${BASE_DIR}/tools/run_sidecar_gaze_tests.js"
        WEB_EXIT_CODE=$?
        set -e
    else
        echo "[SKIP] Web Sidecar integration tests (node is not installed)"
    fi
    
    echo ""
    echo "=== Test Summary ==="
    if [ ${NATIVE_EXIT_CODE} -eq 0 ]; then
        echo "[PASS] Core tests"
        echo "[PASS] OpenCV dependency check"
        echo "[PASS] OpenCV native integration tests"
    else
        echo "[FAIL] C++ Native tests"
    fi
    
    if command -v node >/dev/null 2>&1; then
        if [ ${WEB_EXIT_CODE} -eq 0 ]; then
            echo "[PASS] Web Sidecar integration tests"
        else
            echo "[FAIL] Web Sidecar integration tests"
        fi
    else
        echo "[SKIP] Web Sidecar integration tests (missing node dependency)"
    fi
    
    if [ ${NATIVE_EXIT_CODE} -eq 0 ] && [ ${WEB_EXIT_CODE} -eq 0 ]; then
        exit 0
    else
        exit 1
    fi
else
    echo "OpenCV SDK not found. Compiling Core tests only..."
    
    g++ -std=c++17 \
        "${BASE_DIR}/tests/test_main.cpp" \
        "${BASE_DIR}/src/core/projection_engine.cpp" \
        -I"${BASE_DIR}/src/core" \
        -I"${BASE_DIR}/thirdparty/doctest" \
        -I"${BASE_DIR}/thirdparty/one_euro_filter" \
        -o "${BUILD_DIR}/run_tests"
        
    echo "Compilation successful. Executing Core tests..."
    cd "${BASE_DIR}"
    
    # Run core tests and capture exit code
    set +e
    "${BUILD_DIR}/run_tests" ${FORWARD_ARGS[@]+"${FORWARD_ARGS[@]}"}
    TEST_EXIT_CODE=$?
    set -e
    
    echo ""
    echo "=== Test Summary ==="
    if [ ${TEST_EXIT_CODE} -eq 0 ]; then
        echo "[PASS] Core tests"
    else
        echo "[FAIL] Core tests"
    fi
    
    echo "[FAIL] OpenCV dependency check: OpenCV SDK was not found!"
    echo "       -> Instructions to repair:"
    echo "          1. On macOS, run: brew install opencv"
    echo "          2. Run the symlink setup script: ./scripts/download_opencv.sh"
    echo "          3. Or set the OPENCV_DIR environment variable to your OpenCV SDK root."
    echo "[SKIP] OpenCV native integration tests (skipped due to missing OpenCV dependency)"
    echo ""
    
    # Exit with code 1 to indicate overall failure due to missing dependency
    exit 1
fi
