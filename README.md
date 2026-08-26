# godot-gaze

A high-frequency, real-time 3D gaze estimation & projecting GDExtension plugin for Godot 4.7+.

---

## Key Features

- **Cross-Platform Gaze Tracking**:
  - **Native Desktop & Mobile (macOS, Windows, Linux, iOS, Android)**: Runs an optimized CPU-based **ONNX Runtime (CPU/XNNPACK)** pipeline for face detection (YuNet) and gaze estimation (OpenVINO vehicle models).
  - **WebAssembly (WASM)**: Leverages browser-native **ONNX Runtime Web** (WebGPU/WebGL accelerated) and the same models as the desktop.
- **Background Inference**:
  - **Native (Desktop/Mobile)**: All ML model estimation runs in a dedicated C++ background worker thread (`GazeTrackingPipeline`) with lock-free double buffering.
  - **Web (WASM)**: Camera capture and inference run asynchronously outside the WebAssembly runtime via the browser sidecar (`gaze_sidecar.js`) using ONNX Runtime Web (WebGPU/WebGL accelerated).
- **Principled 3D Spherical Calibration**: Corrects biological user differences using 3D angular biases (pitch/yaw) applied directly to the estimated 3D gaze vector.
- **Adaptive Smoothing**: Implements a **1 Euro Filter** to suppress saccadic jitter during steady gaze while preserving low latency during rapid eye movements.

---

## Getting Started

### 1. Installation

You can install `godot-gaze` using one of the following methods:

#### A. From Pre-compiled Release (Recommended)

1. Download the latest `godot-gaze.zip` from the [GitHub Releases](https://github.com/browndragon/godot-gaze/releases) page.
2. Extract the archive and copy the `addons/godot-gaze` directory directly into your Godot project's root `addons/` directory.
3. Open your project in the Godot Editor, navigate to **Project Settings > Plugins**, and enable the `Godot Gaze` plugin.

#### B. Local Development Symlinking (For Contributors)

If you are developing this plugin locally and want to test changes in your own Godot project without repeatedly copying files:

1. Create a symbolic link from your project's `addons` directory back to this repository's addon folder:
   ```bash
   ln -s /path/to/godot-gaze/project/addons/godot-gaze /path/to/your-project/addons/godot-gaze
   ```
2. When you run `scons` to rebuild any template (e.g. `scons platform=macos target=editor`), the compiled binaries in `project/addons/godot-gaze/bin` are instantly available and loaded by your target project.

### 2. Basic Setup

1. **Autostart & Configuration**:
   When the plugin is enabled, it automatically registers project settings under `gaze/`:
   - `gaze/general/autostart` (default `true`): Starts camera tracking automatically on startup.
   - `gaze/pointing/emulate_gaze_from_mouse` (default `true`): Emulates `InputEventGaze` from mouse cursor movements using 3D inverse kinematics when camera tracking is inactive.
   - `gaze/pointing/emulate_mouse_from_gaze` (default `false`): Dispatches synthetic mouse move events based on gaze coordinates.
   - `gaze/calibration/device_calibration_path` (defaults to `user://calibrations/device_calibration.tres`).
   - `gaze/calibration/bio_calibration_path` (defaults to `user://calibrations/bio_calibration.tres`).
2. **Model Weights**:
   Model weights (`.ort` format) are pre-bundled inside `addons/godot-gaze/models/`.

### 3. Basic GDScript Usage

Nodes receive gaze events idiomatically through `_unhandled_input(event)`:

```gdscript
extends Node2D

@onready var cursor: Node2D = $Cursor
@onready var status: Label = $Status

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventGaze:
        # 2D Screen / Viewport position
        cursor.global_position = event.position
        status.text = "Face Tracked (Openness L: %.2f, R: %.2f)" % [event.left_eye_openness, event.right_eye_openness]

        # 3D Head & Gaze transforms
        var head_pos_mm = event.head_transform.origin
        var gaze_ray_dir = event.gaze_transform.basis.z * -1.0
    elif event is InputEventGazeMissing:
        status.text = "Gaze Lost (Reason: %d)" % event.reason

func _ready() -> void:
    # Tracking starts automatically when autostart is true.
    # You can also manually control tracking refcounts:
    var gs = Engine.get_singleton("GazeServer")
    if gs:
        gs.start_tracking()
```

---

## Web / HTML5 Export Guidelines

When exporting your project to the Web, keep the following considerations in mind:

1. **Automatic Sidecar Export**: The Gaze Tracker plugin includes an editor export plugin (`export_plugin.gd`) that automatically copies `gaze_sidecar.js` to your HTML5 export folder.
2. **Cross-Origin Isolation Requirements (For Threaded Exports)**:
   - Godot's WebAssembly export template supports multi-threading via SharedArrayBuffer. If you enable **Thread Support** in your Godot Web Export Preset options, the hosting server **must** serve your game with the following HTTP response headers:
     ```http
     Cross-Origin-Opener-Policy: same-origin
     Cross-Origin-Embedder-Policy: require-corp
     ```
   - These headers are standard browser security requirements for SharedArrayBuffer (not specific to this plugin). If your hosting environment (e.g. itch.io, GitHub Pages, or corporate intranet) does not allow you to configure these custom headers, you **must disable Thread Support** in your Godot export preset, which falls back to the non-threaded WebAssembly template.

---

## Developer Guidelines & Architecture

- For the full system architecture, layer isolation rules, and concurrency model, refer to [System Architecture & Technical Specification](docs/architecture.md).
- For guidelines on compiling the plugin from source, running test suites, or contributing, please refer to [CONTRIBUTING.md](CONTRIBUTING.md).
- For mathematical and physical projection guides, refer to [docs/mathematical_model.md](docs/mathematical_model.md).
