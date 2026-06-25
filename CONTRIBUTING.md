# Contributing to godot-gaze

Thank you for your interest in the Godot Gaze & Expression Tracking GDExtension plugin! Please adhere to the following guidelines to maintain library quality, testability, and clean code separation.

# Project layout & rules

## 1. Directory Layout & Architecture Layers

To ensure complete testability and cross-platform flexibility, the code is separated into strict directories:

- `src/core/`: Zero-dependency, pure C++ math, filters, and layer interfaces. **Must not depend on Godot-cpp or OpenCV.**
- `src/native/`: Native C++ wrappers utilizing OpenCV and cv::dnn.
- `src/web/`: Web/HTML5 stubs interfacing with the browser Emscripten sidecar.
- `src/godot/`: GDExtension bindings and lifecycle orchestrations.
- `tests/`: Standalone unit tests executing core logic without spinning up Godot.
- `thirdparty/`: External vendored packages (e.g. `doctest`, `one_euro_filter`).

This **6-Layer Decoupled Architecture**: Modular interfaces (Camera Capture, Image Pipeline, ONNX Model Inference, Screen Projection, Filtering, and Godot wrappers) allow testing and compiling layers independently.

## 2. Test-Driven Development (TDD) Practice

Any modifications or additions to the mathematical projection formulas, calibration logic, or smoothing filters **must be written in `src/core/` and validated using the C++ unit test suite**.

### Running Standalone C++ Tests

To verify changes instantly without compiling Godot bindings:

```bash
g++ -std=c++17 tests/test_main.cpp src/core/projection_engine.cpp \
    -Isrc/core -Ithirdparty/doctest -Ithirdparty/one_euro_filter -o run_tests
./run_tests
```

Ensure all assertions pass before submitting code changes.

## 3. Structured Logging Standards

To maintain ease of diagnostics across different platforms (especially on mobile and headless servers), we enforce key-value structured console output.

- Do **not** use `printf`, `std::cout`, or raw Godot prints inside core code.
- Use the macros defined in `src/core/log.hpp`:

  ```cpp
  #include "log.hpp"

  // Format: LOG_INFO("event_name", "key1", val1, "key2", val2)
  Gaze::log_info("CalibrationTriggered", "type", "3D", "target_x", 960, "target_y", 540);
  Gaze::log_error("ModelLoadFailed", "path", model_path, "error_code", -3);
  ```

  Structured logs are easy to parse and automatically output in `event_name key1=val1 key2=val2` formats.

---

# Developer workflow

## 1. Bug Reports, feedback, funny memes

Make them on [github issue tracker](https://github.com/browndragon/godot-gaze/issues).

## 2. Development Environment Setup

This project uses `asdf` to manage local compiler toolchains, Python, and SCons.

### Prerequisites (macOS Host)

1. Install **asdf** package manager:
   ```bash
   brew install asdf
   ```
2. Install tools defined in `.tool-versions`:
   ```bash
   asdf plugin add python
   asdf install python
   asdf plugin add scons
   asdf install scons
   asdf plugin add emsdk
   asdf install emsdk
   ```
3. Install **OpenCV** (required for Native compilation):
   ```bash
   brew install opencv
   ```
4. Run the OpenCV downloader helper to symlink the library:
   ```bash
   ./scripts/download_opencv.sh
   ```

---

## 4. Gaze Pipeline Regression Benchmarking

We enforce automated regression testing against 9 real-image cases (`self_*.jpg`) representing various head poses and gaze targets.

### Running Regression Tests
To run both the core unit tests and the native OpenCV integration tests:
```bash
./scripts/run_tests.sh
```

### Benchmarking and Promotion
1. Every run generates a report at `test_artifacts/gaze_benchmark_report.md`.
2. The report is verified against the baseline in `test_assets/gaze_benchmark_report.md`.
3. If errors regress relative to the baseline (beyond a small tolerance), the test suite will fail.
4. If a change is correct and accepted, promote the new benchmark to update the baseline:
   ```bash
   cp test_artifacts/gaze_benchmark_report.md test_assets/gaze_benchmark_report.md
   ```

## 5. Web/JavaScript Sidecar Integration Testing

We also enforce integration testing of the browser-side script injection loader using a headless Chrome environment.

To run the JavaScript sidecar integration tests:
```bash
node tools/run_sidecar_tests.js
```

## 6. Headless CLI Godot Commands

We provide a `./scripts/godot.sh` helper to run Godot from the command line regardless of host OS. On macOS, this automatically invokes the installed `/Applications/Godot.app` binary.

To export the web build headlessly using this script:
```bash
./scripts/godot.sh --headless --export-debug "Web" project/exports/godot-gaze.html
```

