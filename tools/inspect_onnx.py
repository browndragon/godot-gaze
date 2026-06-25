# tools/inspect_onnx.py
import onnx
import sys
import os

def inspect(model_path):
    if not os.path.exists(model_path):
        print(f"Error: {model_path} not found.")
        return
    print(f"\n================ Inspecting {os.path.basename(model_path)} ================")
    model = onnx.load(model_path)
    
    print("Inputs:")
    for input in model.graph.input:
        shape = [dim.dim_value for dim in input.type.tensor_type.shape.dim]
        print(f"  Name: {input.name}, Shape: {shape}, Type: {input.type.tensor_type.elem_type}")
        
    print("Outputs:")
    for output in model.graph.output:
        shape = [dim.dim_value for dim in output.type.tensor_type.shape.dim]
        print(f"  Name: {output.name}, Shape: {shape}, Type: {output.type.tensor_type.elem_type}")

if __name__ == "__main__":
    inspect("project/models/face_detection_yunet_2023mar.onnx")
    inspect("project/models/gaze-estimation-adas-0002.onnx")
