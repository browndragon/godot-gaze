# GazeTracker Project Backlog & TODO

This document tracks the prioritized outstanding tasks, design decisions, and future roadmap items for `godot-gaze`.

---

## 1. Multi-Component Architecture Redesign
* **Objective**: Refactor `GazeTracker` to reduce its responsibility overhead, breaking it down into independent, modular C++ components that communicate sequentially.
* **Architecture Components**:
  - **CameraSensor**: Manages the raw connection to the camera device, providing frame-ready events/callbacks, webcam preview buffers, and snapshots.
  - **PoseEstimator**: Runs face detection (YuNet) on raw frames, selects the primary face, and estimates the 3D head pose.
  - **EyeEstimator**: Crops eyes and runs gaze estimation networks (GazeNet) to calculate direction vectors.
  - **SmoothFilter**: Manages temporal filtering (OneEuroFilter) to stabilize output coordinates.
* **Rationale**: This is a dependency for the visual debug overlay (`debug_feed.tscn`) and must be implemented first to ensure clean component integration.

## 2. Unified Vision Debug Overlay (`debug_feed.tscn`)
* **Objective**: Create a cross-platform HUD to render live webcam frames, face landmarks, and eye crops inside Godot.
* **Design**: Build a unified Godot Control scene (`debug_feed.tscn`). When instantiated, it will subscribe to notifications/signals on the sub-components of the redesigned `GazeTracker` (e.g. `CameraSensor` and `EyeEstimator`) to obtain and render the frames/crops as Godot `ImageTexture` resources.
* **Prerequisite**: Requires completion of the **Multi-Component Architecture Redesign**.

## 3. Proper Gaze Calibration Resource & Scene Persistence
* **Objective**: Decouple the calibration UI/logic from the core C++ tracker and persist calibration state.
* **Design**:
  - The calibration scene guides the user through multi-point (corners, sides, center) alignment and generates a `GazeCalibration` resource.
  - Persist and reload calibration weights and OLS bias offsets using standard Godot `ResourceSaver` and `ResourceLoader` manual serialization.
  - The saved calibration resource is loaded back onto the `GazeTracker` node upon application launch.

## 4. Unify Native Desktop OpenCV Build with OpenCV Submodule
* **Objective**: Replace the Homebrew-linked `thirdparty/opencv-brew` dependency with native C++ libraries compiled directly from our local `thirdparty/opencv` git submodule (matching the Web/OpenCV.js build setup).
* **Benefits**: Ensures 100% dependency self-containment and build reproducibility across developer machines without system-wide package manager dependencies.
