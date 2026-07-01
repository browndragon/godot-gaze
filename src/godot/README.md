/**

- @dir src/godot
- @brief Godot GDExtension Class Bindings
-
- This directory contains the GDExtension classes and ClassDB registration interfaces that bridge the core C++ gaze engine to Godot.
-
- Classes here include:
- - `GazeTracker`: Singleton wrapping the asynchronous pipeline, filters, and display projector.
- - `GazeServer`: Background thread manager.
- - `CameraSensor`: Frame ingestion and camera property wrapper.
- - `FaceEstimator`: YuNet detector bindings.
- - `EyeEstimator`: Gaze estimation model bindings.
- - `DeviceCalibration`/`BioCalibration`: Serializable Godot Resource classes.
- - `GazeCalibrationSession`: Interactive calibration coordinator.
-
- Ensure all public methods here are fully documented with Doxygen comments.
  */

# Godot GDExtension Layer

Binds the underlying C++ layers to the Godot Engine, exposing node types, resources, and methods to GDScript.
