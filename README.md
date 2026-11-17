# Godot Gaze (`v0.1.0`)

Real-time, on-device eye-gaze and 6-DoF head-pose tracking GDExtension for **Godot 4**, powered by **ONNX Runtime** and physical camera-to-display ray projection.

---

## Key Features

- **On-Device ML Pipeline (No External Services)**: Runs face detection (`face_detection_yunet_2023mar.ort`), 35-point facial landmarks (`facial-landmarks-35-adas-0002.ort`), 3D gaze estimation (`gaze-estimation-adas-0002.ort`), and eye-openness classification (`open_closed_eye.ort`) locally via ONNX Runtime.
- **Physical Camera-to-Display Geometry**: Solves 3D head pose via EPnP in millimeter camera space and intersects the fused gaze ray with the physical screen plane, converting automatically to Godot canvas and `CanvasItem`-local coordinates across window sizes and DPI scales.
- **First-Class Godot Input Events**: Dispatches `InputEventGaze` and `InputEventGazeMissing` through Godot's standard `Input` pipeline.
- **Reference-Counted Lifecycle (`GazeTracker`)**: Automatically starts/stops camera capture and inference when nodes enter or leave the scene tree.
- **Inertial Dwell Node (`FillAccumulator`)**: 2nd-order smooth dwell/charge accumulator with blink/micro-saccade momentum tolerance and distance-scaled off-target drain.
- **Mouse & Gaze Coexistence**: Built-in `MouseStillnessArbitrator` and bi-directional mouse emulation (`emulate_mouse_from_gaze`, `emulate_gaze_from_mouse`) so physical mouse input takes instant priority and mouse-only dev machines can test gaze UIs without a webcam.
- **Calibration & Visual Debugger**: Interactive `GazeCalibrationWizard`, persistent `GazeDeviceProfile`, and live `debug_cam_feed.tscn` camera overlay.
- **Headless CI Testing (`MockVisionServer`)**: Deterministic synthetic camera/gaze driver for automated testing without hardware webcams.
- **Cross-Platform**: Windows, macOS, Linux, Android, iOS, and Web (WebAssembly + `gaze_sidecar.js`).

---

## Installation

### Option 1: Prebuilt Release (`godot-gaze.zip`)

1. Download `godot-gaze.zip` from the [GitHub Releases](../../releases) page.
2. Extract it into the root of your Godot project so that `addons/godot-gaze/` sits inside your `res://` directory (`res://addons/godot-gaze/gaze.gdextension`).
3. Open your project in Godot 4, go to **Project -> Project Settings -> Plugins**, and enable **Godot Gaze**.

### Option 2: Building from Source (or Symlinking `project/addons/godot-gaze`)

1. Clone the repository with submodules (`thirdparty/godot-cpp`, `thirdparty/onnxruntime`):
   ```bash
   git clone --recursive https://github.com/browndragon/godot-gaze.git
   cd godot-gaze
   ```
2. Download and prepare the `.ort` neural models into `project/addons/godot-gaze/models/`:
   ```bash
   ./scripts/download_models.sh
   ```
3. Build the GDExtension for your platform (`macos`, `linux`, `windows`, `android`, `ios`, or `web`):
   ```bash
   scons platform=macos target=editor
   scons platform=macos target=template_debug
   scons platform=macos target=template_release
   ```
   *(Optional)* Regenerate Doxygen + Godot Editor Class Reference XML files (`project/docs/classref/*.xml`, loaded automatically in the editor from `res://docs/classref`):
   ```bash
   ./scripts/generate_docs.sh
   ```
4. Symlink or copy `project/addons/godot-gaze` into `<your-godot-project>/addons/godot-gaze` and enable **Godot Gaze** in **Project Settings -> Plugins**.

---

## Quick Start

### 1. Lifecycle Management with `GazeTracker`

By default, `gaze/general/autostart` is `true`, so tracking starts automatically on startup. For explicit scene-scoped tracking (or if you disable `autostart`), attach a `GazeTracker` node or call `GazeTracker.track_node(self)`. When all active `GazeTracker` nodes have `enabled = false` or exit the tree, `GazeServer` releases the camera automatically.

```gdscript
extends Control

func _ready() -> void:
    # Attach a reference-counted GazeTracker to this node's lifetime
    var tracker := GazeTracker.track_node(self)
    print("Tracking active: ", tracker.is_tracking_active())
    print("Build: ", GazeServer.get_build_info()) # e.g. "godot-gaze v0.1.0"
```

### 2. Handling `InputEventGaze` and `InputEventGazeMissing`

`InputEventGaze` and `InputEventGazeMissing` both inherit `InputEventGazeBase` (`InputEventAction`), so they flow through standard `_input(event)` / `_unhandled_input(event)` callbacks.

