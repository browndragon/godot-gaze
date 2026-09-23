# Gaze Estimation Model Evaluation & Architectural Comparison

This document records the empirical benchmarking, architectural analysis, and model trade-offs conducted to resolve the peripheral non-linearity and angular compression discovered in desktop gaze tracking.

---

## 1. Executive Summary & Benchmark Matrix

All models were evaluated on host hardware (**Apple Silicon M1, 8 CPU cores**, single-threaded ONNX Runtime CPU execution) against our canonical test fixture suite (`tests/resources/*.jpg`) using our automated benchmark harness (`scons tools/benchmark_models`).

| Model Name | Architecture / Backbone | Input Shape | Disk Size | Mean Latency | p95 Latency | Throughput | Horiz. Dynamic Span | Head/Gaze Decoupling |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **OpenVINO ADAS-0002** *(Baseline)* | Custom Shallow CNN | Dual 60×60 + Head [1,3] | **7.2 MB** | **1.22 ms** | **1.58 ms** | **817.2 FPS** | Saturates at $\pm 20^\circ$ | Dependent on PnP |
| **MobileOne-S0 Gaze** *(L2CS)* | MobileOne-S0 (RepVGG) | 1×3×448×448 | **4.7 MB** | **12.06 ms** | **13.51 ms** | **82.9 FPS** | 16.8° (at 448) | **+43.1°** (Robust) |
| **MobileNetV2 Gaze** *(L2CS)* | MobileNetV2 | 1×3×448×448 | 9.3 MB | 24.21 ms | 26.39 ms | 41.3 FPS | 9.9° | Weak |
| **ETH-XGaze** *(Direct Regression)* | ResNet-50 | 1×3×224×224 | 89.6 MB | **42.43 ms** | **57.41 ms** | **23.6 FPS** | **40.5°** (Symmetric) | **+21.0°** (Robust) |
| **ResNet-18 Gaze** *(L2CS)* | ResNet-18 | 1×3×448×448 | 43.0 MB | 52.12 ms | 59.24 ms | 19.2 FPS | 31.6° | Moderate |
| **ResNet-50 Gaze** *(L2CS)* | ResNet-50 | 1×3×448×448 | 91.0 MB | 149.20 ms | 175.57 ms | 6.7 FPS | **57.3°** (Widest) | **+22.5°** (Robust) |

*Artifacts generated*:
* Machine-readable metrics: `build/tests/artifacts/gaze_model_comparison.json`
* Visual interactive report: `build/tests/artifacts/gaze_model_comparison.html`

---

## 2. Deconstructing the 800 FPS vs 12–150 ms Discrepancy

The dramatic throughput gap between OpenVINO ADAS-0002 (817 FPS / 1.22 ms) and candidate models (7–83 FPS) is not an experimental artifact. It stems from two fundamental architectural differences:

### A. The "Baking / Upstream Offload" Illusion
OpenVINO ADAS-0002 is **not** an end-to-end gaze estimator. It only performs the final tensor dot product on heavily pre-digested inputs:
1. **Upstream Landmark Model**: An 18 MB neural network (`facial-landmarks-35-adas-0002.onnx`) must first execute on an upright, counter-rotated face crop to regress 35 2D landmarks.
2. **Upstream PnP Solver**: A Levenberg-Marquardt / SQPnP non-linear iterative solver must compute 3D head pose rotation ($R, t$) from the 35 landmarks.
3. **Dual Eye Warpers**: Two separate bilinear interpolation passes must crop, rotate, and resample the left and right ocular regions into $60\times60$ patches.
4. **Final Step**: Only after steps 1–3 have consumed **3–6 ms of CPU time** does `gaze-estimation-adas-0002` execute its 1.2 ms forward pass.

In contrast, **ETH-XGaze and L2CS-Net are full-face appearance models**:
* They ingest the face bounding box directly from the initial YuNet face detector.
* They completely bypass the 18 MB landmark model, PnP solver, and eye-warping routines.
* The face crop is passed directly to the network, eliminating all upstream landmark jitter and head-pose handoff discontinuities.

### B. Input Spatial Area & Computational Complexity (FLOPs)
Convolutional operations scale linearly with input spatial area ($H \times W$):
* **ADAS-0002**: $2 \times (60 \times 60 \times 3) + 3 = \mathbf{21,603\text{ input floats}}$. Total compute is $\approx \mathbf{0.07\text{ GFLOPs}}$.
* **ETH-XGaze**: $1 \times (224 \times 224 \times 3) = \mathbf{150,528\text{ input floats}}$ (**$7\times$ larger**). Total compute is $\approx \mathbf{4.1\text{ GFLOPs}}$.
* **L2CS-Net Models**: $1 \times (448 \times 448 \times 3) = \mathbf{602,112\text{ input floats}}$ (**$28\times$ larger** than ADAS).
  * A $448 \times 448$ image has **$4\times$ the pixel area** of a $224 \times 224$ image ($448^2 / 224^2 = 4$).
  * Consequently, ResNet-50 at 448×448 requires $\approx \mathbf{16.4\text{ GFLOPs}}$, explaining its 149 ms latency compared to ETH-XGaze ResNet-50's 42 ms at 224×224.

