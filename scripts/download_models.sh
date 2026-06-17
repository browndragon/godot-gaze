#!/usr/bin/env bash
# scripts/download_models.sh
# Automates the download of required pre-trained weights (YuNet face detector and Intel ADAS Gaze model).

set -euo pipefail

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODELS_DIR="${BASE_DIR}/project/models"

echo "=== Gaze Tracker Pre-trained Models Setup ==="
mkdir -p "${MODELS_DIR}"

# 1. Download YuNet Face Detector Model (using stable Hugging Face mirror to avoid raw Git LFS pointer text or 404s)
YUNET_URL="https://huggingface.co/opencv/face_detection_yunet/resolve/main/face_detection_yunet_2023mar.onnx"
YUNET_FILE="${MODELS_DIR}/face_detection_yunet_2023mar.onnx"

# Remove corrupted or legacy 2024may HTML/ONNX file if present
rm -f "${MODELS_DIR}/face_detection_yunet_2024may.onnx"

if [ -f "${YUNET_FILE}" ]; then
    echo "YuNet model already exists at: ${YUNET_FILE}"
else
    echo "Downloading YuNet face detector model..."
    curl -L -o "${YUNET_FILE}" "${YUNET_URL}"
    echo "YuNet model downloaded successfully."
fi

# 2. Download Intel OpenVINO Gaze Estimation ADAS model (.xml and .bin)
# OpenCV's cv::dnn module natively parses OpenVINO IR model files directly!
XML_URL="https://storage.openvinotoolkit.org/repositories/open_model_zoo/2022.1/models_bin/1/gaze-estimation-adas-0002/FP16/gaze-estimation-adas-0002.xml"
BIN_URL="https://storage.openvinotoolkit.org/repositories/open_model_zoo/2022.1/models_bin/1/gaze-estimation-adas-0002/FP16/gaze-estimation-adas-0002.bin"

XML_FILE="${MODELS_DIR}/gaze-estimation-adas-0002.xml"
BIN_FILE="${MODELS_DIR}/gaze-estimation-adas-0002.bin"

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
