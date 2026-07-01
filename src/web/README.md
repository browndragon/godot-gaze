/**

- @dir src/web
- @brief Web-specific GDExtension bridge
-
- This directory contains the WebAssembly (WASM) compatibility bridge.
-
- When compiling for the Web, SCons stub-out the native C++ ONNX Runtime and replaces them with a lightweight Emscripten JS data bridge.
- - The main thread interacts with the browser sidecar (`gaze_sidecar.js`).
- - The sidecar runs MediaPipe Face Landmarker JS and ONNX Runtime Web inside a dedicated browser Web Worker.
- - Ingested coordinates are fed back to Godot through registered Emscripten callbacks on the `WebBindingState` class.
  */

# Web Platform Layer

Provides the Emscripten binding classes and Web Worker sidecar integration to support high-frequency gaze tracking inside web browsers.
