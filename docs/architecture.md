# System Architecture & Technical Specification

**Project:** `godot-gaze`  
**Integration:** Godot 4 GDExtension Plugin  
**Reference Application:** `eyecandy`  

---

## 1. Architectural Topology

`godot-gaze` is engineered as a strictly layered 3D gaze-tracking and facial analysis engine. It decouples high-level engine nodes from numerical algorithms, hardware ingestion, and ONNX Runtime neural inference backends.

```mermaid
flowchart TD
    subgraph L4["Layer 4: Godot High-Level Frontends (InputEvent & UI)"]
        IE["InputEventGaze / InputEventGazeMissing<br/>(Native Input Dispatch)"]
        HUD["Debug HUD / Overlay<br/>(debug_cam_feed.tscn)"]
        DP["DisplayProfile<br/>(Screen Geometry & Physical Millimeters)"]
        Cal["BioCalibration / DeviceCalibration<br/>(User-Specific Offset Tuning)"]
    end

    subgraph L3["Layer 3: Godot GDExtension Servers & Platform Backends"]
        GS["GazeServer Singleton<br/>(RID State Ownership & Concurrency)"]
        VS["VisionServer Singleton<br/>(Frame Ingestion & Textures)"]
        JS["Web JavaScriptBridge Sidecar<br/>(Emscripten / ONNX Web)"]
        WIN["Windows WMF Backend<br/>(MultiByte UTF-8 Path Safety)"]
    end

    subgraph L2["Layer 2: Native ML Inference & Pipeline"]
        GTP["GazeTrackingPipeline<br/>(Asynchronous Worker Loop)"]
        YN["ORTYuNetDetector<br/>(Face Detection & Bounding Box)"]
        LM["ORTLandmarkModel<br/>(ADAS 35-Point Landmark Regression)"]
        PNP["SQPnP / LM PnP Solvers<br/>(Non-Linear Head Pose Optimization)"]
        EM["ORTEyeStateModel<br/>(32x32 Open/Closed Classifier)"]
        GM["ORTGazeModel<br/>(ADAS-0002 Gaze Vector Regression)"]
    end

    subgraph L1["Layer 1: Core Math & Utilities (Zero-Dependency C++)"]
        PE["ProjectionEngine & ScreenProjector<br/>(Ray-Plane & Viewport Pixel Math)"]
        MD["MathDefs, GazeBasis3D, Rodrigues, Warper<br/>(POD Math & Image Geometry)"]
        PL["Pool & AtomicMailbox<br/>(Lock-Free Inter-Thread Queues)"]
        GFD["GazeFrameData POD<br/>(Double-Buffered Frame Payloads)"]
    end

    IE --> GS & VS
    GS --> GTP
    GTP --> YN & LM & PNP & EM & GM
    GTP --> GFD & PL
    GS --> PE & MD
```

---

## 2. Layer Isolation & Purity Rules

1. **Layer 1 (Core - `src/core/`):**
   - Must remain **100% zero-dependency pure C++17/20 standard library**.
   - No inclusion of `<godot_cpp/...>` headers, no Godot RIDs, no ONNX Runtime headers, and no platform-specific OS APIs.
   - Responsible for pure geometric math, screen projections, numerical solvers, and thread-safe data containers (`AtomicMailbox`, `Pool`).

