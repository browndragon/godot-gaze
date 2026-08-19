#!/usr/bin/env python3
import sys
import os
import subprocess
import numpy as np
from types import ModuleType

# 1. Ensure required converter dependencies exist
try:
    import openvino2onnx
except ImportError:
    print("[convert_model] openvino2onnx not found. Installing via pip...")
    subprocess.run([sys.executable, "-m", "pip", "install", "openvino2onnx"], check=True)

# 2. Create a mock onnx.mapping module and register it in sys.modules for openvino2onnx compatibility
mapping_mod = ModuleType("onnx.mapping")

class DummyTypeInfo:
    def __init__(self, np_dtype):
        self.np_dtype = np_dtype

mapping_mod.TENSOR_TYPE_MAP = {
    1: DummyTypeInfo(np.dtype('float32')),
    2: DummyTypeInfo(np.dtype('uint8')),
    3: DummyTypeInfo(np.dtype('int8')),
    4: DummyTypeInfo(np.dtype('uint16')),
    5: DummyTypeInfo(np.dtype('int16')),
    6: DummyTypeInfo(np.dtype('int32')),
    7: DummyTypeInfo(np.dtype('int64')),
    8: DummyTypeInfo(np.dtype('object')),
    9: DummyTypeInfo(np.dtype('bool')),
    10: DummyTypeInfo(np.dtype('float16')),
    11: DummyTypeInfo(np.dtype('float64')),
    12: DummyTypeInfo(np.dtype('uint32')),
    13: DummyTypeInfo(np.dtype('uint64')),
    14: DummyTypeInfo(np.dtype('complex64')),
    15: DummyTypeInfo(np.dtype('complex128')),
    16: DummyTypeInfo(np.dtype('uint16')),
}

sys.modules["onnx.mapping"] = mapping_mod

# 3. Run openvino2onnx for all declared models
from openvino2onnx.__main__ import main

MODELS_TO_CONVERT = [
    "gaze-estimation-adas-0002",
    "facial-landmarks-35-adas-0002",
    "open_closed_eye",
]

for model_name in MODELS_TO_CONVERT:
    xml_path = f"test_assets/models/{model_name}.xml"
    onnx_path = f"project/addons/godot-gaze/models/{model_name}.onnx"
    ort_path = f"project/addons/godot-gaze/models/{model_name}.ort"

    if os.path.exists(xml_path) and not os.path.exists(onnx_path):
        print(f"Converting {xml_path} -> {onnx_path}...")
        sys.argv = ["openvino2onnx", xml_path, onnx_path]
        try:
            main()
            print(f"Successfully converted {xml_path} to {onnx_path}")
        except Exception as e:
            print(f"Error converting {xml_path}: {e}")

    if os.path.exists(onnx_path):
        try:
            import onnx
            from onnx import helper
            model = onnx.load(onnx_path)
            if model_name == "open_closed_eye" and len(model.graph.node) > 10:
                new_nodes = list(model.graph.node[:9])
                softmax_node = helper.make_node(
                    'Softmax',
                    inputs=['16'],
                    outputs=['19'],
                    axis=1,
                    name='softmax'
                )
                new_nodes.append(softmax_node)
                graph = helper.make_graph(
                    new_nodes,
                    model.graph.name,
                    model.graph.input,
                    model.graph.output,
                    initializer=model.graph.initializer
                )
                new_model = helper.make_model(graph, opset_imports=[helper.make_opsetid('', 14)])
                new_model.ir_version = 7
                onnx.save(new_model, onnx_path)
                print(f"Standardized {onnx_path} to opset 14 Softmax.")
        except Exception as e:
            print(f"Note: opset check for {onnx_path}: {e}")

    if os.path.exists(onnx_path) and (not os.path.exists(ort_path) or os.path.getmtime(onnx_path) > os.path.getmtime(ort_path)):
        print(f"Converting {onnx_path} -> {ort_path}...")
        try:
            subprocess.run([
                sys.executable, "-m", "onnxruntime.tools.convert_onnx_models_to_ort",
                onnx_path,
                "--output_dir", "project/addons/godot-gaze/models"
            ], check=True)
            print(f"Successfully generated {ort_path}")
        except Exception as e:
            print(f"Error generating ORT for {onnx_path}: {e}")
