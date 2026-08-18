#!/usr/bin/env bash
# scripts/download_models.sh
# Automates downloading and building required pre-trained weights.

set -euo pipefail

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODELS_DIR="${BASE_DIR}/project/addons/godot-gaze/models"
OPENVINO_DIR="${BASE_DIR}/test_assets/models"

echo "=== Gaze Tracker Pre-trained Models Setup ==="
mkdir -p "${MODELS_DIR}" "${OPENVINO_DIR}"

# 1. Export MediaPipe Face Mesh model
if [ -f "${BASE_DIR}/tools/export_mediapipe_model.py" ]; then
    echo "Exporting MediaPipe Face Mesh model..."
    python3 "${BASE_DIR}/tools/export_mediapipe_model.py"
fi

# 2. Intel OpenVINO ADAS Models (Declarative Registry)
# Format: <model_name> <omz_release_year> <precision>
OPENVINO_MODELS=(
    "gaze-estimation-adas-0002 2022.1 FP16"
    "facial-landmarks-35-adas-0002 2023.0 FP32"
)

for entry in "${OPENVINO_MODELS[@]}"; do
    read -r model_name release_year precision <<< "${entry}"

    xml_url="https://storage.openvinotoolkit.org/repositories/open_model_zoo/${release_year}/models_bin/1/${model_name}/${precision}/${model_name}.xml"
    bin_url="https://storage.openvinotoolkit.org/repositories/open_model_zoo/${release_year}/models_bin/1/${model_name}/${precision}/${model_name}.bin"

    xml_dst="${OPENVINO_DIR}/${model_name}.xml"
    bin_dst="${OPENVINO_DIR}/${model_name}.bin"

    if [ -f "${xml_dst}" ] && [ -f "${bin_dst}" ]; then
        echo "OpenVINO model ${model_name} already exists."
    else
        echo "Downloading OpenVINO model: ${model_name} (${release_year}/${precision})..."
        curl -sSL -o "${xml_dst}" "${xml_url}"
        curl -sSL -o "${bin_dst}" "${bin_url}"
        echo "Successfully downloaded ${model_name}."
    fi
done

# 3. Convert OpenVINO IR (.xml/.bin) to ONNX
echo "Converting OpenVINO models to ONNX..."
python3 "${BASE_DIR}/scripts/convert_model.py"

echo "=== Models Setup Complete! ==="
ls -la "${MODELS_DIR}"
