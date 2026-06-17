# Contributing to godot-gaze

Thank you for contributing to the Godot Gaze & Expression Tracking GDExtension plugin! Please adhere to the following guidelines to maintain library quality, testability, and clean code separation.

---

## 1. Development Environment Setup

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

## 2. Directory Layout & Architecture Layers

To ensure complete testability and cross-platform flexibility, the code is separated into strict directories:

* `src/core/`: Zero-dependency, pure C++ math, filters, and layer interfaces. **Must not depend on Godot-cpp or OpenCV.**
* `src/native/`: Native C++ wrappers utilizing OpenCV and cv::dnn.
* `src/web/`: Web/HTML5 stubs interfacing with the browser Emscripten sidecar.
* `src/godot/`: GDExtension bindings and lifecycle orchestrations.
* `tests/`: Standalone unit tests executing core logic without spinning up Godot.
* `thirdparty/`: External vendored packages (e.g. `doctest`, `one_euro_filter`).

---

## 3. Test-Driven Development (TDD) Practice

Any modifications or additions to the mathematical projection formulas, calibration logic, or smoothing filters **must be written in `src/core/` and validated using the C++ unit test suite**.

### Running Standalone C++ Tests
To verify changes instantly without compiling Godot bindings:
```bash
g++ -std=c++17 tests/test_main.cpp src/core/projection_engine.cpp \
    -Isrc/core -Ithirdparty/doctest -Ithirdparty/one_euro_filter -o run_tests
./run_tests
```
Ensure all assertions pass before submitting code changes.

---

## 4. Structured Logging Standards

To maintain ease of diagnostics across different platforms (especially on mobile and headless servers), we enforce key-value structured console output.
* Do **not** use `printf`, `std::cout`, or raw Godot prints inside core code.
* Use the macros defined in `src/core/log.hpp`:
  ```cpp
  #include "log.hpp"
  
  // Format: LOG_INFO("event_name", "key1", val1, "key2", val2)
  Gaze::log_info("CalibrationTriggered", "type", "3D", "target_x", 960, "target_y", 540);
  Gaze::log_error("ModelLoadFailed", "path", model_path, "error_code", -3);
  ```
Structured logs are easy to parse and automatically output in `event_name key1=val1 key2=val2` formats.
