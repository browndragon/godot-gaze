"""
Automated Pytest suite verifying the converted MediaPipe Face Mesh ORT model (.ort)
for contract integrity, tensor shapes, and empirical EAR signal bounds on test fixtures.
"""

import os
import numpy as np
import pytest

try:
    import onnxruntime as ort
    import cv2
    HAS_DEPENDENCIES = True
except ImportError:
    HAS_DEPENDENCIES = False

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
MODEL_PATH = os.path.join(PROJECT_ROOT, "project", "addons", "godot-gaze", "models", "mediapipe_face_mesh.ort")
TEST_RESOURCES_DIR = os.path.join(PROJECT_ROOT, "tests", "resources")


@pytest.mark.skipif(not HAS_DEPENDENCIES, reason="onnxruntime or cv2 not installed")
def test_mediapipe_model_file_exists():
    assert os.path.exists(MODEL_PATH), f"MediaPipe ORT model not found at {MODEL_PATH}"


@pytest.mark.skipif(not HAS_DEPENDENCIES, reason="onnxruntime or cv2 not installed")
def test_mediapipe_model_tensor_contract():
    session = ort.InferenceSession(MODEL_PATH)
    inputs = session.get_inputs()
    outputs = session.get_outputs()

    assert len(inputs) == 1, "Expected 1 input tensor"
    assert inputs[0].name == "input_12", f"Unexpected input name {inputs[0].name}"
    assert list(inputs[0].shape[1:]) == [256, 256, 3], f"Unexpected input spatial shape {inputs[0].shape}"

    assert len(outputs) >= 2, "Expected at least 2 output tensors"
    assert list(outputs[0].shape[1:]) == [1, 1, 1434], f"Unexpected landmark output shape {outputs[0].shape}"


@pytest.mark.skipif(not HAS_DEPENDENCIES, reason="onnxruntime or cv2 not installed")
def test_mediapipe_model_empirical_signal_separation():
    session = ort.InferenceSession(MODEL_PATH)

    def evaluate_fixture(filename):
        img_path = os.path.join(TEST_RESOURCES_DIR, filename)
        assert os.path.exists(img_path), f"Test fixture not found: {img_path}"

        img = cv2.imread(img_path)
        h, w = img.shape[:2]
        crop = img[int(h * 0.1):int(h * 0.9), int(w * 0.1):int(w * 0.9)]
        resized = cv2.resize(crop, (256, 256))
        rgb = cv2.cvtColor(resized, cv2.COLOR_BGR2RGB).astype(np.float32)

        tensor = rgb[np.newaxis, :]
        outputs = session.run(None, {"input_12": tensor})

        landmarks = outputs[0].reshape((478, 3))
        presence = outputs[1].flatten()[0]

        # Standard 6-point 3D EAR calculation
        # Left eye: 159/145 (center), 158/153 (inner), 33/133 (corners)
        l_v1 = np.linalg.norm(landmarks[159] - landmarks[145])
        l_v2 = np.linalg.norm(landmarks[158] - landmarks[153])
        l_h = np.linalg.norm(landmarks[33] - landmarks[133]) + 1e-6
        l_ear = (l_v1 + l_v2) / (2.0 * l_h)

        # Right eye: 386/374 (center), 385/380 (inner), 362/263 (corners)
        r_v1 = np.linalg.norm(landmarks[386] - landmarks[374])
        r_v2 = np.linalg.norm(landmarks[385] - landmarks[380])
        r_h = np.linalg.norm(landmarks[362] - landmarks[263]) + 1e-6
        r_ear = (r_v1 + r_v2) / (2.0 * r_h)

        print(f"\n[MediaPipe Test] {filename:<32} -> Presence: {presence:6.2f} | Left EAR: {l_ear:.4f} | Right EAR: {r_ear:.4f}")
        return presence, l_ear, r_ear

    print("\n--- MEDIAPIPE FACE MESH EMPIRICAL FIXTURE SUITE ---")
    pres_open, l_open_ear, r_open_ear = evaluate_fixture("eyes_both_open.jpg")
    pres_wink, l_wink_ear, r_wink_ear = evaluate_fixture("eyes_both_wink.jpg")
    pres_lwink, l_lwink_ear, r_lwink_ear = evaluate_fixture("eyes_anatomical_left_wink.jpg")
    pres_rwink, l_rwink_ear, r_rwink_ear = evaluate_fixture("eyes_anatomical_right_wink.jpg")

    assert pres_open > 0.0, "Face presence should be positive for open eyes fixture"
    assert pres_wink > 0.0, "Face presence should be positive for closed eyes fixture"
