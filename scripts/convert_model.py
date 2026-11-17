#!/usr/bin/env python3
"""Convert OpenVINO IR and ONNX model assets into ONNX Runtime (.ort) files."""

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
from types import ModuleType

BASE_DIR = Path(__file__).resolve().parent.parent
ASSETS_DIR = BASE_DIR / "test_assets" / "models"
DEFAULT_MODELS_DIR = BASE_DIR / "project" / "addons" / "godot-gaze" / "models"
REQ_OPS_SRC = BASE_DIR / "scripts" / "required_operators.config"

MODELS_TO_CONVERT = [
    "gaze-estimation-adas-0002",
    "facial-landmarks-35-adas-0002",
    "open_closed_eye",
]


def _validate_dependencies() -> None:
    missing = []
    for pkg in ("numpy", "onnx", "onnxruntime"):
        try:
            __import__(pkg)
        except ImportError:
            missing.append(pkg)
    if missing:
        raise RuntimeError(
            f"Python interpreter '{sys.executable}' is missing required packages: {', '.join(missing)}. "
            f"Install with: {sys.executable} -m pip install numpy openvino2onnx onnx onnxruntime==1.28.0"
        )


def _convert_xml_to_onnx(xml_path: Path, onnx_path: Path) -> None:
    import numpy as np

    try:
        import openvino2onnx  # noqa: F401
    except ImportError as exc:
        raise RuntimeError(
            f"Missing required package 'openvino2onnx' in '{sys.executable}'. "
            f"Install with: {sys.executable} -m pip install openvino2onnx"
        ) from exc

    # Provide onnx.mapping compatibility for openvino2onnx against modern onnx releases
    mapping_mod = ModuleType("onnx.mapping")

    class DummyTypeInfo:
        def __init__(self, np_dtype):
            self.np_dtype = np_dtype

    mapping_mod.TENSOR_TYPE_MAP = {
        1: DummyTypeInfo(np.dtype("float32")),
        2: DummyTypeInfo(np.dtype("uint8")),
        3: DummyTypeInfo(np.dtype("int8")),
        4: DummyTypeInfo(np.dtype("uint16")),
        5: DummyTypeInfo(np.dtype("int16")),
        6: DummyTypeInfo(np.dtype("int32")),
        7: DummyTypeInfo(np.dtype("int64")),
        8: DummyTypeInfo(np.dtype("object")),
        9: DummyTypeInfo(np.dtype("bool")),
        10: DummyTypeInfo(np.dtype("float16")),
        11: DummyTypeInfo(np.dtype("float64")),
        12: DummyTypeInfo(np.dtype("uint32")),
        13: DummyTypeInfo(np.dtype("uint64")),
        14: DummyTypeInfo(np.dtype("complex64")),
        15: DummyTypeInfo(np.dtype("complex128")),
        16: DummyTypeInfo(np.dtype("uint16")),
    }
    sys.modules["onnx.mapping"] = mapping_mod

    from openvino2onnx.__main__ import main as o2o_main

    print(f"Converting {xml_path} -> {onnx_path}...")
    saved_argv = list(sys.argv)
    try:
        sys.argv = ["openvino2onnx", str(xml_path), str(onnx_path)]
        o2o_main()
    finally:
        sys.argv = saved_argv
    print(f"Successfully converted {xml_path} to {onnx_path}")


def _standardize_open_closed_eye_onnx(onnx_path: Path) -> None:
    import onnx
    from onnx import helper

    model = onnx.load(str(onnx_path))
    if len(model.graph.node) > 10:
        new_nodes = list(model.graph.node[:9])
        softmax_node = helper.make_node(
            "Softmax",
            inputs=["16"],
            outputs=["19"],
            axis=1,
            name="softmax",
        )
        new_nodes.append(softmax_node)
        graph = helper.make_graph(
            new_nodes,
            model.graph.name,
            model.graph.input,
            model.graph.output,
            initializer=model.graph.initializer,
        )
        new_model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 14)])
        new_model.ir_version = 7
        onnx.save(new_model, str(onnx_path))
        print(f"Standardized {onnx_path} to opset 14 Softmax.")


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Convert OpenVINO/ONNX gaze models into .ort format.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_MODELS_DIR,
        help="Destination directory for converted .ort models (default: project/addons/godot-gaze/models).",
    )
    return parser.parse_args()


def main() -> None:
    args = _parse_args()
    _validate_dependencies()

    models_dir = args.output_dir.resolve()
    models_dir.mkdir(parents=True, exist_ok=True)

    for model_name in MODELS_TO_CONVERT:
        xml_path = ASSETS_DIR / f"{model_name}.xml"
        onnx_path = models_dir / f"{model_name}.onnx"
        ort_path = models_dir / f"{model_name}.ort"

        if xml_path.exists() and not onnx_path.exists() and not ort_path.exists():
            _convert_xml_to_onnx(xml_path, onnx_path)

        if model_name == "open_closed_eye":
            asset_onnx = ASSETS_DIR / "open_closed_eye.onnx"
            if not onnx_path.exists() and not ort_path.exists() and asset_onnx.exists():
                shutil.copy2(asset_onnx, onnx_path)
            if onnx_path.exists():
                _standardize_open_closed_eye_onnx(onnx_path)

        if onnx_path.exists() and (
            not ort_path.exists() or onnx_path.stat().st_mtime > ort_path.stat().st_mtime
        ):
            opt_style = (
                "Runtime"
                if model_name in ("gaze-estimation-adas-0002", "facial-landmarks-35-adas-0002")
                else "Fixed"
            )
            print(f"Converting {onnx_path} -> {ort_path} (optimization_style={opt_style})...")
            subprocess.run(
                [
                    sys.executable,
                    "-m",
                    "onnxruntime.tools.convert_onnx_models_to_ort",
                    f"--optimization_style={opt_style}",
                    str(onnx_path),
                    "--output_dir",
                    str(models_dir),
                ],
                check=True,
            )

            with_opt = models_dir / f"{model_name}.with_runtime_opt.ort"
            if with_opt.exists() and (opt_style == "Runtime" or not ort_path.exists()):
                shutil.move(str(with_opt), str(ort_path))
            elif with_opt.exists():
                with_opt.unlink()

            for temp_cfg in (
                models_dir / f"{model_name}.required_operators.config",
                models_dir / f"{model_name}.required_operators.with_runtime_opt.config",
                models_dir / "required_operators.with_runtime_opt.config",
            ):
                if temp_cfg.exists():
                    temp_cfg.unlink()

            print(f"Successfully generated {ort_path}")

    req_ops_dst = models_dir / "required_operators.config"
    if not req_ops_dst.exists() and REQ_OPS_SRC.exists():
        shutil.copy2(REQ_OPS_SRC, req_ops_dst)


if __name__ == "__main__":
    main()
