#!/usr/bin/env python3
"""
Benchmark suite comparing candidate gaze estimation models against baseline OpenVINO ADAS-0002:
1. OpenVINO ADAS-0002 (Baseline)
2. ETH-XGaze (ResNet-50 Direct Regression)
3. MobileOne-S0 Gaze (L2CS Softmax Expectation)
4. MobileNetV2 Gaze (L2CS Softmax Expectation)
5. ResNet-18 Gaze (L2CS Softmax Expectation)
6. ResNet-50 Gaze (L2CS Softmax Expectation)

Metrics Evaluated:
- Model size (MB) & parameter count
- CPU Inference Latency on Apple Silicon M1 (Mean, Median, StdDev, p95, p99, FPS)
- Angular Dynamic Range & Peripheral Linearity on Canonical Fixtures
"""

import os
import sys
import time
import json
import math
import numpy as np
import cv2
import onnxruntime as ort

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
ARTIFACTS_DIR = os.path.join(PROJECT_ROOT, "build", "tests", "artifacts")
os.makedirs(ARTIFACTS_DIR, exist_ok=True)

import argparse

def find_model_file(filename, custom_dir=None, extra_candidates=None):
    candidates = []
    if custom_dir:
        candidates.append(os.path.join(custom_dir, filename))
    if extra_candidates:
        candidates.extend(extra_candidates)
    candidates.extend([
        os.path.join(PROJECT_ROOT, "test_assets", "models", "candidate_models", filename),
        os.path.expanduser(f"~/src/gaze-estimation/weights/{filename}"),
        f"/Users/acunningham/src/gaze-estimation/weights/{filename}",
        os.path.expanduser(f"~/src/ETH-XGaze/ckpt/{filename}"),
        f"/Users/acunningham/src/ETH-XGaze/ckpt/{filename}",
    ])
    for p in candidates:
        if p and os.path.exists(p):
            return p
    return None

def build_model_configs(weights_dir=None, eth_weights=None):
    configs = [
        {
            "name": "OpenVINO ADAS-0002 (Baseline)",
            "id": "adas_0002",
            "type": "openvino_adas",
            "path": os.path.join(PROJECT_ROOT, "project", "addons", "godot-gaze", "models", "gaze-estimation-adas-0002.onnx"),
            "input_shape": "left_eye: (1,3,60,60), right_eye: (1,3,60,60), head_pose: (1,3)",
            "output_type": "3D Cartesian unit vector [dx, dy, dz]",
            "backbone": "Custom Light CNN"
        },
        {
            "name": "MobileOne-S0 Gaze (L2CS)",
            "id": "mobileone_s0",
            "type": "l2cs",
            "path": find_model_file("mobileone_s0_gaze.onnx", weights_dir),
            "input_shape": "(1, 3, 448, 448)",
            "output_type": "90-bin Softmax expectation (yaw, pitch)",
            "backbone": "MobileOne-S0 (Reparameterized)"
        },
        {
            "name": "MobileNetV2 Gaze (L2CS)",
            "id": "mobilenetv2",
            "type": "l2cs",
            "path": find_model_file("mobilenetv2_gaze.onnx", weights_dir),
            "input_shape": "(1, 3, 448, 448)",
            "output_type": "90-bin Softmax expectation (yaw, pitch)",
            "backbone": "MobileNetV2"
        },
        {
            "name": "ResNet-18 Gaze (L2CS)",
            "id": "resnet18",
            "type": "l2cs",
            "path": find_model_file("resnet18_gaze.onnx", weights_dir),
            "input_shape": "(1, 3, 448, 448)",
            "output_type": "90-bin Softmax expectation (yaw, pitch)",
            "backbone": "ResNet-18"
        },
        {
            "name": "ResNet-50 Gaze (L2CS)",
            "id": "resnet50_l2cs",
            "type": "l2cs",
            "path": find_model_file("resnet50_gaze.onnx", weights_dir),
            "input_shape": "(1, 3, 448, 448)",
            "output_type": "90-bin Softmax expectation (yaw, pitch)",
            "backbone": "ResNet-50"
        },
        {
            "name": "ETH-XGaze (ResNet-50 Regression)",
            "id": "eth_xgaze",
            "type": "eth_xgaze",
            "path": find_model_file("eth_xgaze.onnx", weights_dir, extra_candidates=[eth_weights]),
            "input_shape": "(1, 3, 224, 224)",
            "output_type": "Direct 2D regression [pitch, yaw] (radians)",
            "backbone": "ResNet-50"
        }
    ]
    return configs


