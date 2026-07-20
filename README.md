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

1. In your Godot Scene, add a **GazeTracker** node.
2. The tracker automatically attempts to guess the camera offsets, screen resolution, and physical display characteristics. You can also customize these properties on the node in the Inspector if needed:
   - `camera_offset`: Camera mount position relative to the screen center in millimeters.
   - `camera_tilt`: Downtilted angle of the camera in degrees.
   - `screen_size_pixels`: Monitor resolution in pixels.
   - `screen_size_mm`: Physical display dimensions in millimeters.
3. Model weights (`.ort` format) are pre-bundled inside the addon's `models/` folder.

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

## Developer Guidelines & Compiling

For guidelines on compiling the plugin from source, running tests, or contributing, please refer to [CONTRIBUTING.md](CONTRIBUTING.md).
For detailed design and math specifications, refer to the [Mathematical & Physical Model Guide](docs/mathematical_model.md) and the [Architecture & Layout section of CONTRIBUTING.md](CONTRIBUTING.md#1-directory-layout--architecture-layers).
