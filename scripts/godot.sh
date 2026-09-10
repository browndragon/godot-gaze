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
TEST_MODE=true
EXPLICIT_S=""
if [[ -f "project/project.godot" ]]; then
    PROJECT_PATH="project"
elif [[ -f "project.godot" ]]; then
    PROJECT_PATH="."
else
    PROJECT_PATH="."
fi
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
        -s)
            EXPLICIT_S="$2"
            shift 2
            ;;
        --notest|--no-test)
            TEST_MODE=false
            shift
            ;;
        --test)
            TEST_MODE=true
            shift
            ;;
        --path)
            PROJECT_PATH="$2"
            shift 2
            ;;
        *)
            ARG="$1"
            if [[ -d "$PROJECT_PATH" ]]; then
                ABS_PROJ="$(cd "$PROJECT_PATH" && pwd)"
                if [[ "$ARG" == "$ABS_PROJ/"* ]]; then
                    ARG="res://${ARG#$ABS_PROJ/}"
                fi
            fi
            if [[ "$ARG" == "project/"* && ("$PROJECT_PATH" == "project" || "$PROJECT_PATH" == *"/project") ]]; then
                ARG="res://${ARG#project/}"
            elif [[ "$ARG" == "addons/"* ]]; then
                ARG="res://${ARG}"
            elif [[ "$ARG" == *".tscn" || "$ARG" == *".scn" ]]; then
                ARG="res://${ARG}"
            fi
            if [[ "$ARG" == *".tscn" || "$ARG" == *".scn" ]]; then
                TEST_MODE=false
            fi
            GODOT_ARGS+=("$ARG")
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
        TIMEOUT=45
    else
        TIMEOUT=0
    fi
fi

FINAL_ARGS=()

# 1. Project path
if [[ -d "$PROJECT_PATH" ]]; then
    PROJECT_PATH="$(cd "$PROJECT_PATH" && pwd)"
fi
FINAL_ARGS+=("--path" "$PROJECT_PATH")

# 2. Headless mode
if [[ "$HEADLESS" == "true" ]]; then
    FINAL_ARGS+=("--headless")
fi

# 3. Script flag precedence (-s arg, --notest, or default to GUT)
if [[ -n "$EXPLICIT_S" ]]; then
    FINAL_ARGS+=("-s" "$EXPLICIT_S")
elif [[ "$TEST_MODE" == "true" ]]; then
    FINAL_ARGS+=("-s" "res://addons/gut/gut_cmdln.gd")
fi

# 4. Remaining Godot arguments (e.g. -gtest=..., -gdir=..., or scene path)
FINAL_ARGS+=(${GODOT_ARGS[@]+"${GODOT_ARGS[@]}"})

echo "Running Godot with: ${FINAL_ARGS[*]:-}"

# Run Godot with or without watchdog timeout
if [[ $TIMEOUT -gt 0 ]]; then
    "$GODOT_BIN" ${FINAL_ARGS[@]+"${FINAL_ARGS[@]}"} &
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
    
    pkill -P "$WATCHDOG_PID" 2>/dev/null || true
    kill "$WATCHDOG_PID" 2>/dev/null || true
    exit "$EXIT_CODE"
else
    exec "$GODOT_BIN" ${FINAL_ARGS[@]+"${FINAL_ARGS[@]}"}
fi
