# GazeTracker Project Backlog & TODO

This document tracks the prioritized outstanding tasks, design decisions, and future roadmap items for `godot-gaze`.

---

## 1. Multi-Component Architecture Redesign [DONE]

- **Objective**: Refactor `GazeTracker` to reduce its responsibility overhead, breaking it down into independent, modular C++ components that communicate sequentially.
- **Architecture Components**:
  - **CameraSensor**: Manages the raw connection to the camera device, providing frame-ready events/callbacks, webcam preview buffers, and snapshots.
  - **FaceEstimator**: Runs face detection (YuNet) on raw frames, selects the primary face, and estimates the 3D head pose.
  - **EyeEstimator**: Crops eyes and runs gaze estimation networks (GazeNet) to calculate direction vectors.
  - **Smoother**: Manages temporal filtering (OneEuroFilter) to stabilize output coordinates.
- **Status**: Completed. Components are decoupled, communicating via `GazeServer` RID events and hierarchal connections.

## 2. Unified Vision Debug Overlay (`debug_feed.tscn`) [DONE]

- **Objective**: Create a cross-platform HUD to render live webcam frames, face landmarks, and eye crops inside Godot.
- **Design**: Build a unified Godot Control scene (`debug_feed.tscn`). When instantiated, it will subscribe to notifications/signals on the sub-components of the redesigned `GazeTracker` (e.g. `CameraSensor` and `EyeEstimator`) to obtain and render the frames/crops as Godot `ImageTexture` resources.

## 3. Proper Gaze Calibration Resource & Scene Persistence

