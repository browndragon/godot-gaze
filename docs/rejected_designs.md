# Rejected Designs & Anti-Patterns

This document records architectural decisions, technical theories, and design patterns that have been tested, rejected, or identified as anti-patterns in `godot-gaze`. Contributors must consider these Frequently Asserted Quibbles when evaluating features or diagnosing issues.

---

## 1. Frequently Asserted Quibbles & Technical Traps

1. **Mac Webcam Feed Mirroring Assumption**:
   * **False Assumption**: Assuming macOS webcam feeds are horizontally mirrored by default and adding manual GDScript or C++ flip logic.
   * **Fact**: Frame orientation is handled consistently upstream by Godot and AVFoundation. Adding manual flips breaks facial landmark alignment and gaze vector projection math.

2. **Custom Classifiers & Synthesized Heuristics**:
   * **False Assumption**: Inventing hardcoded heuristics, state machines, or synthesized dummy algorithms when an ML feature (like blink detection or iris tracking) is needed.
   * **Fact**: Inventing custom heuristics fails in practice. When a model or classifier is required, search upstream open-source alternatives (OpenCV, MediaPipe, OpenVINO, ONNX Model Zoo) for models supported by established literature.

3. **Attributing Head Pose Tracking Errors to Calibration**:
   * **False Assumption**: Claiming 3D head pose translation or rotation errors are caused by missing or uncalibrated screen/bio geometry.
   * **Fact**: Gaze projection vectors may relate to bio or screen calibration, but head pose tracking errors have never been caused by calibration. Head pose errors are always caused by logic bugs (misaligned spatial transformations, bad PnP solve inputs, invented landmark coordinates, or frame offset math).

4. **Zero-Copy GPU Texture Pipeline**:
   * **False Assumption**: Streaming raw camera frames directly on GPU to ONNX Runtime without CPU memory roundtrips.
   * **Fact**: Partial CPU-GPU-CPU synchronization stalls for facial crop extraction negated any compute speedups. Abandoned in favor of the self-contained CPU pipeline (see `TODO.md` Item 8).

5. **DisplayServer Coordinates on HiDPI / Retina Displays**:
   * **False Assumption**: Assuming `DisplayServer.screen_get_size()` or `DisplayServer.window_get_position()` return physical hardware pixels that must be divided by `DisplayServer.screen_get_scale()`.
   * **Fact**: In Godot 4 on macOS, `DisplayServer` coordinates are already expressed in logical screen points/pixels (`lpix`), matching 2D viewport coordinates. Dividing by `scale` halves the screen and window coordinates, causing projection misalignment by a factor of 2. All Godot-facing spatial APIs must consistently use logical pixels (`lpix`).

6. **CameraFeed Formats & ONNX Model Color Spaces**:
   * **False Assumption**: Assuming all camera feeds deliver ready `FORMAT_RGB8` directly, or that all deep learning models take RGB.
   * **Fact**: All four neural models in `godot-gaze` (YuNet, ADAS Landmarker, ADAS Gaze, and Eye Openness) require **BGR** input channel ordering. Camera ingestion normalizes OS frames (`RGBA8`, `RGB8`, `R8`, `NV12`) directly into packed BGR8 working buffers without redundant intermediate color permutations. For full model provenances, tensor layouts, and hardware capture characteristics, see [Vision Models & Camera Ingestion Specification](file:///Users/acunningham/src/godot-gaze/docs/vision_models.md).

---

## 2. Code Style & Integration Guidelines

1. **GDExtension Subclassing & Method Overrides**:
   * When overriding a virtual method in GDScript (e.g., `_process`), only invoke `super(...)` if that virtual method is explicitly defined in the C++ parent class.
   * Calling `super()` on a method NOT defined in the parent C++ class causes a GDScript runtime error. Inspect `src/godot/` C++ class bindings first.

2. **Explicit Coordinate Space Annotations**:
   * Methods transforming spatial coordinates (e.g., camera space, viewport space, inference space, screen space) must explicitly document and annotate coordinate systems at function parameter boundaries.

3. **GDExtension Signal Parameter Types**:
   * Signal signatures exposed from C++ to GDScript should use standard engine primitives (`Vector2`, `Transform2D`, `float`, `int`, `bool`) to avoid binding friction across script boundaries.
