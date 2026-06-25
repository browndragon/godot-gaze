#!/usr/bin/env bash
# scripts/godot.sh
# Cross-platform helper to locate and run Godot executable.

set -euo pipefail

# 1. Check if running on macOS
if [[ "$OSTYPE" == "darwin"* ]]; then
    MACOS_GODOT="/Applications/Godot.app/Contents/MacOS/Godot"
    if [[ -f "$MACOS_GODOT" ]]; then
        exec "$MACOS_GODOT" "$@"
    fi
fi

# 2. Try looking in PATH for godot or godot4
for cmd in godot godot4; do
    if command -v "$cmd" &> /dev/null; then
        exec "$cmd" "$@"
    fi
done

echo "Error: Could not locate Godot executable." >&2
echo "- On macOS, please ensure Godot is installed at: /Applications/Godot.app" >&2
echo "- On other platforms, ensure 'godot' or 'godot4' is in your PATH." >&2
exit 1
