# Godot Project Folder

This directory contains the Godot editor project, target resources, and the plugin addon itself.

## Directory Structure

- `addons/godot-gaze/`: The plugin addon directory that gets distributed to users.
  - `models/`: Pre-bundled ONNX Runtime `.ort` model weights.
  - `gaze.gdextension`: GDExtension configuration and library mapping metadata.
  - `tests/`: Integration test scenes (such as `calibration.tscn`).
- `exports/`: Destination folder for exported builds (macOS, Android, Web).
