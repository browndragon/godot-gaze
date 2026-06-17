# godot-gaze

A high-frequency, real-time 3D gaze estimation and expression tracking GDExtension plugin for Godot 4.7+. 

Optimized for cross-platform games, this library provides precise gaze ray projection onto the screen using configurable display geometries, customizable calibration, and adaptive signal filtering.

---

## Key Features

- **6-Layer Decoupled Architecture**: Modular interfaces (Camera Capture, Image Pipeline, ONNX Model Inference, Screen Projection, Filtering, and Godot wrappers) allow testing and compiling layers independently.
- **Cross-Platform Delivery (Native vs Web Split)**:
  - **Native Desktop/Mobile/iOS**: Uses an OpenCV (`cv::dnn`) pipeline for face detection (YuNet) and gaze estimation.
  - **WebAssembly (WASM)**: Leverages browser-native **ONNX Runtime Web** (utilizing WebGPU/WebGL acceleration) and **Google MediaPipe Face Landmarker JS** to run inference outside WebAssembly, passing payloads back via Godot's Emscripten JS Bridge.
- **3D Projection Math**: Resolves 3D ray-plane intersections accounting for camera tilt, offset from screen center, and display scale.
- **Dual Calibration System**: Provides both a **3D spherical angular correction** (pitch/yaw biases applied directly to the raw gaze vector) and **2D pixel-space shifts** for biological error correction.
- **Adaptive Smoothing**: Implements a **1 Euro Filter** to suppress high-frequency saccadic jitter during slow movements without introducing lag during rapid saccades.

---

## Project Structure

```
godot-gaze/
├── SConstruct                         # SCons compilation file
├── LICENSE.md                         # License agreements
├── README.md                          # Main project guide
├── CONTRIBUTING.md                    # Coding standards and logging rules
├── docs/
│   └── gaze_math_physical_model.md    # Gaze math derivations and physics reference
├── scripts/
│   └── download_opencv.sh             # Dependency installer (macOS/Android/iOS)
├── thirdparty/                        # External dependency directories
│   ├── doctest/                       # C++ unit testing
│   └── one_euro_filter/               # 1 Euro filter code
├── src/
│   ├── core/                          # Pure C++ engine (OpenCV/Godot independent)
│   ├── native/                        # Native OpenCV capture and DNN inference
│   ├── web/                           # WASM browser sidecar bridge setters
│   └── godot/                         # Godot GDExtension node bindings
└── project/                           # Godot Project directory
    └── addons/
        └── godot-gaze/
            ├── gaze.gdextension       # GDExtension library mapping metadata
            └── tests/                 # Integration test scene
```

---

## Getting Started & Build Instructions

Ensure you have Python, SCons, and OpenCV installed (see [CONTRIBUTING.md](CONTRIBUTING.md) for automated setup).

### 1. Build the GDExtension Binary
SCons is used to compile the library. Adjust the `platform` parameter as required:
```bash
# Build for macOS
scons platform=macos target=template_debug

# Build for Windows
scons platform=windows target=template_debug

# Build for WebAssembly (WASM)
scons platform=javascript target=template_debug
```

### 2. Integration inside Godot

1. Copy the `addons/godot-gaze/` folder into your Godot project's `addons/` directory.
2. In your Godot Scene, add a **GazeTracker** node.
3. Configure the physical constants on the node in the Inspector:
   - `camera_offset`: Camera mount position relative to screen center in millimeters (e.g. `(0, -148, 10)` for top-center of a 24-inch screen).
   - `camera_tilt`: Angle in degrees tilted downwards (e.g., `15.0`).
   - `screen_size_pixels`: Native viewport resolution.
   - `screen_size_mm`: Physical display dimensions (e.g. `(527, 296)`).
   - `yunet_model_path` & `gaze_onnx_path`: Paths to your local model weights.

### 3. Basic GDScript Usage

Attach a script to your scene root node:
```gdscript
extends Node2D

@onready var gaze_tracker = $GazeTracker

func _ready():
    # Connect signals
    gaze_tracker.gaze_updated.connect(_on_gaze_updated)
    gaze_tracker.face_detected.connect(_on_face_detected)
    
    # Start tracker
    if gaze_tracker.initialize_tracker():
        print("Gaze Tracker Started.")

func _on_gaze_updated(screen_pixel: Vector2):
    # Move custom cursor node
    $Cursor.global_position = screen_pixel

func _on_face_detected(detected: bool):
    $Status.text = "Face Tracked" if detected else "Face Lost"

func _input(event):
    # Calibrate at current cursor target (e.g., center of screen)
    if event.is_action_pressed("ui_select"):
        gaze_tracker.calibrate_3d(Vector2(960, 540))
```

---

## Web / HTML5 Export Integration

When exporting for Web/WASM:
1. SCons compiles out OpenCV libraries and links a lightweight data bridge.
2. Load `opencv.js` and `onnxruntime-web` in your HTML page shell.
3. Run eye extraction on a canvas and feed crops to ONNX Runtime Web.
4. Pass the calculated 3D gaze vector back into Godot's WebAssembly loop using the JavaScript bridge:
   ```javascript
   // JavaScript sidecar
   const gazeTracker = godot.engine.get_singleton("GazeTracker");
   gazeTracker.feed_gaze_web(originVector3, directionVector3);
   ```
