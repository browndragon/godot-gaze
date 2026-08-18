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
]

for model_name in MODELS_TO_CONVERT:
    xml_path = f"test_assets/models/{model_name}.xml"
    onnx_path = f"project/addons/godot-gaze/models/{model_name}.onnx"
    if os.path.exists(xml_path):
        if os.path.exists(onnx_path):
            print(f"Model already converted: {onnx_path}")
            continue
        print(f"Converting {xml_path} -> {onnx_path}...")
        sys.argv = ["openvino2onnx", xml_path, onnx_path]
        try:
            main()
            print(f"Successfully converted {xml_path} to {onnx_path}")
        except Exception as e:
            print(f"Error converting {xml_path}: {e}")
