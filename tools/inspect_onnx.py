# TODO: This does not meet our current code standards. It hardcodes the paths (should be flags, though default values ok). The inspection is pretty sparse. Needs documentation about what it does and enforces.
#       Again, I think this could be converted into a unit test, because all we _actually_ want to enforce is that the input and output shapes are of the expected types! No reason to be coy and require examination, just encode that the data are the shape we think!
#       If this were a tool, it would operate on file(s) and output data about those file(s) -- which, I suppose, we could reuse. So this could have a presence in `scripts/` for some of what it's doing if you want, with real --help text and input handling.
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
    inspect("project/addons/godot-gaze/models/face_detection_yunet_2023mar.onnx")
    inspect("project/addons/godot-gaze/models/gaze-estimation-adas-0002.onnx")
