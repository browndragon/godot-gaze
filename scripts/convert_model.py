#!/usr/bin/env python3
import sys
import numpy as np

# 1. Create a mock onnx.mapping module and register it in sys.modules
from types import ModuleType
mapping_mod = ModuleType("onnx.mapping")

class DummyTypeInfo:
    def __init__(self, np_dtype):
        self.np_dtype = np_dtype

# Map TensorProto element type enum integers to numpy types
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

# 2. Now run the main conversion logic of openvino2onnx
from openvino2onnx.__main__ import main
sys.argv = [
    "openvino2onnx",
    "test_assets/models/gaze-estimation-adas-0002.xml",
    "project/addons/godot-gaze/models/gaze-estimation-adas-0002.onnx"
]
main()