def count_onnx_params(onnx_path):
    try:
        import onnx
        model = onnx.load(onnx_path, load_external_data=False)
        total_params = 0
        for tensor in model.graph.initializer:
            dims = tensor.dims
            num_elements = 1
            for d in dims:
                num_elements *= d
            total_params += num_elements
        return total_params
    except Exception:
        return 0

def softmax(x):
    e_x = np.exp(x - np.max(x, axis=-1, keepdims=True))
    return e_x / np.sum(e_x, axis=-1, keepdims=True)

class YuNetDetector:
    def __init__(self, model_path):
        self.session = ort.InferenceSession(model_path, providers=['CPUExecutionProvider'])
        self.input_name = self.session.get_inputs()[0].name
        self.model_w = 640
        self.model_h = 640
        self.anchors = self._generate_anchors(self.model_w, self.model_h)

    def _generate_anchors(self, width, height):
        strides = [8, 16, 32]
        anchors = []
        for stride in strides:
            feature_w = int(math.ceil(width / stride))
            feature_h = int(math.ceil(height / stride))
            for i in range(feature_h):
                for j in range(feature_w):
                    anchors.append({
                        "cx": (j + 0.5) * stride,
                        "cy": (i + 0.5) * stride,
                        "stride": stride
                    })
        return anchors

    def detect(self, img):
        h, w = img.shape[:2]
        scale = min(self.model_w / w, self.model_h / h)
        new_w = int(w * scale)
        new_h = int(h * scale)
        pad_x = (self.model_w - new_w) // 2
        pad_y = (self.model_h - new_h) // 2

        resized = cv2.resize(img, (new_w, new_h), interpolation=cv2.INTER_LINEAR)
        letterbox = np.zeros((self.model_h, self.model_w, 3), dtype=np.uint8)
        letterbox[pad_y:pad_y + new_h, pad_x:pad_x + new_w, :] = resized

        inp = letterbox.transpose(2, 0, 1).astype(np.float32)
        inp = np.expand_dims(inp, axis=0)

        outputs = self.session.run(None, {self.input_name: inp})

        bboxes = []
        scores = []
        landmarks = []
        anchor_offset = 0
        strides = [8, 16, 32]

        for s in range(len(strides)):
            cls_tensor = outputs[s][0]
            obj_tensor = outputs[3 + s][0]
            bbox_tensor = outputs[6 + s][0]
            kps_tensor = outputs[9 + s][0]

            num_anchors = cls_tensor.shape[0]
            for idx in range(num_anchors):
                score = float(cls_tensor[idx][0] * obj_tensor[idx][0])
                if score > 0.35:
                    anchor = self.anchors[anchor_offset + idx]
                    st = anchor["stride"]
                    cx = bbox_tensor[idx][0] * st + anchor["cx"]
                    cy = bbox_tensor[idx][1] * st + anchor["cy"]
                    bw = math.exp(bbox_tensor[idx][2]) * st
                    bh = math.exp(bbox_tensor[idx][3]) * st

                    # Unpad and rescale
                    rx = (cx - pad_x) / scale
                    ry = (cy - pad_y) / scale
                    rw = bw / scale
                    rh = bh / scale

                    kps = kps_tensor[idx]
                    ldm = []
                    for j in range(5):
                        lx = (kps[j * 2 + 0] * st + anchor["cx"] - pad_x) / scale
                        ly = (kps[j * 2 + 1] * st + anchor["cy"] - pad_y) / scale
                        ldm.append((lx, ly))

                    bboxes.append([rx - rw / 2, ry - rh / 2, rw, rh])
                    scores.append(score)
                    landmarks.append(ldm)

            anchor_offset += num_anchors

        if not bboxes:
            # Fallback face detection box: center of image
            return {"box": [w * 0.2, h * 0.15, w * 0.6, h * 0.7], "kps": []}

        best = int(np.argmax(scores))
        return {"box": bboxes[best], "kps": landmarks[best]}

