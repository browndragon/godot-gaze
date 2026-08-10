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
TFLITE_PATH = os.path.join(MODELS_DIR, "face_landmarks_detector.tflite")
ONNX_PATH = os.path.join(MODELS_DIR, "mediapipe_face_mesh.onnx")
ORT_PATH = os.path.join(MODELS_DIR, "mediapipe_face_mesh.ort")

def export_model():
    os.makedirs(MODELS_DIR, exist_ok=True)

    print(f"[Tool] Downloading official MediaPipe task bundle from {GOOGLE_FACE_LANDMARKER_URL}...")
    urllib.request.urlretrieve(GOOGLE_FACE_LANDMARKER_URL, TASK_PATH)

    print("[Tool] Extracting face_landmarks_detector.tflite from task bundle...")
    with zipfile.ZipFile(TASK_PATH, "r") as z:
        z.extract("face_landmarks_detector.tflite", path=MODELS_DIR)

    print("[Tool] Converting TFLite model to ONNX format via tf2onnx...")
    cmd_tf2onnx = [
        sys.executable,
        "-m", "tf2onnx.convert",
        "--tflite", TFLITE_PATH,
        "--output", ONNX_PATH
    ]
    subprocess.run(cmd_tf2onnx, check=True)

    print("[Tool] Converting ONNX model to ORT FlatBuffer format via onnxruntime tools...")
    cmd_ort = [
        sys.executable,
        "-m", "onnxruntime.tools.convert_onnx_models_to_ort",
        "--optimization_style=Runtime",
        ONNX_PATH
    ]
    subprocess.run(cmd_ort, check=True)

    generated_ort = os.path.join(MODELS_DIR, "mediapipe_face_mesh.with_runtime_opt.ort")
    if os.path.exists(generated_ort):
        shutil.move(generated_ort, ORT_PATH)

    print(f"[Tool] Model export successfully completed! Final ORT model saved to {ORT_PATH}")

if __name__ == "__main__":
    export_model()
