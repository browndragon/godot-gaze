#!/usr/bin/env bash
# scripts/godot.sh
# Cross-platform helper to locate and run Godot executable.

set -euo pipefail

# 1. Locate Godot executable
GODOT_BIN=""
if [[ "$OSTYPE" == "darwin"* ]]; then
    MACOS_GODOT="/Applications/Godot.app/Contents/MacOS/Godot"
    if [[ -f "$MACOS_GODOT" ]]; then
        GODOT_BIN="$MACOS_GODOT"
    fi
fi

if [[ -z "$GODOT_BIN" ]]; then
    for cmd in godot godot4; do
        if command -v "$cmd" &> /dev/null; then
            GODOT_BIN="$(command -v "$cmd")"
            break
        fi
    done
fi

if [[ -z "$GODOT_BIN" ]]; then
    echo "Error: Could not locate Godot executable." >&2
    echo "- On macOS, please ensure Godot is installed at: /Applications/Godot.app" >&2
    echo "- On other platforms, ensure 'godot' or 'godot4' is in your PATH." >&2
    exit 1
fi

# Default parameter values
AUTO=true
HEADLESS=""
TIMEOUT=""
GODOT_ARGS=()

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --auto)
            AUTO=true
            shift
            ;;
        --no-auto|--noauto)
            AUTO=false
            shift
            ;;
        --headless)
            HEADLESS=true
            shift
            ;;
        --no-headless|--noheadless)
            HEADLESS=false
            shift
            ;;
        --timeout)
            TIMEOUT="$2"
            shift 2
            ;;
        --no-timeout|--notimeout)
            TIMEOUT=0
            shift
            ;;
        *)
            GODOT_ARGS+=("$1")
            shift
            ;;
    esac
done

# Resolve parameter defaults based on AUTO if not explicitly overridden
if [[ -z "$HEADLESS" ]]; then
    if [[ "$AUTO" == "true" ]]; then
        HEADLESS=true
    else
        HEADLESS=false
    fi
fi

if [[ -z "$TIMEOUT" ]]; then
    if [[ "$AUTO" == "true" ]]; then
        TIMEOUT=20
    else
        TIMEOUT=0
    fi
fi

# Add --headless if required
if [[ "$HEADLESS" == "true" ]]; then
    HAS_HEADLESS=false
    for arg in ${GODOT_ARGS[@]+"${GODOT_ARGS[@]}"}; do
        if [[ "$arg" == "--headless" ]]; then
            HAS_HEADLESS=true
            break
        fi
    done
    if [[ "$HAS_HEADLESS" == "false" ]]; then
        GODOT_ARGS+=("--headless")
    fi
fi

# Always ensure path is set to project directory if not specified
HAS_PATH=false
for arg in ${GODOT_ARGS[@]+"${GODOT_ARGS[@]}"}; do
    if [[ "$arg" == "--path" ]]; then
        HAS_PATH=true
        break
    fi
done
if [[ "$HAS_PATH" == "false" ]]; then
    GODOT_ARGS+=("--path" "project")
fi

echo "Running Godot tests with: ${GODOT_ARGS[*]:-}"

# Run Godot with or without watchdog timeout
if [[ $TIMEOUT -gt 0 ]]; then
    "$GODOT_BIN" ${GODOT_ARGS[@]+"${GODOT_ARGS[@]}"} &
    GODOT_PID=$!
    
    (
        sleep "$TIMEOUT"
        if kill -0 "$GODOT_PID" 2>/dev/null; then
            echo "[WATCHDOG] Godot process timed out after $TIMEOUT seconds. Terminating..." >&2
            kill -9 "$GODOT_PID" 2>/dev/null || true
        fi
    ) &
    WATCHDOG_PID=$!
    
    set +e
    wait "$GODOT_PID"
    EXIT_CODE=$?
    set -e
    
    kill "$WATCHDOG_PID" 2>/dev/null || true
    exit "$EXIT_CODE"
else
    exec "$GODOT_BIN" ${GODOT_ARGS[@]+"${GODOT_ARGS[@]}"}
fi
