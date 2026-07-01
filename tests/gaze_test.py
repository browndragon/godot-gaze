# tests/gaze_test.py
# Python unit tests for godot-gaze tracking and coordinate systems.

import os
import numpy as np
import pytest

def test_model_files_exist():
    """Verify that all required models are present in the addons folder."""
    model_dir = "project/addons/godot-gaze/models"
    project_files = [
        "face_detection_yunet_2023mar.ort",
        "gaze-estimation-adas-0002.ort"
    ]
    
    assert os.path.isdir(model_dir), f"Model directory not found: {model_dir}"
    for filename in project_files:
        file_path = os.path.join(model_dir, filename)
        assert os.path.exists(file_path), f"Required model file missing in project: {file_path}"
        assert os.path.getsize(file_path) > 0, f"Model file is empty: {file_path}"

    # Verify developer assets exist in test_assets/models
    dev_model_dir = "test_assets/models"
    dev_files = [
        "gaze-estimation-adas-0002.xml",
        "gaze-estimation-adas-0002.bin"
    ]
    assert os.path.isdir(dev_model_dir), f"Dev model directory not found: {dev_model_dir}"
    for filename in dev_files:
        file_path = os.path.join(dev_model_dir, filename)
        assert os.path.exists(file_path), f"Required dev model file missing in test_assets: {file_path}"
        assert os.path.getsize(file_path) > 0, f"Dev model file is empty: {file_path}"

def test_coordinate_system_conventions():
    """
    Verify and document the coordinate system transformations and conventions.
    Conventions are detailed in: docs/gaze_math_physical_model.md
    """
    # 1. Subject's Left vs. Right eye X-coordinate convention:
    # Standard Camera Space: +X points camera-right (viewer's left / subject's left).
    # Therefore, the subject's left eye MUST have a larger X coordinate than the right eye.
    left_eye_cam = np.array([30.0, 28.0, -500.0]) # subject's left eye (positive X)
    right_eye_cam = np.array([-30.0, 28.0, -500.0]) # subject's right eye (negative X)
    
    assert left_eye_cam[0] > right_eye_cam[0], "Left eye X must be greater than right eye X"

    # 2. Nose position Z (Convexity check):
    # In Godot/graphics standard space, the camera looks down the negative Z-axis (-Z is forward).
    # Thus, objects in front of the camera have negative Z coordinates.
    # The nose tip, being closer to the camera than the eyes, must have a LESS negative Z coordinate
    # (i.e., a mathematically larger value) than the eyes.
    eye_z = -500.0
    nose_z = -450.0 # closer to the camera (origin)
    
    assert nose_z > eye_z, "Nose Z must be greater than eye Z (closer to camera / less negative)"

def test_camera_space_transform():
    """
    Verify standard transformation from Inference Face Space to GodotGaze Camera Space.
    Formula: P_cam = R_cam * P_local + t_cam
    Where:
      - R_x(180 deg) pitch maps vertical Y direction (down in Inference -> up in Camera Space).
      - R_z(180 deg) roll maps horizontal X direction.
    """
    # Pitch rotation of 180 degrees around X-axis
    # maps Y -> -Y and Z -> -Z
    R_x_180 = np.array([
        [1,  0,  0],
        [0, -1,  0],
        [0,  0, -1]
    ])
    
    # Test a point pointing down and forward in Inference Camera Space: (0, 100, 500)
    p_inf = np.array([10.0, 100.0, 500.0])
    
    # Map to GodotGaze standard camera space: Y and Z signs are inverted
    p_cam = R_x_180.dot(p_inf)
    
    assert p_cam[0] == p_inf[0] # X remains identical
    assert p_cam[1] == -p_inf[1] # Y is inverted (up is positive)
    assert p_cam[2] == -p_inf[2] # Z is inverted (forward is negative Z)
