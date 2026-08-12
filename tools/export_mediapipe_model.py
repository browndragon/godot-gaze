#!/usr/bin/env python3
"""
Tool script to download Google's official MediaPipe Face Landmarker model,
extract face_landmarks_detector.tflite, re-encode it to ONNX via tf2onnx,
and convert it to ONNX Runtime FlatBuffer format (.ort).
"""

import os
import sys
import shutil
import urllib.request
import zipfile
import subprocess

GOOGLE_FACE_LANDMARKER_URL = (
    "https://storage.googleapis.com/mediapipe-models/face_landmarker/"
    "face_landmarker/float16/1/face_landmarker.task"
)

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
MODELS_DIR = os.path.join(PROJECT_ROOT, "project", "addons", "godot-gaze", "models")
TASK_PATH = os.path.join(MODELS_DIR, "face_landmarker.task")
MESH_TFLITE_PATH = os.path.join(MODELS_DIR, "face_landmarks_detector.tflite")
MESH_ONNX_PATH = os.path.join(MODELS_DIR, "mediapipe_face_mesh.onnx")
MESH_ORT_PATH = os.path.join(MODELS_DIR, "mediapipe_face_mesh.ort")

DETECTOR_TFLITE_PATH = os.path.join(MODELS_DIR, "face_detector.tflite")
DETECTOR_ONNX_PATH = os.path.join(MODELS_DIR, "mediapipe_face_detector.onnx")
DETECTOR_ORT_PATH = os.path.join(MODELS_DIR, "mediapipe_face_detector.ort")

def get_python_exec():
    # Test current sys.executable
    try:
        res = subprocess.run([sys.executable, "-c", "import tensorflow"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if res.returncode == 0:
            return sys.executable
    except Exception:
        pass

    # Try python3.11 if available (common on modern macOS with Python 3.14 system default)
    sh_py11 = shutil.which("python3.11")
    if sh_py11:
        try:
            res = subprocess.run([sh_py11, "-c", "import tensorflow"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if res.returncode == 0:
                return sh_py11
        except Exception:
            pass

    return sys.executable

def export_single_model(py_exec, tflite_name, onnx_name, ort_name):
    tflite_p = os.path.join(MODELS_DIR, tflite_name)
    onnx_p = os.path.join(MODELS_DIR, onnx_name)
    ort_p = os.path.join(MODELS_DIR, ort_name)

    print(f"[Tool] Converting {tflite_name} to {onnx_name} via tf2onnx using {py_exec}...")
    cmd_tf2onnx = [
        py_exec,
        "-m", "tf2onnx.convert",
        "--tflite", tflite_p,
        "--output", onnx_p
    ]
    subprocess.run(cmd_tf2onnx, check=True)

    print(f"[Tool] Converting {onnx_name} to {ort_name} via onnxruntime tools...")
    cmd_ort = [
        sys.executable,
        "-m", "onnxruntime.tools.convert_onnx_models_to_ort",
        "--optimization_style=Runtime",
        onnx_p
    ]
    subprocess.run(cmd_ort, check=True)

    base_name = os.path.splitext(onnx_name)[0]
    generated_ort = os.path.join(MODELS_DIR, f"{base_name}.with_runtime_opt.ort")
    if os.path.exists(generated_ort):
        shutil.move(generated_ort, ort_p)

    print(f"[Tool] Export of {ort_name} successfully completed!")

def export_model():
    py_exec = get_python_exec()
    os.makedirs(MODELS_DIR, exist_ok=True)

    print(f"[Tool] Downloading official MediaPipe task bundle from {GOOGLE_FACE_LANDMARKER_URL}...")
    urllib.request.urlretrieve(GOOGLE_FACE_LANDMARKER_URL, TASK_PATH)

    print("[Tool] Extracting models from task bundle...")
    with zipfile.ZipFile(TASK_PATH, "r") as z:
        z.extract("face_landmarks_detector.tflite", path=MODELS_DIR)
        z.extract("face_detector.tflite", path=MODELS_DIR)

    export_single_model(py_exec, "face_landmarks_detector.tflite", "mediapipe_face_mesh.onnx", "mediapipe_face_mesh.ort")
    export_single_model(py_exec, "face_detector.tflite", "mediapipe_face_detector.onnx", "mediapipe_face_detector.ort")

    print("[Tool] All MediaPipe model exports successfully completed!")

if __name__ == "__main__":
    export_model()