- **Objective**: Decouple the calibration UI/logic from the core C++ tracker and persist calibration state.
- **Design**:
  - The calibration scene guides the user through multi-point (corners, sides, center) alignment and generates a `GazeCalibration` resource.
  - Persist and reload calibration weights and OLS bias offsets using standard Godot `ResourceSaver` and `ResourceLoader` manual serialization to a `user://` path
  - The saved calibration resource is loaded back onto the `GazeTracker` node on application launch.
  - This behavior should be the null-default (if no calibration is specified, the user path is loaded), the default path should be a project setting, and should be hot-swappable (if a new calibration is produced it can be slotted in).
  - Stacked/Replaced calibrations? (if a game does a 1-point center-point recalibration or a 5-point corner-recalibration, we make it easy for them to slot those "on the fly" and get updated data going forward?)
  - Remember, there is a distinction between a **screen** calibration (used to learn the actual screen density/geometry) and the **bio** calibration (used to understand the user's actual personal gaze adjustment from the system calibration, based on depth of eyes or strength between the eyes).

## 4. Native Dependency Isolation & OpenCV Removal [DONE]

- **Objective**: Remove all OpenCV dependencies and compile native binaries with lightweight ONNX Runtime libraries.
- **Benefits**: Reduced packaged footprint size by over 90 MB, eliminated external brew/SDK requirements, and ensured 100% build self-containment.

## 5. Standardize Gaze Model Format [DONE]

- **Objective**: Standardize both the Native and Web Gaze Estimation pipelines on the unified `.ort` model format.
- **Details**: Unified the pipelines to load `.ort` models under custom-optimized minimal ONNX Runtime configurations for both native desktop/mobile platforms and browser sidecars.

## 6. Model Search Paths and Configurable Options [DONE]

- **Objective**: Introduce a configurable `gaze/models/search_paths` Project Setting to allow developers to customize where the plugin searches for model weights (e.g. `res://models` or `res://addons/godot-gaze/models`).
- **Status**: Completed. Path resolution utility `resolve_model_path()` handles lookup order and resolves filenames sequentially.

## 7. CI/CD Cross-Platform Compilation Pipeline [DONE]

- **Objective**: Configure a GitHub Actions build workflow using runner matrices (macOS, Windows, Linux) to compile native GDExtension binaries for all platforms on target triggers and bundle them into export packages.
- **Status**: Completed. Workflow `.github/workflows/build.yml` compiles and bundles all binaries automatically.

## 8. Zero-Copy GPU Metal/DirectX/Vulkan Texture Pipeline [REVERTED/ABANDONED]

- **Objective**: Stream raw camera frames and eye crops entirely in VRAM without CPU-to-GPU memory roundtrips.
- **Platform Support Roadmap**:
  - **macOS**: Directly ingests Godot's `CameraTexture` `RID`, extracts the `id<MTLTexture>` pointer, performs resizing and eye warping via compute shaders on the GPU, and binds them to the ONNX Runtime session using native Metal texture bindings (`initWithMTLTexture`).
  - **Windows**: Partially completed (Direct3D12/DirectML bindings exist). A future roadmap item will complete the integration of Direct3D texture extraction and DirectML bindings to match the macOS zero-copy efficiency.
  - **Mobile/Linux/Android/iOS**: Direct GPU bindings are currently unimplemented (Roadmap / future iterations).
- **CPU Fallback Path**: Fully supported. If a platform or device does not yet support zero-copy GPU texture binding, the pipeline automatically falls back to CPU-based warping and CPU-based ONNX Runtime inference.
- **Project Settings**:
  - Developers can force the pipeline to use the CPU path by checking the project setting `gaze/config/force_cpu`.
- **Renderer Compatibility**:
  - **Forward+ / Mobile**: Compute shaders and native texture handles are supported. The GPU zero-copy path executes successfully if supported by the OS and graphics driver.
  - **Compatibility (OpenGL / WebGL)**: Compute shaders are not supported in Godot's Compatibility renderer. The plugin automatically and gracefully falls back to the CPU pipeline.
- **Status:** abandoned. We couldn't get pure-GPU eyecrops working, so GPU<->CPU<->GPU round trips on every frame to get face detect->eyecrop->gaze detect was too messy for too little benefit. Dropped for now until we have a design that can actually achieve and lock in the gains a pure GPU solution promises without thread stalls.

## X. Test Automation Backlog

- **Android Emulator Permission Automation**: Script UI Automator or use ADB (`pm grant`) to pre-grant camera permissions automatically during instrumentation runs.
- **C++ State Machine Unit Tests**: Unit test permission states and callback triggers using a mock `OS` and `JavaScriptBridge` interface wrapper layer in C++.

## X. Full multiplat strategy

- **Android implementation/testing**
- **IOS implementation/testing**
- **Linux implementation/testing**
- **Windows implementation/testing**
- **Macos implementation/testing** status: **done** (this device!)
- **web implementation/testing** status: **done** (this device!)

## 9. Simplify Backend Directory Layouts

- **Objective**: Consolidate target-specific platform backends (e.g. moving target-specific backends out of custom sub-folders and putting them directly in the native source folder with platform suffixes, e.g. `*.windows.cpp`, `*.macos.cpp`).
- **Status**: Backlog. Tracked for future platform organization refactors.

## 10. Audience-Specific Documentation & Testing Roadmap

- **Objective**: Develop structured documentation and robust test suites specifically targeted at the two primary audiences of the project: GDScript game developers and native C++ engine/plugin contributors.
- **Tasks for GDScript Users**:
  - **GDScript API Reference & Guides**: Write comprehensive XML class reference pages integrated with Godot's built-in editor documentation. Provide tutorials on utilizing the `GazeTracker` Node3D scene node, handling tracking signals, and customizing the pipeline configuration at runtime using `GazePipelineConfig` resources.
  - **GDScript Integration Demo Project**: Build a small, clean template project demonstrating 3D viewport coordinate mapping, custom UI calibration overlays, and basic gaze-driven gameplay mechanics.
  - **GDScript Integration Tests**: Implement E2E and integration tests using Godot's GDScript test integration frameworks (like GUT - Godot Unit Test) to verify the behavior of tracker nodes, calibration resources, and signals directly from scripts.
- **Tasks for C++ Contributors**:
  - **C++ Contribution & Compilation Guide**: Document the project build process, detailing platform dependencies, SCons compilation flags, GDExtension binding mechanisms, and cross-compilation setup.
  - **Native ML Pipeline Integration Guide**: Provide step-by-step instructions on updating or replacing neural models (`.ort` format) and writing platform-agnostic wraps/inference loops within `src/native/` (e.g. updating input/output shapes or sign conventions).
  - **Comprehensive Native Unit Testing**: Expand standard unit tests (using the pure C++ `doctest` suite) to cover state machines, calibration estimators, and projection math. Introduce mock interfaces for platform-dependent behaviors (e.g. mocking `OS` or browser/JS bridges).
- **Status**: Backlog. Priority roadmap items for community/ecosystem enablement.

