# Test & Candidate Gaze Models

This directory contains developer model references and outlines procedures for fetching candidate gaze estimation models benchmarked during the 2026 Gaze Architecture Evaluation.

---

## 1. Production Baseline Models

The current production baseline utilizes the **Intel OpenVINO ADAS-0002** suite:
* `face_detection_yunet_2023mar.ort`: YuNet 5-point face detector (`[1, 3, 640, 640]` BGR NCHW).
* `facial-landmarks-35-adas-0002.onnx`: 35-point facial landmark regressor (`[1, 3, 60, 60]` BGR NCHW).
* `gaze-estimation-adas-0002.onnx`: Dual eye crop + head pose gaze regressor (`[1, 3, 60, 60]` x2 + `[1, 3]` angles).

---

## 2. Candidate Full-Face Models (Benchmark Assets)

During evaluation, candidate direct appearance-based models were benchmarked to assess peripheral dynamic range and pipeline simplification:

| Model Identifier | Source Repository | Checkpoint Format | Input Tensor Shape | Output Specification |
| :--- | :--- | :--- | :--- | :--- |
| `mobileone_s0_gaze.onnx` | [yakhyo/gaze-estimation](https://github.com/yakhyo/gaze-estimation) | ONNX (4.7 MB) | `[1, 3, 448, 448]` RGB | 90-bin Softmax expectation (yaw, pitch) |
| `mobilenetv2_gaze.onnx` | [yakhyo/gaze-estimation](https://github.com/yakhyo/gaze-estimation) | ONNX (9.3 MB) | `[1, 3, 448, 448]` RGB | 90-bin Softmax expectation (yaw, pitch) |
| `resnet18_gaze.onnx` | [yakhyo/gaze-estimation](https://github.com/yakhyo/gaze-estimation) | ONNX (43.0 MB) | `[1, 3, 448, 448]` RGB | 90-bin Softmax expectation (yaw, pitch) |
| `resnet50_gaze.onnx` | [yakhyo/gaze-estimation](https://github.com/yakhyo/gaze-estimation) | ONNX (91.0 MB) | `[1, 3, 448, 448]` RGB | 90-bin Softmax expectation (yaw, pitch) |
| `eth_xgaze.onnx` | [face-analysis/ETH-XGaze](https://github.com/face-analysis/ETH-XGaze) | PyTorch `epoch0040.model` (277 MB) $\to$ ONNX (89.6 MB) | `[1, 3, 224, 224]` RGB | Direct 2D regression `[pitch, yaw]` (radians) |

---

## 3. How to Fetch and Export Candidate Weights

To re-run the benchmark suite or test alternative models:

### A. L2CS-Net Model Family (MobileOne-S0, MobileNetV2, ResNet-18, ResNet-50)
Clone the repository and run their automated weight downloader:
```bash
git clone https://github.com/yakhyo/gaze-estimation.git
cd gaze-estimation
bash weights/download_weights.sh
# Weights will be downloaded to weights/*.onnx
```

### B. ETH-XGaze Official ResNet-50 Checkpoint
1. Download the pre-trained checkpoint from the official ETH-XGaze release:
   ```bash
   pip install gdown
   gdown "https://drive.google.com/uc?id=1Ma6zJrECNTjo_mToZ5GKk7EF-0FS4nEC" -O /path/to/epoch0040.model
   ```
2. Export to ONNX using legacy TorchScript exporter:
   ```python
   import torch
   from model import gaze_network
   
   net = gaze_network()
   ckpt = torch.load("epoch0040.model", map_location="cpu")
   net.load_state_dict({k.replace("module.", ""): v for k, v in ckpt["model_state"].items()})
   net.eval()
   
   dummy = torch.randn(1, 3, 224, 224)
   torch.onnx.export(
       net, dummy, "eth_xgaze.onnx",
       input_names=["input"], output_names=["pitch_yaw"],
       opset_version=14, dynamo=False
   )
   ```

---

## 4. Running the Benchmark Harness

Pass the path to your downloaded weights via the `--weights-dir` argument:
```bash
python3 tools/benchmark_gaze_models.py --weights-dir /path/to/weights/
# Or run via SCons:
scons tools/benchmark_models
```

Results and visual comparison charts will be output to:
* `build/tests/artifacts/gaze_model_comparison.json`
* `build/tests/artifacts/gaze_model_comparison.html`
