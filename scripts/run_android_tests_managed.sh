#!/usr/bin/env bash
# scripts/run_android_tests_managed.sh
# Automated Android test script with managed emulator lifecycle.

set -euo pipefail

AVD_NAME="${AVD_NAME:-Medium_Phone_API_36}"

# Find active device
active_device=$(adb devices | grep -v "List" | grep "device" | head -n 1 | awk '{print $1}' || true)

emulator_pid=""
if [[ -z "$active_device" ]]; then
    emulator_flags="-no-audio -gpu host"
    if [[ "${HEADLESS:-}" == "true" ]]; then
        echo "Running emulator in HEADLESS mode..."
        emulator_flags="$emulator_flags -no-window"
    fi
    echo "No running emulator found. Launching AVD: $AVD_NAME in the background..."
    emulator -avd "$AVD_NAME" $emulator_flags &
    emulator_pid=$!
    
    echo "Waiting for device to connect via adb..."
    adb wait-for-device
    
    echo "Waiting for Android system boot..."
    while [ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" != "1" ]; do
        sleep 1
    done
    echo "Emulator booted and online."
else
    echo "Using existing active device/emulator: $active_device"
fi

cleanup() {
    set +e
    if [[ -n "$emulator_pid" ]]; then
        echo "Shutting down managed emulator (PID: $emulator_pid)..."
        adb emu kill || kill "$emulator_pid" || true
        # Wait for the process to exit
        wait "$emulator_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT

echo "Executing Android E2E tests..."
./scripts/run_android_tests.sh
