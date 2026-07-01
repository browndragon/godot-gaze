#!/usr/bin/env bash
# scripts/generate_docs.sh
# Checks for doxygen installation and compiles the API documentation.

set -euo pipefail

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOXYFILE="${BASE_DIR}/Doxyfile"
OUTPUT_DIR="${BASE_DIR}/docs/doxygen"

echo "=== Godot Gaze Documentation Generator ==="

# Check if doxygen is available in path
if ! command -v doxygen >/dev/null 2>&1; then
    echo ""
    echo "ERROR: Doxygen was not found in your PATH."
    echo "To install Doxygen on macOS using Homebrew, run:"
    echo "    brew install doxygen"
    echo "To install on Linux:"
    echo "    sudo apt-get install doxygen"
    echo ""
    exit 1
fi

echo "Doxygen found: $(doxygen --version)"
echo "Cleaning old documentation..."
rm -rf "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"

echo "Generating documentation website..."
cd "${BASE_DIR}"
doxygen Doxyfile

echo "Translating Doxygen XML to Godot classref XML..."
python3 "${BASE_DIR}/scripts/doxygen_to_godot_doc.py"

echo ""
echo "=== Documentation successfully compiled! ==="
echo "You can open the documentation main page by running:"
echo "    open docs/doxygen/html/index.html"
echo ""