> **Important:** Uninitialized or invalid gaze positions are represented as `Vector2(NAN, NAN)`, and clamping preserves non-finite values rather than collapsing them to `(0, 0)`. Always check `pos.is_finite()` before consuming gaze coordinates.

```gdscript
extends Control

@onready var reticle: Control = $Reticle

func _input(event: InputEvent) -> void:
    if event is InputEventGaze:
        # 2D canvas coordinates (or pass `self` for coordinates local to this Control)
        var gaze_pos: Vector2 = event.get_eye_gaze(self, InputEventGaze.CLAMPING_DEFAULT)
        if gaze_pos.is_finite():
            reticle.position = gaze_pos
            reticle.visible = true

        # Optional head pose projection & continuous eye openness [0.0 closed .. 1.0 open]
        var nose_pos: Vector2 = event.get_nose_gaze(self)
        var left_open: float = event.left_eye_openness
        var right_open: float = event.right_eye_openness

        # 3D transforms in camera-space millimeters
        var _head_xform: Transform3D = event.head_transform
        var _gaze_xform: Transform3D = event.gaze_transform

    elif event is InputEventGazeMissing:
        reticle.visible = false
        match event.reason:
            InputEventGazeMissing.REASON_NO_FACE_DETECTED:
                pass # No face in camera frame
            InputEventGazeMissing.REASON_OCCLUDED:
                pass # Eyes occluded / blink
            InputEventGazeMissing.REASON_OUT_OF_BOUNDS:
                pass # Gaze fell outside display bounds
            InputEventGazeMissing.REASON_SENSOR_INACTIVE:
                pass # Camera/pipeline inactive
```

`InputEventGaze.ClampingMode` options for `get_eye_gaze(local_to, clamping)` and `get_nose_gaze(local_to, clamping)`:
- `InputEventGaze.CLAMPING_FREE` (`0`): Unclamped viewport/canvas coordinates (may fall outside the window).
- `InputEventGaze.CLAMPING_CLAMPED` (`1`): Clamped to viewport bounds.
- `InputEventGaze.CLAMPING_DEFAULT` (`2`): Uses the `gaze/pointing/default_clamping` project setting (default `true`).

### 3. Inertial Dwell Interactions (`FillAccumulator`)

`FillAccumulator` provides a 2nd-order damped charge/drain state machine for gaze buttons and dwell targets without harsh resets during blinks or micro-saccades:

```gdscript
extends Control

var dwell: FillAccumulator
var reticle_canvas_pos := Vector2(NAN, NAN)

func _ready() -> void:
    dwell = FillAccumulator.new()
    dwell.fill_sec = 0.8
    dwell.drain_sec = 0.4
    dwell.error_doubling_distance = 100.0
    dwell.process_callback = FillAccumulator.PROCESS_IDLE
    dwell.filled.connect(_on_dwell_filled)
    add_child(dwell)

func _input(event: InputEvent) -> void:
    if event is InputEventGaze:
        var pos: Vector2 = event.get_eye_gaze()
        if pos.is_finite():
            reticle_canvas_pos = pos

func _process(_delta: float) -> void:
    if not reticle_canvas_pos.is_finite():
        dwell.target_error = 200.0
        return
    # Signed distance outside this Control's global rect (<= 0 charges, > 0 drains)
    var rect := get_global_rect()
    var clamped := reticle_canvas_pos.clamp(rect.position, rect.end)
    dwell.target_error = reticle_canvas_pos.distance_to(clamped)
    queue_redraw()

func _on_dwell_filled() -> void:
    print("Activated by gaze dwell!")
    dwell.reset()
```

You can also step manually with `dwell.process_callback = FillAccumulator.PROCESS_MANUAL` and call `dwell.process_error_delta(error_px, delta)` or `dwell.process_dwell_delta(is_active, delta)`. Signals emitted: `fill_changed(fill: float)`, `fill_started()`, `filled()`, `emptied()`.

---

## Mouse & Gaze Coexistence

- **Gaze -> Mouse (`gaze/pointing/emulate_mouse_from_gaze`, default `true`)**: Drives Godot's pointer position from gaze when the physical mouse is idle, with optional dwell clicks (`gaze/pointing/mouse_emulation_dwell_sec`, default `1.5`s). Moving the physical mouse outside the anchor bubble (`gaze/pointing/mouse_stillness_threshold_px`, default `3.0` px) immediately returns pointer authority to the mouse; remaining still for `gaze/pointing/mouse_stillness_duration_sec` (`1.5`s) blends control back to gaze over `gaze/pointing/mouse_emulation_transition_sec` (`0.3`s).
- **Mouse -> Gaze (`gaze/pointing/emulate_gaze_from_mouse`, default `true`)**: When no face/webcam feed is tracking, mouse movement synthesizes `InputEventGaze` events so you can develop and test gaze interactions with a regular mouse. Inspect runtime authority with `GazeServer.is_physical_mouse_still()`, `GazeServer.is_physical_mouse_active()`, and `GazeServer.is_mouse_emulated()`.

