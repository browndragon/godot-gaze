#!/usr/bin/env bash
# scripts/run_ios.sh
# iOS device/simulator setup, build, deployment, and test runner script.

set -euo pipefail

find_active_device() {
    xcrun simctl list devices | grep "Booted" | head -n 1 | sed -E 's/.* \(([-0-9A-Fa-f]+)\) \(Booted\)/\1/' || true
}

export_debug() {
    echo "==== Starting export-debug pipeline for iOS ===="
    echo "Cleaning stale test binary artifacts..."
    rm -f build/tests/test_native.ios.*
    
    echo "Building iOS test binaries via SCons..."
    local host_arch
    host_arch=$(uname -m)
    scons platform=ios target=template_debug ios_simulator=yes arch="$host_arch" build_tests=yes
    echo "iOS test binaries built successfully."
}

run_mobile_tests() {
    local device_id
    device_id=$(find_active_device)
    
    if [[ -z "$device_id" ]]; then
        echo "Error: No active iOS simulator/emulator found." >&2
        exit 1
    fi
    echo "Using active iOS device: $device_id"
    
    local host_arch
    host_arch=$(uname -m)
    local test_bin="build/tests/test_native.ios.simulator.$host_arch"
    
    if [[ ! -f "$test_bin" ]]; then
        echo "Error: Native test binary not found: $test_bin" >&2
        exit 1
    fi
    
    echo "Starting tests on iOS Simulator: $device_id..."
    local log_file="ios_test_run.log"
    rm -f "$log_file"
    
    set +e
    xcrun simctl spawn "$device_id" "$test_bin" > "$log_file" 2>&1
    local exit_code=$?
    set -e
    
    cat "$log_file"
    
    if [[ $exit_code -eq 0 ]] && (grep -q "SUCCESS" "$log_file" || grep -q "all test cases passed" "$log_file"); then
        echo "==== iOS automated tests PASSED ===="
        exit 0
    else
        echo "==== iOS automated tests FAILED (exit code: $exit_code) ===="
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