2. **Layer 2 (Native ML - `src/native/`):**
   - Wraps ONNX Runtime (`onnxruntime_cxx_api.h`).
   - Implements multi-threaded inference pipelining (`GazeTrackingPipeline`).
   - Does not depend on Godot engine types or GDScript bindings.
   - For detailed neural model specifications, tensor formats, and provenances, see [Vision Models & Camera Ingestion Specification](file:///Users/acunningham/src/godot-gaze/docs/vision_models.md).

3. **Layer 3 (Platforms & Servers - `src/godot/`, `src/windows/`, `src/web/`):**
   - Implements Godot's RID (Resource Identifier) server architecture (`GazeServer`, `VisionServer`).
   - Manages platform camera feeds (Windows Media Foundation, Godot CameraFeed, Web `getUserMedia`).
   - Normalizes hardware capture frames to packed BGR8 working buffers (see [Vision Models & Camera Ingestion Specification](file:///Users/acunningham/src/godot-gaze/docs/vision_models.md#1-camera-ingestion-architecture--color-formats)).
   - Enforces Pimpl (`GazeServerImpl`) encapsulation to insulate Godot bindings from native allocations.

4. **Layer 4 (High-Level Nodes & Events - `src/godot/` & `eyecandy`):**
   - `InputEventGazeBase`, `InputEventGaze`, `InputEventGazeMissing`, and resources (`DisplayProfile`, `DeviceCalibration`, `BioCalibration`).
   - Communicates with backends solely by querying `GazeServer` or receiving input events.

---

## 3. Concurrency & Threading Model

```mermaid
sequenceDiagram
    autonumber
    participant MainThread as Godot Main Thread (GazeServer)
    participant RequestMailbox as AtomicMailbox<GazeFrameData*>
    participant WorkerThread as GazeTrackingPipeline Worker Thread
    participant ResultsMailbox as AtomicMailbox<GazeFrameData*>

    Note over MainThread,WorkerThread: Frame Ingestion & Dispatch
    MainThread->>RequestMailbox: push_frame_request(frame_data)
    WorkerThread->>RequestMailbox: take(data) (Lock-Free)
    Note over WorkerThread: 1. Counter-rotate frame for Roll<br/>2. YuNet Face Detection<br/>3. ADAS 35-pt Landmark Regression<br/>4. Levenberg-Marquardt PnP Head Pose<br/>5. Dense 60x60 Eye Crops<br/>6. Eye Openness & Gaze Estimation
    WorkerThread->>ResultsMailbox: push_result(data) (Lock-Free)
    MainThread->>ResultsMailbox: pop_result(&out_res) (Every _process frame)
    MainThread->>MainThread: Update Spatial RIDs & Screen Projections
```

- **Lock-Free Pipeline Execution:** The background inference worker thread communicates with Godot exclusively via `AtomicMailbox<GazeFrameData*>`. The worker loop never locks `GazeServer::state_mutex`.
- **Main Thread Mutex Safety:** `GazeServer::state_mutex` exists solely to synchronize concurrent or recursive GDScript/C++ calls against internal RID registry dictionaries.

---

## 4. Hardware & Memory Model: Why CPU RAM is the Common Denominator

Our tracking pipeline is a multi-stage **hybrid pipeline**:
$$\text{Camera Frame} \xrightarrow{\text{CPU}} \text{YuNet CNN} \xrightarrow{\text{CPU Transform}} \text{ADAS LM CNN} \xrightarrow{\text{CPU PnP Solver}} \text{ADAS GazeNet CNN} \xrightarrow{\text{Screen Projection}}$$

### Why Zero-Copy GPU Roundtrips Were Rejected
- **Iterative Solvers on CPU:** Head pose estimation requires a non-linear Levenberg-Marquardt / SQPnP optimization solver over 35 $(X, Y)$ landmarks. This is an iterative CPU numerical algorithm.
- **Cross-Platform GDExtension Boundaries:** Passing GPU textures directly into neural execution providers (e.g. DirectML, Metal, WebGPU) without CPU readbacks requires compute shaders for intermediate bounding-box rotations and eye crops across macOS, Windows, Android, and WebAssembly.
- **Contiguous CPU Memory:** Pre-allocating contiguous BGR buffers (`GazeFrameData::camera_raw_bgr`, `GazeFrameData::rotated_frame_bgr`) provides zero-allocation steady-state performance at 60 FPS without GPU $\leftrightarrow$ CPU synchronization pipeline bubbles.

---

## 5. WebAssembly & JavaScript Boundary Architecture

For Godot Web exports (HTML5 / WebAssembly):

```mermaid
flowchart LR
    subgraph BrowserJS["Browser Main Thread (JavaScript)"]
        Cam["getUserMedia<br/>Video Stream"]
        ONNXWeb["onnxruntime-web<br/>(WebGL / WebGPU)"]
    end

    subgraph GodotWASM["Godot C++ GDExtension (WebAssembly)"]
        Bridge["JavaScriptBridge Ingestion"]
        PnP["PnP LM / SQPnP Solvers<br/>(src/core/pnp_solver.cpp)"]
        Proj["Screen Ray Projections<br/>(src/core/projection_engine.cpp)"]
    end

    Cam --> ONNXWeb
    ONNXWeb -- "Raw 2D Landmarks & Crops" --> Bridge
    Bridge --> PnP --> Proj
```

1. **Hardware & ML I/O Bridge (`gaze_sidecar.js`):**
   - Manages browser permissions and video stream capture (`navigator.mediaDevices.getUserMedia`).
   - Executes neural network graphs in browser-optimized WebGL/WebGPU kernels via `onnxruntime-web`.
   - Passes raw bounding boxes and 2D landmark coordinates to Godot via `JavaScriptBridge`.

2. **Unified Mathematical Core (C++ WebAssembly):**
   - 100% of spatial geometry, PnP head pose solving, basis rotations, and screen ray projections are compiled directly from `src/core/` into WebAssembly.
   - Eliminates JavaScript math duplication and guarantees identical behavior between Native desktop and Web builds.
   - WebAssembly execution reduces main-thread CPU time by ~2x–4x compared to pure JavaScript numerical loops.

---

## 6. Input Subsystem & Event Architecture

> [!NOTE]
> **Status: Phase 1 Implemented / Phase 2 (Interaction & Dwell) In Active Development**
> The input event hierarchy (`InputEventGazeBase`, `InputEventGaze`, `InputEventGazeMissing`) and centralized `GazeServer` lifecycle are **implemented** as of Phase 1. The high-level interaction widgets (`PowerAccumulator`, `GazeAreaControl`) are being implemented in Phase 2.

To integrate idiomatic Godot input handling, `godot-gaze` models eye-gaze as first-class engine input events rather than relying strictly on out-of-band polling.

```mermaid
flowchart TD
    GS["GazeServer Singleton"] -->|Constructs 60 FPS Frame Event| IEG["InputEventGaze / InputEventGazeMissing<br/>(inherits InputEventAction)"]
    IEG -->|Input.parse_input_event| INP["Godot Input Pipeline"]
    INP -->|Standard Dispatch| UN["Node._unhandled_input / _input"]
    INP -->|Interaction Layer (Phase 2)| GAC["GazeAreaControl / GazeDetector"]
    GAC -->|Target Power 1.0 / 0.0| PA["PowerAccumulator (2nd-Order Dynamics)"]
    PA -->|charge_changed / full| UI["UI Focus & Button Activation"]
```

### 1. First-Class Events: `InputEventGazeBase`, `InputEventGaze`, `InputEventGazeMissing` (Implemented)
- **Inheritance**: Subclasses `InputEventAction` (concrete in Godot's `ClassDB`), enabling registration and routing through `Input::get_singleton()->parse_input_event()`.
- **`InputEventGazeBase`**:
  - `window_id: int`, `frame_id: int`, `timestamp_usec: int`.
  - Continuous eye openness: `left_eye_openness: float`, `right_eye_openness: float` ($0.0$ closed to $1.0$ open).
  - Blink queries: `is_blink(threshold = 0.5)`, `is_left_blink()`, `is_right_blink()`.
  - `is_face_tracked() -> bool` (virtual, defaults to false on base / missing).
- **`InputEventGaze`**:
  - Dispatched when face detection and gaze estimation succeed.
  - **2D Coordinates**: `position: Vector2` (viewport pixels) and `global_position: Vector2` (window pixels).
  - **Kinematics**: `relative: Vector2`, `velocity: Vector2` ($\Delta\text{pos}/\Delta t$ in px/sec), `screen_velocity: Vector2`.
  - **3D Spatial Transforms**: `head_transform: Transform3D` (origin in mm, orientation basis) and `gaze_transform: Transform3D` (combined optical ray origin and direction basis).
- **`InputEventGazeMissing`**:
  - Dispatched when face or eye tracking drops.
  - Carries `reason: MissingReason` (`REASON_NO_FACE_DETECTED`, `REASON_OCCLUSION_BLINK`, `REASON_OUT_OF_BOUNDS`, `REASON_LOW_CONFIDENCE`).

### 2. Decoupled Interaction & Dwell Layer (Phase 2 - In Progress)
- **`PowerAccumulator`**: A 2nd-order dynamical integrator ($\frac{dv}{dt} = A(P_{\text{target}} - P) - D v, \frac{dq}{dt} = v$) that handles dwell charging and decay with physical momentum. Absorbs natural $100\text{–}150\text{ms}$ micro-saccades and dropouts without discrete pause timers.
- **Unified Infinite Margins**: Hitbox expansion (`margin_left`, `margin_right`, `margin_top`, `margin_bottom`) allows expanding target regions; setting margins to $\infty$ on $X$ or $Y$ creates full-width row or full-height column bands.
- **Focus-Driven UI**: UI targets react to gaze via standard Godot `grab_focus()` rather than hijacking physical mouse hover coordinates.
