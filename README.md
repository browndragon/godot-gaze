# godot-gaze

A high-frequency, real-time 3D gaze estimation & projecting GDExtension plugin for Godot 4.7+.

---

## Key Features

- **Cross-Platform Gaze Tracking**:
  - **Native Desktop & Mobile (macOS, Windows, Linux, iOS, Android)**: Runs an optimized CPU-based **ONNX Runtime (CPU/XNNPACK)** pipeline for face detection (YuNet) and gaze estimation.
  - **WebAssembly (WASM)**: Leverages browser-native **ONNX Runtime Web** (WebGPU/WebGL accelerated) and MediaPipe Face Landmarker JS to process gaze outside the WebAssembly runtime, passing tracking data back to Godot.
- **Background Thread Inference**: All ML model estimation runs in a background worker thread (`GazeTrackingPipeline`) to keep Godot's main thread smooth and responsive.
- **Principled 3D Spherical Calibration**: Corrects biological user differences using 3D angular biases (pitch/yaw) applied directly to the estimated 3D gaze vector.
- **Adaptive Smoothing**: Implements a **1 Euro Filter** to suppress saccadic jitter during steady gaze while preserving low latency during rapid eye movements.

---

## Getting Started

### 1. Installation

1. Copy the `addons/godot-gaze/` folder from the `project/` directory into your Godot project's `addons/` folder.
2. In the Godot Editor, go to **Project Settings > Plugins** and enable the `godot-gaze` plugin.

### 2. Basic Setup

1. In your Godot Scene, add a **GazeTracker** node.
2. The tracker automatically attempts to guess the camera offsets, screen resolution, and physical display characteristics. You can also customize these properties on the node in the Inspector if needed:
   - `camera_offset`: Camera mount position relative to the screen center in millimeters.
   - `camera_tilt`: Downtilted angle of the camera in degrees.
   - `screen_size_pixels`: Monitor resolution in pixels.
   - `screen_size_mm`: Physical display dimensions in millimeters.
3. Model weights (`.ort` format) are pre-bundled inside the addon folder.

### 3. Basic GDScript Usage

Attach a script to your scene root node to start receiving tracking data:

```gdscript
extends Node2D

@onready var gaze_tracker: GazeTracker = $GazeTracker
@onready var cursor: Node2D = $Cursor
@onready var status: Label = $Status

func _ready():
    # Connect signals
    gaze_tracker.gaze_updated.connect(_on_gaze_updated)
    gaze_tracker.face_detected.connect(_on_face_detected)

    # Start the tracker
    if gaze_tracker.initialize_tracker():
        print("Gaze Tracker Started.")

func _on_gaze_updated(screen_pixel: Vector2):
    cursor.global_position = screen_pixel

func _on_face_detected(detected: bool):
    status.text = "Face Tracked" if detected else "Face Lost"
```

---

## Developer Guidelines & Compiling

For guidelines on compiling the plugin from source, running tests, or contributing, please refer to [CONTRIBUTING.md](CONTRIBUTING.md).
For detailed design details, refer to the [Architecture Guide](docs/ARCHITECTURE.md).
