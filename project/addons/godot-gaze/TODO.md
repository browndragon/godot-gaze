# GazeTracker Addon Backlog & TODO

This document tracks Godot-addon-specific backlog items, design decisions, and testing automation. 

For the primary project backlog, see the root [TODO.md](file:///Users/acunningham/src/godot-gaze/TODO.md).

---

## 1. Multi-Component Architecture Redesign
* **Priority**: High (Prerequisite for Debug Overlay).
* **Objective**: Refactor `GazeTracker` into distinct sub-components (`CameraSensor`, `PoseEstimator`, `EyeEstimator`, `SmoothFilter`).

## 2. Unified Vision Debug Overlay (`debug_feed.tscn`)
* **Priority**: Medium.
* **Objective**: Create a cross-platform HUD Control scene that renders live webcam feeds, face landmarks, and eye crops inside Godot using `ImageTexture` resources from the redesigned `GazeTracker` components.

## 3. Calibration State Serialization
* **Objective**: Automatically load and save calibration weights and OLS bias offsets. We will use standard manual `ResourceSaver` and `ResourceLoader` calls on the `GazeCalibration` resource to persist data to disk and reload it onto `GazeTracker` at startup.

## 4. Test Automation Backlog
* **Android Emulator Permission Automation**: Script UI Automator or use ADB (`pm grant`) to pre-grant camera permissions automatically during instrumentation runs.
* **C++ State Machine Unit Tests**: Unit test permission states and callback triggers using a mock `OS` and `JavaScriptBridge` interface wrapper layer in C++.
