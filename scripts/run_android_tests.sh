#!/usr/bin/env bash
# scripts/run_android_tests.sh
# Android E2E Godot test execution script.

set -euo pipefail

find_active_device() {
    adb devices | grep -v "List" | grep "device" | head -n 1 | awk '{print $1}' || true
}

device_id=$(find_active_device)
if [[ -z "$device_id" ]]; then
    echo "Error: No active Android device/emulator found via adb." >&2
    exit 1
fi
echo "Using active Android device: $device_id"

apk_path="exports/android/godot-gaze.apk"
if [[ ! -f "$apk_path" ]]; then
    echo "Error: Android debug APK not found at $apk_path. Please run export first." >&2
    exit 1
fi

echo "Installing APK to emulator..."
adb -s "$device_id" install -r "$apk_path"

echo "Pre-granting camera permission..."
adb -s "$device_id" shell pm grant org.godotengine.godotgaze android.permission.CAMERA || true

echo "Clearing logcat..."
adb -s "$device_id" logcat -c

# Launch the Godot app E2E tests
echo "Launching Godot App E2E tests..."
adb -s "$device_id" shell am start -n org.godotengine.godotgaze/com.godot.game.GodotApp --esa cmdline "run-tests"

log_file="build/tests/logs/android_logcat_raw.log"
rm -f "$log_file"

# Stream logcat in background
adb -s "$device_id" logcat godot:I AndroidRuntime:E DEBUG:V *:S > "$log_file" &
logcat_pid=$!

cleanup() {
    set +e
    if [[ -n "${logcat_pid:-}" ]]; then
        kill "$logcat_pid" 2>/dev/null || true
    fi
    # Uninstall to keep emulator clean
    adb -s "$device_id" shell pm uninstall org.godotengine.godotgaze >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo "Monitoring logs for test completion..."
timeout=30
elapsed=0
test_passed=0
test_completed=0

while [[ $elapsed -lt $timeout ]]; do
    if [[ -f "$log_file" ]]; then
        if grep -q "ALL Headless Integration & E2E tests have passed successfully\!" "$log_file" || \
           grep -q "ALL Windowed GPU integration tests have passed successfully\!" "$log_file" || \
           grep -q "ALL Headless Integration & E2E tests have passed successfully" "$log_file"; then
            test_passed=1
            test_completed=1
            break
        elif grep -q "FAIL:" "$log_file" || grep -q "FATAL:" "$log_file"; then
            test_passed=0
            test_completed=1
            break
        fi
    fi
    sleep 1
    elapsed=$((elapsed + 1))
done

# Mirror captured logs to stdout
if [[ -f "$log_file" ]]; then
    echo "=== ANDROID LOGS ==="
    cat "$log_file"
fi

if [[ $test_passed -eq 1 ]]; then
    echo "==== Android automated E2E tests PASSED ===="
    exit 0
else
    if [[ $test_completed -eq 0 ]]; then
        echo "Error: Timeout reached waiting for Android tests to complete." >&2
    else
        echo "Error: Android automated E2E tests FAILED." >&2
    fi
    exit 1
fi
