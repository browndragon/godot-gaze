#!/usr/bin/env bash
# scripts/download_models.sh
# Automates downloading and building required pre-trained weights (MediaPipe Face Mesh and Intel ADAS Gaze model).

set -euo pipefail

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODELS_DIR="${BASE_DIR}/project/addons/godot-gaze/models"

echo "=== Gaze Tracker Pre-trained Models Setup ==="
mkdir -p "${MODELS_DIR}"

# 1. Download and convert MediaPipe Face Mesh model via export_mediapipe_model.py
echo "Exporting MediaPipe Face Mesh model..."
python3 "${BASE_DIR}/tools/export_mediapipe_model.py"

# 2. Download Intel OpenVINO Gaze Estimation ADAS model (.xml and .bin)
XML_URL="https://storage.openvinotoolkit.org/repositories/open_model_zoo/2022.1/models_bin/1/gaze-estimation-adas-0002/FP16/gaze-estimation-adas-0002.xml"
BIN_URL="https://storage.openvinotoolkit.org/repositories/open_model_zoo/2022.1/models_bin/1/gaze-estimation-adas-0002/FP16/gaze-estimation-adas-0002.bin"

OPENVINO_DIR="${BASE_DIR}/test_assets/models"
mkdir -p "${OPENVINO_DIR}"
XML_FILE="${OPENVINO_DIR}/gaze-estimation-adas-0002.xml"
BIN_FILE="${OPENVINO_DIR}/gaze-estimation-adas-0002.bin"

if [ -f "${XML_FILE}" ] && [ -f "${BIN_FILE}" ]; then
    echo "Intel ADAS Gaze model already exists at: ${XML_FILE}"
else
    echo "Downloading Intel ADAS Gaze Estimation model (.xml & .bin)..."
    curl -L -o "${XML_FILE}" "${XML_URL}"
    curl -L -o "${BIN_FILE}" "${BIN_URL}"
    echo "Intel ADAS Gaze Estimation model downloaded successfully."
fi

echo "=== Models Setup Complete! ==="
echo "Models directory: ${MODELS_DIR}"
ls -la "${MODELS_DIR}"
