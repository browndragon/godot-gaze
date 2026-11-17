#!/usr/bin/env bash
# scripts/download_models.sh
# Automates downloading and building required pre-trained weights.

set -euo pipefail

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${BASE_DIR}"

MODELS_DIR="${BASE_DIR}/project/addons/godot-gaze/models"
OPENVINO_DIR="${BASE_DIR}/test_assets/models"

while [ $# -gt 0 ]; do
    case "$1" in
        --output-dir)
            if [ $# -lt 2 ] || [ -z "$2" ]; then
                echo "ERROR: --output-dir requires a non-empty path argument." >&2
                exit 1
            fi
            MODELS_DIR="$2"
            shift 2
            ;;
        *)
            echo "ERROR: Unknown argument '$1'. Usage: $0 [--output-dir <path>]" >&2
            exit 1
            ;;
    esac
done

echo "=== Gaze Tracker Pre-trained Models Setup ==="
mkdir -p "${MODELS_DIR}" "${OPENVINO_DIR}"

PYTHON_BIN="$(command -v python3 || true)"
if [ -z "${PYTHON_BIN}" ]; then
    echo "ERROR: 'python3' was not found in PATH." >&2
    exit 1
fi

if ! python3 -c "import numpy, onnx, onnxruntime" >/dev/null 2>&1; then
    echo "ERROR: Python interpreter '${PYTHON_BIN}' is missing required packages (numpy, onnx, onnxruntime)." >&2
    echo "Install them before running this script, e.g.: python3 -m pip install numpy openvino2onnx onnx onnxruntime==1.28.0" >&2
    exit 1
fi

# 1. YuNet Face Detector Model (.ort)
YUNET_DST="${MODELS_DIR}/face_detection_yunet_2023mar.ort"
YUNET_REF="${OPENVINO_DIR}/face_detection_yunet_2023mar.ort"
if [ ! -f "${YUNET_DST}" ]; then
    if [ -f "${YUNET_REF}" ]; then
        echo "Copying canonical YuNet model from ${YUNET_REF}..."
        cp -f "${YUNET_REF}" "${YUNET_DST}"
    else
        echo "Downloading YuNet face detector ONNX model..."
        YUNET_ONNX="${MODELS_DIR}/face_detection_yunet_2023mar.onnx"
        curl -fsSL -o "${YUNET_ONNX}" "https://huggingface.co/opencv/face_detection_yunet/resolve/main/face_detection_yunet_2023mar.onnx"
        python3 -m onnxruntime.tools.convert_onnx_models_to_ort --optimization_style=Fixed "${YUNET_ONNX}" --output_dir "${MODELS_DIR}"
        rm -f "${MODELS_DIR}/face_detection_yunet_2023mar.required_operators.config"
    fi
fi
YUNET_SIZE=$(wc -c < "${YUNET_DST}" | tr -d '[:space:]')
if [ "${YUNET_SIZE}" -le 100000 ]; then
    echo "ERROR: Invalid YuNet ORT model at ${YUNET_DST} (${YUNET_SIZE} bytes)." >&2
    exit 1
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
        curl -fsSL -o "${xml_dst}" "${xml_url}"
        curl -fsSL -o "${bin_dst}" "${bin_url}"
        echo "Successfully downloaded ${model_name}."
    fi
done

# 3. Eye Openness Model (open_closed_eye.onnx from OMZ, cached in test_assets/models)
OPEN_CLOSED_EYE_URL="https://storage.openvinotoolkit.org/repositories/open_model_zoo/public/2022.1/open-closed-eye-0001/open_closed_eye.onnx"
OPEN_CLOSED_EYE_REF="${OPENVINO_DIR}/open_closed_eye.onnx"
OPEN_CLOSED_EYE_DST="${MODELS_DIR}/open_closed_eye.onnx"
if [ ! -f "${OPEN_CLOSED_EYE_REF}" ]; then
    echo "Downloading open_closed_eye.onnx..."
    curl -fsSL -H "User-Agent: Mozilla/5.0" -o "${OPEN_CLOSED_EYE_REF}" "${OPEN_CLOSED_EYE_URL}"
    echo "Successfully downloaded open_closed_eye.onnx."
fi
if [ ! -f "${MODELS_DIR}/open_closed_eye.ort" ] && [ ! -f "${OPEN_CLOSED_EYE_DST}" ]; then
    cp -f "${OPEN_CLOSED_EYE_REF}" "${OPEN_CLOSED_EYE_DST}"
fi

# 4. Convert OpenVINO IR (.xml/.bin) and ONNX models to ORT
echo "Converting OpenVINO and ONNX models to ORT..."
python3 "${BASE_DIR}/scripts/convert_model.py" --output-dir "${MODELS_DIR}"

if [ ! -f "${MODELS_DIR}/required_operators.config" ] && [ -f "${BASE_DIR}/scripts/required_operators.config" ]; then
    cp -f "${BASE_DIR}/scripts/required_operators.config" "${MODELS_DIR}/required_operators.config"
fi

# 5. Verify all required runtime model artifacts exist and are non-empty
REQUIRED_FILES=(
    "${MODELS_DIR}/face_detection_yunet_2023mar.ort"
    "${MODELS_DIR}/facial-landmarks-35-adas-0002.ort"
    "${MODELS_DIR}/gaze-estimation-adas-0002.ort"
    "${MODELS_DIR}/open_closed_eye.ort"
    "${MODELS_DIR}/required_operators.config"
)
for req_file in "${REQUIRED_FILES[@]}"; do
    if [ ! -s "${req_file}" ]; then
        echo "ERROR: Missing or empty required model asset: ${req_file}" >&2
        exit 1
    fi
done

echo "=== Models Setup Complete! ==="
ls -la "${MODELS_DIR}"