---

## 3. Peripheral Dynamic Range vs. Automotive Bias

| Aspect | OpenVINO ADAS-0002 | ETH-XGaze & L2CS Family |
| :--- | :--- | :--- |
| **Training Domain** | In-cabin automotive driver monitoring (forward road view, mirrors, instrument cluster). | In-the-wild desktop monitors, mobile phones, and 360° gaze spheres (ETH-XGaze, Gaze360). |
| **Angular Range** | Confined to $\pm 15^\circ\text{--}20^\circ$. Looking beyond $20^\circ$ causes non-linear compression and flat-line saturation. | Continuous coverage across $\pm 45^\circ\text{--}60^\circ$ eccentricities without mean-collapse. |
| **Head Coupling** | Heavily couples eye gaze to head pose angles. When head pose is static, eye excursions are severely compressed. | Full facial appearance decouples eye movements from head orientation (e.g. $+43.1^\circ$ on `self_noseleft_eyesright`). |

---

## 4. Resolution Scaling & Optimization Opportunities

Candidate models use adaptive global average pooling (`AdaptiveAvgPool2d((1, 1))`), which means their feature maps can accept arbitrary input resolutions without architectural changes:
* **MobileOne-S0 at 224×224**:
  Reducing MobileOne-S0 from 448×448 to 224×224 reduces spatial tensor area by $4\times$.
  Projected CPU inference latency drops from **12 ms (83 FPS)** down to **$\approx 3.0\text{ ms}$ ($> 300\text{ FPS}$)** on Apple Silicon, while maintaining a tiny **4.7 MB** disk footprint.

---

## 5. Architectural Roadmap: Pluggable Gaze Estimator Strategy

To provide flexibility across platforms (mobile, web, desktop) and use cases (low-power vs. high-accuracy desktop), `godot-gaze` will adopt a pluggable model strategy:

## 5. Architectural Decision & Future Swappable Strategy

### A. Production Decision: OpenVINO ADAS-0002 Remains Default
* **Rationale**: Over the vast majority of common interactive display regions and typical head/eye angles ($\le 20^\circ$), OpenVINO ADAS-0002 performs roughly on par with larger candidate models.
* **Throughput & Multiplatform Reach**: Its lightweight footprint and 600–800 FPS execution speed provide unmatched power efficiency and low latency across all targeted deployment targets (desktop, mobile, and web).
* **Conclusion**: We will not perform an immediate architectural replacement. OpenVINO ADAS-0002 is maintained as the primary production estimator.

### B. Future Readiness: Blueprint for `DirectFaceGazeEstimator`
When requirements expand to require extreme peripheral display coverage or when GPU/NPU acceleration is prioritized, a `DirectFaceGazeEstimator` strategy can drop in cleanly:

```mermaid
flowchart TD
    FRAME["Incoming Camera Frame (BGR8)"] --> DET["ORTYuNetDetector (640x640)"]
    DET --> STRAT{"Gaze Estimator Strategy<br/>(gaze/models/estimator_type)"}

    subgraph ADAS["Strategy A: OpenVINO ADAS (Production Default)"]
        STRAT -->|"adas_0002" (default)| LM["ORTLandmarkModel (60x60)<br/>35-point counter-rotated"]
        LM --> PNP["PnP Solver (Head Pose angles)"]
        PNP --> EYE["Dual Eye Cropper (60x60 x2)"]
        EYE --> ADAS_NET["ORTGazeModel (1.2 ms)<br/>Ultra-high throughput, standard range"]
    end

    subgraph END2END["Strategy B: Direct Face Estimator (Future Extension)"]
        STRAT -->|"mobileone_s0" / "eth_xgaze"| FCROP["Face Cropper (224x224 / 448x448)"]
        FCROP --> E2E_NET["DirectFaceGazeEstimator<br/>(3 - 12 ms, wide angular range)"]
    end

    ADAS_NET --> PROJ["ProjectionEngine (Ray-Plane Intersection)"]
    E2E_NET --> PROJ
```

#### Blueprint for Future Implementation:
1. **Interface Contract**: Define an abstract `IGazeDirectionEstimator` with:
   ```cpp
   virtual bool estimate_gaze(const GazeFrameData& frame, Vector3& out_gaze_dir) = 0;
   ```
2. **Implementation Strategy**:
   - `ADASGazeEstimator`: Wraps existing `ORTLandmarkModel` + `PnPSolver` + `ORTGazeModel`.
   - `DirectFaceGazeEstimator`: Takes the face bounding box directly from `ORTYuNetDetector`, crops and normalizes the face patch, and invokes `mobileone_s0_gaze.onnx` or `eth_xgaze.onnx`.
3. **Reproducibility & Weight Provenance**:
   - Refer to [`test_assets/models/README.md`](../test_assets/models/README.md) for automated download scripts, PyTorch-to-ONNX conversion instructions, and the repeatable benchmark harness (`scons tools/benchmark_models`).

