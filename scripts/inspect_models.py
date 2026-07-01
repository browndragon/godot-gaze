import onnx
import sys

# TODO: I think we have this as a tool/ too? Why both?

def inspect_model_onnx(path):
    print(f"Inspecting {path} via onnx:")
    try:
        model = onnx.load(path)
        graph = model.graph
        print("  Inputs:")
        for i in graph.input:
            print(f"    Name: {i.name}")
        print("  Outputs:")
        for o in graph.output:
            print(f"    Name: {o.name}")
    except Exception as e:
        print(f"  Error loading model: {e}")

inspect_model_onnx("project/addons/godot-gaze/models/face_detection_yunet_2023mar.onnx")
inspect_model_onnx("project/addons/godot-gaze/models/gaze-estimation-adas-0002.onnx")