---

## Customizing Events (`GazeEventFactory`)

To attach domain metadata or return a custom subclass of `InputEventGaze` / `InputEventGazeMissing`, extend `GazeEventFactory` and override `create_gaze_event()` / `create_missing_event(reason)`:

```gdscript
extends GazeEventFactory

class AppGazeEvent extends InputEventGaze:
    var tag: String = ""

func create_gaze_event() -> InputEventGazeBase:
    var base := GazeServer.create_default_event()
    var ev := AppGazeEvent.new()
    ev.copy_from(base)
    ev.tag = "gameplay"
    return ev

func create_missing_event(reason: int) -> InputEventGazeBase:
    return GazeServer.create_default_missing_event(reason)
```

Register your factory resource via **Project Settings -> `gaze/events/event_factory_path`** or dynamically via `GazeServer.set_event_factory(my_factory)`.

---

## Device Calibration & Visual Debugging

### Device Profiles & Interactive Calibration Wizard

Accurate projection relies on physical display pitch, resolution, and camera offset/tilt represented by `GazeDeviceProfile` (default storage path: `user://calibrations/device_profile.cfg` via `gaze/calibration/device_profile_path`).

- **Built-in Wizard Scene**: Instantiate `res://addons/godot-gaze/calibration/gaze_calibration_wizard.tscn` (`GazeCalibrationWizard`), connect `wizard_completed(profile)` / `wizard_canceled()`, and call `start_wizard()`.
- **Programmatic Configuration**:
  ```gdscript
  var profile := GazeDeviceProfile.create_system_guess()
  profile.camera_offset_mm = Vector3(0.0, 12.0, 0.0)
  profile.camera_tilt_deg = -4.0
  GazeServer.set_device_profile(profile)
  profile.save_to_file("user://calibrations/device_profile.cfg")
  ```

### Camera & Landmark Overlay (`debug_cam_feed.tscn`)

Instance `res://addons/godot-gaze/debug_cam_feed.tscn` (`DebugCamFeed`, configured by `gaze/debug/overlay_scene_path`) in your scene to visualize the live BGR camera preview, face bounding box, 35 facial landmarks, eye crops, and stage timings (`GazeServer.get_pipeline_stage_timings()`).

---

## Headless Testing & CI (`MockVisionServer`)

The vision driver automatically switches to `MockVisionServer` when any of the following is true:
- Godot is running headless (`DisplayServer` name is `"headless"`), e.g., `--headless` unit tests.
- `--mock-camera` or `--mock-vision` is passed on the command line.
- Environment variable `GAZE_VISION_DRIVER=mock` is set.
- Project setting `gaze/vision/driver` is set to `"mock"`.

In tests, you can inject synthetic tracking state directly into `GazeServer`:

```gdscript
GazeServer.set_face_pose(Vector3(0.0, 0.0, 500.0), Vector3.ZERO, true)
GazeServer.set_gaze(Vector3(0.0, 0.0, 500.0), Vector3(0.0, 0.0, -1.0))
GazeServer.set_eye_openness(0.95, 0.95)
GazeServer.trigger_process()
```

---

## Project Settings Reference

All settings are registered under the `gaze/` namespace in **Project -> Project Settings** (`Advanced Settings` enabled):

