#!/usr/bin/env python3
# scripts/generate_gaze_model.py
# Generates a dummy appearance-based gaze estimation ONNX model with the exact input/output nodes required by godot-gaze.

import os
import sys

def main():
    print("=== Generating Dummy Gaze ONNX Model ===")
    
    # 1. Ensure torch and onnx are installed
    try:
        import torch
        import torch.nn as nn
    except ImportError:
        print("Error: PyTorch not found. Please run this script within the prepared virtual environment.")
        sys.exit(1)
        
    try:
        import onnx
    except ImportError:
        print("Error: ONNX package not found. Please run this script within the prepared virtual environment.")
        sys.exit(1)

    # 2. Define a simple, dummy gaze network structure
    class DummyGazeModel(nn.Module):
        def __init__(self):
            super(DummyGazeModel, self).__init__()
            # Simple convolutional layers for eye crops (grayscale 36x60)
            self.eye_features = nn.Sequential(
                nn.Conv2d(1, 4, kernel_size=3, padding=1),
                nn.ReLU(),
                nn.MaxPool2d(2), # 18x30
                nn.Conv2d(4, 8, kernel_size=3, padding=1),
                nn.ReLU(),
                nn.AdaptiveAvgPool2d((1, 1)),
                nn.Flatten() # 8 features
            )
            # Fully connected layers combining left eye, right eye, and 2D head pose (pitch, yaw)
            self.fc = nn.Sequential(
                nn.Linear(8 + 8 + 2, 16),
                nn.ReLU(),
                nn.Linear(16, 3) # Outputs a 3D gaze direction vector (Case B in C++)
            )

        def forward(self, left_eye_input, right_eye_input, head_pose_input):
            feat_l = self.eye_features(left_eye_input)
            feat_r = self.eye_features(right_eye_input)
            # Concatenate features: shape [batch, 8 + 8 + 2 = 18]
            combined = torch.cat([feat_l, feat_r, head_pose_input], dim=1)
            out = self.fc(combined)
            # Normalize to return a unit vector
            norm = torch.norm(out, dim=1, keepdim=True) + 1e-6
            return out / norm

    # 3. Instantiate model
    model = DummyGazeModel()
    model.eval()

    # 4. Define dummy inputs matching the required shapes
    # left_eye_input: [batch, 1, 36, 60]
    # right_eye_input: [batch, 1, 36, 60]
    # head_pose_input: [batch, 2]
    dummy_left = torch.randn(1, 1, 36, 60, dtype=torch.float32)
    dummy_right = torch.randn(1, 1, 36, 60, dtype=torch.float32)
    dummy_head = torch.randn(1, 2, dtype=torch.float32)

    # 5. Export model to ONNX
    output_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../project/models"))
    os.makedirs(output_dir, exist_ok=True)
    output_path = os.path.join(output_dir, "gaze.onnx")

    print(f"Exporting ONNX model to: {output_path}...")
    torch.onnx.export(
        model,
        (dummy_left, dummy_right, dummy_head),
        output_path,
        export_params=True,
        opset_version=11,
        do_constant_folding=True,
        input_names=["left_eye_input", "right_eye_input", "head_pose_input"],
        output_names=["gaze_output"],
        dynamic_axes={
            "left_eye_input": {0: "batch_size"},
            "right_eye_input": {0: "batch_size"},
            "head_pose_input": {0: "batch_size"},
            "gaze_output": {0: "batch_size"}
        }
    )

    # 6. Verify model validity
    try:
        onnx_model = onnx.load(output_path)
        onnx.checker.check_model(onnx_model)
        print("ONNX model generated and verified successfully!")
    except Exception as e:
        print(f"ONNX validation warning: {e}")

if __name__ == "__main__":
    main()
