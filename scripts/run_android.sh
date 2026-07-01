#!/usr/bin/env bash
# scripts/run_android.sh
# Android device/simulator setup, build, deployment, and test runner script.

set -euo pipefail

find_active_device() {
    adb devices | grep -v "List" | grep "device" | head -n 1 | awk '{print $1}' || true
}

export_debug() {
    echo "==== Starting export-debug pipeline for Android ===="
    echo "Cleaning stale test binary artifacts..."
    rm -f build/tests/test_native.android.*
    
    echo "Building Android test binaries via SCons..."
    scons platform=android target=template_debug arch=arm64 build_tests=yes
    scons platform=android target=template_debug arch=x86_64 build_tests=yes
    echo "Android test binaries built successfully."
}

run_mobile_tests() {
    local device_id
    device_id=$(find_active_device)
    
    if [[ -z "$device_id" ]]; then
        echo "Error: No active Android simulator/emulator found." >&2
        exit 1
    fi
    echo "Using active Android device: $device_id"
    
    # Detect target architecture of the active device
    local abi
    abi=$(adb -s "$device_id" shell getprop ro.product.cpu.abi | tr -d '\r\n')
    local arch="arm64"
    if [[ "$abi" == *"x86_64"* ]]; then
        arch="x86_64"
    fi
    echo "Detected Emulator ABI: $abi (using arch: $arch)"
    
    local test_bin="build/tests/test_native.android.$arch"
    if [[ ! -f "$test_bin" ]]; then
        echo "Error: Native test binary not found: $test_bin" >&2
        exit 1
    fi
    
    echo "Pushing test binary to emulator..."
    adb -s "$device_id" push "$test_bin" /data/local/tmp/test_native
    
    echo "Setting executable permission..."
    adb -s "$device_id" shell chmod +x /data/local/tmp/test_native
    
    # Launch & Capture Logs
    echo "Starting tests on Android Emulator..."
    adb -s "$device_id" logcat -c
    
    local log_file="android_test_run.log"
    rm -f "$log_file"
    
    # Start background logcat
    adb -s "$device_id" logcat -v raw godot:I AndroidRuntime:E DEBUG:V *:S > "$log_file" &
    local logcat_pid=$!
    
    cleanup() {
        set +e
        if [[ -n "${logcat_pid:-}" ]]; then
            kill "$logcat_pid" 2>/dev/null || true
        fi
        if [[ -n "${device_id:-}" ]]; then
            adb -s "$device_id" shell rm -f /data/local/tmp/test_native 2>/dev/null || true
        fi
    }
    trap cleanup EXIT
    
    set +e
    local test_out_file="android_test_output.log"
    rm -f "$test_out_file"
    adb -s "$device_id" shell /data/local/tmp/test_native > "$test_out_file" 2>&1
    local exit_code=$?
    set -e
    
    if [[ -f "$test_out_file" ]]; then
        cat "$test_out_file"
    fi
    
    cleanup
    trap - EXIT
    
    local has_success=0
    if [[ $exit_code -eq 0 ]]; then
        if grep -q "SUCCESS" "$test_out_file" || grep -q "all test cases passed" "$test_out_file"; then
            has_success=1
        fi
    fi
    
    if [[ $has_success -eq 1 ]]; then
        echo "==== Android automated tests PASSED ===="
        exit 0
    else
        echo "==== Android automated tests FAILED (exit code: $exit_code) ===="
        if [[ -f "$log_file" ]]; then
            echo "==== Android logcat diagnostic dump ===="
            cat "$log_file"
        fi
        exit 1
    fi
}

# Command dispatching
if [[ "${1:-}" == "--export" ]]; then
    export_debug
elif [[ "${1:-}" == "--run" ]]; then
    run_mobile_tests
else
    echo "Usage: $0 [--export | --run]" >&2
    exit 1
fi