| Setting Key | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `gaze/general/autostart` | `bool` | `true` | Automatically start gaze tracking when the engine boots. |
| `gaze/pointing/emulate_gaze_from_mouse` | `bool` | `true` | Synthesize `InputEventGaze` from mouse events when tracking is inactive/missing. |
| `gaze/pointing/emulate_mouse_from_gaze` | `bool` | `true` | Drive Godot's mouse pointer from gaze when the physical mouse is still. |
| `gaze/pointing/default_clamping` | `bool` | `true` | Default behavior for `InputEventGaze.CLAMPING_DEFAULT`. |
| `gaze/pointing/mouse_stillness_duration_sec` | `float` | `1.5` | Seconds the physical mouse must stay still before gaze regains pointer control. |
| `gaze/pointing/mouse_stillness_threshold_px` | `float` | `3.0` | Anchor bubble radius (px) above which physical mouse motion claims control. |
| `gaze/pointing/mouse_emulation_dwell_sec` | `float` | `1.5` | Dwell duration (s) for synthesized gaze-to-mouse clicks. |
| `gaze/pointing/mouse_emulation_transition_sec` | `float` | `0.3` | Blend duration (s) when returning pointer authority to gaze. |
| `gaze/models/search_paths` | `String` | `"res://models,res://addons/godot-gaze/models"` | Comma-separated model search directories. |
| `gaze/models/face_detector_prefix` | `String` | `"face_detection_yunet_2023mar"` | Filename prefix for face detector model. |
| `gaze/models/yunet_prefix` | `String` | `"face_detection_yunet_2023mar"` | Legacy YuNet prefix key (mirrors `face_detector_prefix`). |
| `gaze/models/landmarks_prefix` | `String` | `"facial-landmarks-35-adas-0002"` | Filename prefix for 35-point facial landmarker model. |
| `gaze/models/gaze_prefix` | `String` | `"gaze-estimation-adas-0002"` | Filename prefix for gaze estimation model. |
| `gaze/models/eye_openness_prefix` | `String` | `"open_closed_eye"` | Filename prefix for eye openness classifier model. |
| `gaze/models/acceptable_suffixes` | `PackedStringArray` | `[".ort", ".xml"]` | Allowed model file extensions during resolution. |
| `gaze/config/pitch_t_gain` | `float` | `0.0` | Head-pitch compensation gain for gaze estimation. |
| `gaze/config/yaw_t_gain` | `float` | `0.0` | Head-yaw compensation gain for gaze estimation. |
| `gaze/config/nose_y` | `float` | `-0.5` | Nose anchor Y ratio for head-pose estimation. |
| `gaze/config/nose_z` | `float` | `-52.0` | Nose anchor Z offset (mm). |
| `gaze/config/ipd_mm` | `float` | `63.0` | Default interpupillary distance (mm). |
| `gaze/config/debug_image_throttle_interval` | `int` | `1` | Frame throttle interval for debug preview texture updates. |
| `gaze/config/desired_camera_width` | `int` | `640` | Requested hardware capture width (px). |
| `gaze/config/desired_camera_height` | `int` | `480` | Requested hardware capture height (px). |
| `gaze/config/force_cpu` | `bool` | `false` | Force CPU ONNX Runtime execution provider instead of CoreML/NNAPI/DirectML. |
| `gaze/calibration/device_profile_path` | `String` | `"user://calibrations/device_profile.cfg"` | Default file path for saved `GazeDeviceProfile` resources. |
| `gaze/debug/overlay_scene_path` | `String` | `"res://addons/godot-gaze/debug_cam_feed.tscn"` | Scene path used by the debug camera overlay. |
| `gaze/blink/openness_threshold` | `float` | `0.50` | Openness probability threshold (`[0.0, 1.0]`). |
| `gaze/blink/allow_single_eye` | `bool` | `true` | Allow single-eye winks when classifying blink state. |
| `gaze/blink/min_duration` | `float` | `0.05` | Minimum blink duration (seconds). |
| `gaze/debug/verbosity` | `int` | `1` | Logging verbosity level. |
| `gaze/events/event_factory_path` | `String` | `""` | Optional path to a custom `GazeEventFactory` `.tres`/`.gd` resource. |
| `gaze/vision/driver` | `String` | `"native"` | Vision backend (`"native"` or `"mock"`). |

---

## Web Export Guidelines

When exporting a project to **Web (HTML5 / WebAssembly)**:

1. **Enable the Godot Gaze Editor Plugin**: Activating the plugin registers `GodotGazeExportPlugin`, which automatically copies `res://addons/godot-gaze/bin/gaze_sidecar.js` into your export output folder alongside the HTML/JS bundle.
2. **Thread Support & Cross-Origin Isolation Headers**:
   - If you enable **Thread Support** in your Godot Web export preset, browsers require `SharedArrayBuffer` cross-origin isolation. Your web server **must** send:
     ```http
     Cross-Origin-Opener-Policy: same-origin
     Cross-Origin-Embedder-Policy: require-corp
     ```
   - If your static host does not let you configure custom HTTP headers, **disable Thread Support** in the Web export preset so Godot uses the single-threaded `.wasm` GDExtension binary.
3. **Web Model Assets**: Web exports initialize `gaze_sidecar.js` (`onnxruntime-web`) against `res://models/face_detection_yunet_2023mar.onnx` and `res://models/gaze-estimation-adas-0002.onnx`. Ensure those non-resource model files are included in your Web export preset's non-resource file filter (`*.onnx, *.ort`).

---

## Documentation & Deep Dives

- [Architecture Overview](docs/architecture.md)
- [Mathematical Model & Camera-to-Display Projection](docs/mathematical_model.md)
- [Vision Models & Camera Ingestion Specification](docs/vision_models.md)
- [Lifecycle & Reference-Counted Tracking](docs/lifecycle_tracking.md)
- [Gaze Model Evaluation](docs/gaze_model_evaluation.md)
- [Rejected Designs & Anti-Patterns](docs/rejected_designs.md)
- [Contributing & Test Suite Guide](CONTRIBUTING.md)
- [License & Third-Party Notices](LICENSE.md)
