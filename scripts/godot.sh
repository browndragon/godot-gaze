#!/usr/bin/env bash
# scripts/godot.sh
# Cross-platform helper to locate and run Godot executable.

set -euo pipefail

# Process arguments: default to --headless unless --noheadless is specified
HEADLESS=true
GODOT_ARGS=()
for arg in "$@"; do
    if [[ "$arg" == "--noheadless" ]]; then
        HEADLESS=false
    else
        GODOT_ARGS+=("$arg")
    fi
done

if [[ "$HEADLESS" == "true" ]]; then
    # Add --headless if not already present
    HAS_HEADLESS=false
    if [[ "${#GODOT_ARGS[@]}" -gt 0 ]]; then
        for arg in "${GODOT_ARGS[@]}"; do
            if [[ "$arg" == "--headless" ]]; then
                HAS_HEADLESS=true
                break
            fi
        done
    fi
    if [[ "$HAS_HEADLESS" == "false" ]]; then
        GODOT_ARGS+=("--headless")
    fi
fi

# 1. Check if running on macOS
if [[ "$OSTYPE" == "darwin"* ]]; then
    MACOS_GODOT="/Applications/Godot.app/Contents/MacOS/Godot"
    if [[ -f "$MACOS_GODOT" ]]; then
        if [[ "${#GODOT_ARGS[@]}" -gt 0 ]]; then
            exec "$MACOS_GODOT" "${GODOT_ARGS[@]}"
        else
            exec "$MACOS_GODOT"
        fi
    fi
fi

# 2. Try looking in PATH for godot or godot4
for cmd in godot godot4; do
    if command -v "$cmd" &> /dev/null; then
        if [[ "${#GODOT_ARGS[@]}" -gt 0 ]]; then
            exec "$cmd" "${GODOT_ARGS[@]}"
        else
            exec "$cmd"
        fi
    fi
done

echo "Error: Could not locate Godot executable." >&2
echo "- On macOS, please ensure Godot is installed at: /Applications/Godot.app" >&2
echo "- On other platforms, ensure 'godot' or 'godot4' is in your PATH." >&2
exit 1