def run_latency_benchmark(sess, model_cfg, num_warmup=15, num_iter=100):
    input_type = model_cfg["type"]
    dummy_inputs = {}

    if input_type == "openvino_adas":
        dummy_inputs["left_eye_image"] = np.random.uniform(0, 255, (1, 3, 60, 60)).astype(np.float32)
        dummy_inputs["right_eye_image"] = np.random.uniform(0, 255, (1, 3, 60, 60)).astype(np.float32)
        dummy_inputs["head_pose_angles"] = np.array([[0.0, 0.0, 0.0]], dtype=np.float32)
    elif input_type == "eth_xgaze":
        dummy_inputs["input"] = np.random.randn(1, 3, 224, 224).astype(np.float32)
    elif input_type == "l2cs":
        dummy_inputs["input"] = np.random.randn(1, 3, 448, 448).astype(np.float32)

    # Warmup
    for _ in range(num_warmup):
        sess.run(None, dummy_inputs)

    times_ms = []
    for _ in range(num_iter):
        t0 = time.perf_counter()
        sess.run(None, dummy_inputs)
        t1 = time.perf_counter()
        times_ms.append((t1 - t0) * 1000.0)

    times_ms = np.array(times_ms)
    return {
        "mean_ms": float(np.mean(times_ms)),
        "median_ms": float(np.median(times_ms)),
        "std_ms": float(np.std(times_ms)),
        "p95_ms": float(np.percentile(times_ms, 95)),
        "p99_ms": float(np.percentile(times_ms, 99)),
        "fps": float(1000.0 / np.mean(times_ms))
    }

def predict_gaze(sess, model_cfg, img_bgr, face_info):
    h, w = img_bgr.shape[:2]
    bx, by, bw, bh = face_info["box"]
    kps = face_info.get("kps", [])

    model_type = model_cfg["type"]

    if model_type == "openvino_adas":
        # Eye crops:
        if len(kps) >= 2:
            r_eye_x, r_eye_y = kps[0] # anatomical right eye (image left)
            l_eye_x, l_eye_y = kps[1] # anatomical left eye (image right)
        else:
            r_eye_x, r_eye_y = bx + bw * 0.35, by + bh * 0.35
            l_eye_x, l_eye_y = bx + bw * 0.65, by + bh * 0.35

        def extract_eye_crop(cx, cy, size=60):
            half = size * (bw / 250.0)
            x0 = max(0, int(cx - half))
            y0 = max(0, int(cy - half))
            x1 = min(w, int(cx + half))
            y1 = min(h, int(cy + half))
            crop = img_bgr[y0:y1, x0:x1]
            if crop.size == 0:
                crop = np.zeros((size, size, 3), dtype=np.uint8)
            else:
                crop = cv2.resize(crop, (size, size))
            return crop.transpose(2, 0, 1).astype(np.float32)

        # left_eye_image is anatomical right (image left), right_eye_image is anatomical left (image right)
        left_eye_tensor = np.expand_dims(extract_eye_crop(r_eye_x, r_eye_y), axis=0)
        right_eye_tensor = np.expand_dims(extract_eye_crop(l_eye_x, l_eye_y), axis=0)
        head_pose = np.zeros((1, 3), dtype=np.float32)

        outputs = sess.run(None, {
            "left_eye_image": left_eye_tensor,
            "right_eye_image": right_eye_tensor,
            "head_pose_angles": head_pose
        })
        vec = outputs[0][0]
        # vec is [dx, dy, dz]
        dx, dy, dz = vec[0], vec[1], vec[2]
        yaw = math.degrees(math.atan2(dx, dz))
        pitch = math.degrees(math.atan2(-dy, dz))
        return float(yaw), float(pitch)

    elif model_type == "eth_xgaze":
        # Face crop resized to 224x224
        x0 = max(0, int(bx))
        y0 = max(0, int(by))
        x1 = min(w, int(bx + bw))
        y1 = min(h, int(by + bh))
        face_crop = img_bgr[y0:y1, x0:x1]
        if face_crop.size == 0:
            face_crop = np.zeros((224, 224, 3), dtype=np.uint8)
        else:
            face_crop = cv2.resize(face_crop, (224, 224))
        
        face_rgb = cv2.cvtColor(face_crop, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
        mean = np.array([0.485, 0.456, 0.406], dtype=np.float32)
        std = np.array([0.229, 0.224, 0.225], dtype=np.float32)
        norm = (face_rgb - mean) / std
        inp = np.expand_dims(norm.transpose(2, 0, 1), axis=0).astype(np.float32)

        out = sess.run(None, {"input": inp})[0][0]
        # ETH-XGaze outputs [pitch, yaw] in radians
        pitch = math.degrees(out[0])
        yaw = math.degrees(out[1])
        return float(yaw), float(pitch)

    elif model_type == "l2cs":
        # Face crop resized to 448x448
        # Enlarge bounding box slightly by 15% for stable facial features
        pad_w = bw * 0.15
        pad_h = bh * 0.15
        x0 = max(0, int(bx - pad_w))
        y0 = max(0, int(by - pad_h))
        x1 = min(w, int(bx + bw + pad_w))
        y1 = min(h, int(by + bh + pad_h))
        face_crop = img_bgr[y0:y1, x0:x1]
        if face_crop.size == 0:
            face_crop = np.zeros((448, 448, 3), dtype=np.uint8)
        else:
            face_crop = cv2.resize(face_crop, (448, 448))

        face_rgb = cv2.cvtColor(face_crop, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
        mean = np.array([0.485, 0.456, 0.406], dtype=np.float32)
        std = np.array([0.229, 0.224, 0.225], dtype=np.float32)
        norm = (face_rgb - mean) / std
        inp = np.expand_dims(norm.transpose(2, 0, 1), axis=0).astype(np.float32)

        out = sess.run(None, {"input": inp})
        yaw_logits = out[0]
        pitch_logits = out[1]

        yaw_probs = softmax(yaw_logits)
        pitch_probs = softmax(pitch_logits)

        bins = 90
        binwidth = 4.0
        angle_offset = 180.0
        idx_tensor = np.arange(bins, dtype=np.float32)

        yaw_deg = float(np.sum(yaw_probs * idx_tensor) * binwidth - angle_offset)
        pitch_deg = float(np.sum(pitch_probs * idx_tensor) * binwidth - angle_offset)
        return yaw_deg, pitch_deg

    return 0.0, 0.0

def main():
    print("=" * 80)
    print("GAZE ESTIMATION MODEL BENCHMARK & COMPARISON")
    print("Evaluating OpenVINO ADAS, ETH-XGaze, and L2CS Model Family")
    print("=" * 80)

    parser = argparse.ArgumentParser(description="Gaze Model Benchmark")
    parser.add_argument("--weights-dir", type=str, default=None, help="Directory containing candidate ONNX models")
    parser.add_argument("--eth-weights", type=str, default=None, help="Explicit path to eth_xgaze.onnx")
    args = parser.parse_args()

    model_configs = build_model_configs(args.weights_dir, args.eth_weights)

    # 1. Initialize YuNet face detector
    yunet_path = os.path.join(PROJECT_ROOT, "project", "addons", "godot-gaze", "models", "face_detection_yunet_2023mar.ort")
    detector = YuNetDetector(yunet_path)

    # 2. Canonical test fixtures
    fixture_dir = os.path.join(PROJECT_ROOT, "tests", "resources")
    test_images = [
        "self_center.jpg",
        "self_left_left.jpg",
        "self_right_right.jpg",
        "self_top_top.jpg",
        "self_down_down.jpg",
        "self_nosedown_eyesup.jpg",
        "self_nosetop_eyesdown.jpg",
        "self_noseleft_eyesright.jpg",
        "self_noseright_eyesleft.jpg"
    ]

    loaded_images = {}
    for img_name in test_images:
        p = os.path.join(fixture_dir, img_name)
        if os.path.exists(p):
            img = cv2.imread(p)
            det = detector.detect(img)
            loaded_images[img_name] = {"img": img, "face": det}
        else:
            print(f"[WARN] Test fixture missing: {p}")

    results = []

    for cfg in model_configs:
        model_name = cfg["name"]
        model_path = cfg["path"]
        print(f"\nEvaluating: {model_name}...")

        if not model_path or not os.path.exists(model_path):
            print(f"  [SKIPPED] Model file not found (see test_assets/models/README.md)")
            continue


        file_size_mb = os.path.getsize(model_path) / (1024 * 1024)
        params_count = count_onnx_params(model_path)

        # Create session
        t_load0 = time.perf_counter()
        sess = ort.InferenceSession(model_path, providers=['CPUExecutionProvider'])
        load_time_ms = (time.perf_counter() - t_load0) * 1000.0

        # Benchmark latency
        print("  Running 100-iteration CPU latency benchmark...")
        latency = run_latency_benchmark(sess, cfg, num_warmup=15, num_iter=100)

        # Run accuracy & dynamic range evaluation across test fixtures
        predictions = {}
        for img_name, data in loaded_images.items():
            yaw, pitch = predict_gaze(sess, cfg, data["img"], data["face"])
            predictions[img_name] = {"yaw_deg": round(yaw, 2), "pitch_deg": round(pitch, 2)}

        # Evaluate diagnostic metrics
        c_yaw = predictions.get("self_center.jpg", {}).get("yaw_deg", 0.0)
        c_pitch = predictions.get("self_center.jpg", {}).get("pitch_deg", 0.0)
        left_yaw = predictions.get("self_left_left.jpg", {}).get("yaw_deg", 0.0)
        right_yaw = predictions.get("self_right_right.jpg", {}).get("yaw_deg", 0.0)
        top_pitch = predictions.get("self_top_top.jpg", {}).get("pitch_deg", 0.0)
        down_pitch = predictions.get("self_down_down.jpg", {}).get("pitch_deg", 0.0)

        h_dynamic_range = abs(right_yaw - left_yaw)
        v_dynamic_range = abs(down_pitch - top_pitch)
        center_drift = math.sqrt(c_yaw ** 2 + c_pitch ** 2)

        res_entry = {
            "name": model_name,
            "id": cfg["id"],
            "backbone": cfg["backbone"],
            "file_size_mb": round(file_size_mb, 2),
            "params_millions": round(params_count / 1e6, 2) if params_count > 0 else "N/A",
            "load_time_ms": round(load_time_ms, 2),
            "latency_mean_ms": round(latency["mean_ms"], 2),
            "latency_median_ms": round(latency["median_ms"], 2),
            "latency_p95_ms": round(latency["p95_ms"], 2),
            "latency_std_ms": round(latency["std_ms"], 2),
            "fps": round(latency["fps"], 1),
            "horizontal_dynamic_range_deg": round(h_dynamic_range, 2),
            "vertical_dynamic_range_deg": round(v_dynamic_range, 2),
            "center_drift_deg": round(center_drift, 2),
            "predictions": predictions
        }
        results.append(res_entry)

    # Save JSON report
    json_path = os.path.join(ARTIFACTS_DIR, "gaze_model_comparison.json")
    with open(json_path, "w") as jf:
        json.dump(results, jf, indent=2)
    print(f"\n[OK] Benchmark JSON written to: {json_path}")

    # Save HTML report
    html_path = os.path.join(ARTIFACTS_DIR, "gaze_model_comparison.html")
    generate_html_report(results, html_path)
    print(f"[OK] Benchmark HTML report written to: {html_path}")

    # Print summary table to terminal
    print("\n" + "=" * 95)
    print(f"{'Model Name':<32} | {'Size':<8} | {'Latency':<9} | {'p95':<8} | {'FPS':<6} | {'H-Range':<8} | {'V-Range':<8}")
    print("=" * 95)
    for r in results:
        print(f"{r['name']:<32} | {r['file_size_mb']:>5.1f} MB | {r['latency_mean_ms']:>6.2f} ms | {r['latency_p95_ms']:>5.2f} ms | {r['fps']:>5.1f} | {r['horizontal_dynamic_range_deg']:>6.1f}° | {r['vertical_dynamic_range_deg']:>6.1f}°")
    print("=" * 95)


def generate_html_report(results, output_path):
    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Gaze Estimation Model Comparison & Benchmark</title>
<style>
  body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; margin: 24px; background: #0f172a; color: #f8fafc; }}
  h1, h2, h3 {{ color: #38bdf8; }}
  .card-grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 16px; margin-bottom: 24px; }}
  .card {{ background: #1e293b; border-radius: 8px; padding: 16px; border: 1px solid #334155; }}
  .card h3 {{ margin-top: 0; color: #94a3b8; font-size: 0.9rem; text-transform: uppercase; }}
  .card .val {{ font-size: 1.8rem; font-weight: bold; color: #38bdf8; }}
  .card .sub {{ font-size: 0.85rem; color: #64748b; margin-top: 4px; }}
  table {{ width: 100%; border-collapse: collapse; margin-top: 16px; margin-bottom: 32px; background: #1e293b; border-radius: 8px; overflow: hidden; }}
  th, td {{ padding: 12px 16px; text-align: left; border-bottom: 1px solid #334155; }}
  th {{ background: #0f172a; color: #94a3b8; font-size: 0.85rem; text-transform: uppercase; }}
  tr:hover {{ background: #283548; }}
  .badge {{ display: inline-block; padding: 4px 8px; border-radius: 4px; font-size: 0.75rem; font-weight: bold; }}
  .badge-best {{ background: #065f46; color: #34d399; }}
  .badge-warn {{ background: #854d0e; color: #fde047; }}
  .badge-danger {{ background: #991b1b; color: #fca5a5; }}
</style>
</head>
<body>
<h1>Gaze Estimation Model Comparison & Benchmark</h1>
<p>Empirical evaluation of candidate models on Apple Silicon M1 (ONNX Runtime CPU): size, CPU latency, throughput, and angular dynamic range on canonical test fixtures.</p>

<div class="card-grid">
  <div class="card">
    <h3>Fastest Inference</h3>
    <div class="val">{min(results, key=lambda x: x['latency_mean_ms'])['name']}</div>
    <div class="sub">{min(results, key=lambda x: x['latency_mean_ms'])['latency_mean_ms']:.2f} ms ({min(results, key=lambda x: x['latency_mean_ms'])['fps']:.1f} FPS)</div>
  </div>
  <div class="card">
    <h3>Smallest Footprint</h3>
    <div class="val">{min(results, key=lambda x: x['file_size_mb'])['name']}</div>
    <div class="sub">{min(results, key=lambda x: x['file_size_mb'])['file_size_mb']:.1f} MB</div>
  </div>
  <div class="card">
    <h3>Widest Horizontal Range</h3>
    <div class="val">{max(results, key=lambda x: x['horizontal_dynamic_range_deg'])['name']}</div>
    <div class="sub">{max(results, key=lambda x: x['horizontal_dynamic_range_deg'])['horizontal_dynamic_range_deg']:.1f}° span (vs {results[0]['horizontal_dynamic_range_deg']:.1f}° ADAS baseline)</div>
  </div>
</div>

<h2>1. Performance & Model Footprint</h2>
<table>
  <thead>
    <tr>
      <th>Model Name</th>
      <th>Backbone</th>
      <th>Disk Size</th>
      <th>Mean Latency</th>
      <th>Median</th>
      <th>p95 Latency</th>
      <th>Throughput</th>
    </tr>
  </thead>
  <tbody>
"""
    for r in results:
        html += f"""    <tr>
      <td><strong>{r['name']}</strong></td>
      <td>{r['backbone']}</td>
      <td>{r['file_size_mb']} MB</td>
      <td>{r['latency_mean_ms']} ms</td>
      <td>{r['latency_median_ms']} ms</td>
      <td>{r['latency_p95_ms']} ms</td>
      <td>{r['fps']} FPS</td>
    </tr>\n"""

    html += """  </tbody>
</table>

<h2>2. Angular Dynamic Range & Peripheral Coverage</h2>
<p>Measures how well each model covers wide desktop viewing spreads (avoiding OpenVINO's in-cabin peripheral saturation).</p>
<table>
  <thead>
    <tr>
      <th>Model Name</th>
      <th>Horiz. Range (Right - Left)</th>
      <th>Vert. Range (Down - Top)</th>
      <th>Center Drift</th>
      <th>Left Fixture (Yaw, Pitch)</th>
      <th>Center Fixture (Yaw, Pitch)</th>
      <th>Right Fixture (Yaw, Pitch)</th>
    </tr>
  </thead>
  <tbody>
"""
    for r in results:
        preds = r["predictions"]
        c = preds.get("self_center.jpg", {})
        l = preds.get("self_left_left.jpg", {})
        rt = preds.get("self_right_right.jpg", {})
        html += f"""    <tr>
      <td><strong>{r['name']}</strong></td>
      <td><strong>{r['horizontal_dynamic_range_deg']}°</strong></td>
      <td>{r['vertical_dynamic_range_deg']}°</td>
      <td>{r['center_drift_deg']}°</td>
      <td>({l.get('yaw_deg', 0)}°, {l.get('pitch_deg', 0)}°)</td>
      <td>({c.get('yaw_deg', 0)}°, {c.get('pitch_deg', 0)}°)</td>
      <td>({rt.get('yaw_deg', 0)}°, {rt.get('pitch_deg', 0)}°)</td>
    </tr>\n"""

    html += """  </tbody>
</table>

<h2>3. Canonical Fixture Predictions (All Fixtures)</h2>
<table>
  <thead>
    <tr>
      <th>Fixture</th>
"""
    for r in results:
        html += f"      <th>{r['name']} (Yaw, Pitch)</th>\n"
    html += """    </tr>
  </thead>
  <tbody>
"""
    # Sample fixture rows
    fixtures = list(results[0]["predictions"].keys()) if results else []
    for fix in fixtures:
        html += f"    <tr><td><code>{fix}</code></td>\n"
        for r in results:
            p = r["predictions"].get(fix, {})
            html += f"      <td>({p.get('yaw_deg', 0)}°, {p.get('pitch_deg', 0)}°)</td>\n"
        html += "    </tr>\n"

    html += """  </tbody>
</table>
</body>
</html>
"""
    with open(output_path, "w") as hf:
        hf.write(html)

if __name__ == "__main__":
    main()
