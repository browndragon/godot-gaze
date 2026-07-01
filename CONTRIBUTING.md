# Contributing to godot-gaze

Thank you for your interest in the Godot Gaze & Expression Tracking GDExtension plugin!
Please adhere to the following guidelines to maintain library quality, testability, and clean code separation.

---

# Project Layout & Rules

## 1. Directory Layout & Architecture Layers

The codebase is organized as a layered architecture to ensure complete testability and platform separation:

- `src/core/`: Zero-dependency, pure C++ structures, math, filters, and projection algorithms. **Must not depend on Godot-cpp or ONNX Runtime.**
- `src/native/`: Platform-independent ML inference pipelines utilizing ONNX Runtime (CPU/XNNPACK).
- `src/windows/`: Windows-specific C++ platform implementations (e.g., path mapping/conversions).
- `src/android/`: Android-specific C++ platform implementations (e.g., NNAPI execution provider setup).
- `src/web/`: Web/HTML5 stubs and stubs interfacing with the browser Emscripten sidecar.
- `src/godot/`: GDExtension bindings and lifecycle orchestrations.
- `project/`: The Godot project containing the editor addon (`addons/godot-gaze/`).
- `tests/`: Standalone C++ and python unit tests, regression benchmarks, and Godot headless/windowed integration tests.
- `docs/`: Physical/calibration mathematical guides and documentation.
- `thirdparty/`: External vendored packages (e.g. `doctest`, `one_euro_filter`, `godot-cpp`).

---

## 2. Coding Style & Safety Standards

To maintain a clean and safe C++ codebase, we enforce the following rules:
- **No Mutable Reference Parameters for Output**: Output parameters must be passed as pointers (e.g., `T*`) rather than mutable references (e.g., `T&`). Read-only inputs should be passed as const references (e.g., `const T&`).
- **Memory Safety**: Prefer smart pointers (`std::unique_ptr`, `std::shared_ptr`) over raw resource allocations.
- **Layer Isolation**: Ensure that core classes (`src/core/`) never reference GDExtension or ONNX headers directly.

---

## 3. Test-Driven Development (TDD) Practice

Any modifications or additions to the mathematical projection formulas, calibration logic, or smoothing filters must be written in `src/core/` and validated using the unit test suite.

Our testing strategy follows these core principles:
1. **Test-First Diagnostics**: When diagnosing an issue, first duplicate the issue with a failing test before writing any solution. If the issue cannot be reproduced with a test, reconsider your theory of action.
2. **Test Real Code**: Tests should invoke the actual production code under test, rather than mocking or duplicating the production environment.
3. **Justify Baseline Updates**: When updating the regression test benchmark report, you must provide a detailed justification in your commit message explaining why the changes are mathematically correct and overall beneficial.

---

## 4. Structured Logging Standards

To maintain ease of diagnostics across different platforms (especially on mobile and headless servers), we enforce key-value structured console output.

- Do **not** use `printf`, `std::cout`, or raw Godot prints inside core code.
- Use the macros defined in `src/core/log.hpp`:

  ```cpp
  #include "log.hpp"

  // Format: LOG_INFO("event_name", "key1", val1, "key2", val2)
  Gaze::log_info("CalibrationTriggered", "type", "3D", "target_x", 960, "target_y", 540);
  Gaze::log_error("ModelLoadFailed", "path", model_path, "error_code", -3);
  ```

---

# Developer Workflow

## 1. Development Environment Setup

This project uses `asdf` to manage local compiler toolchains, Python, and SCons.

### Prerequisites (macOS Host)

1. Install **asdf** package manager:
   ```bash
   brew install asdf
   ```
2. Install tools defined in `.tool-versions`:
   ```bash
   asdf install python
   asdf install scons
   asdf install emsdk
   ```
3. Install **Node.js** and **npm** (required for Web testing):
   ```bash
   brew install node
   ```
4. Install **Doxygen** (required for API documentation generation):
   ```bash
   brew install doxygen
   ```

---

## 2. Compiling the GDExtension Binary

SCons is used to compile the GDExtension libraries for different target platforms:

```bash
# Compile for macOS
scons platform=macos target=template_debug

# Compile for Windows
scons platform=windows target=template_debug

# Compile for WebAssembly
scons platform=web target=template_debug

# Compile for Android (ARM64)
scons platform=android target=template_debug arch=arm64

# Compile for iOS (Simulator)
scons platform=ios target=template_debug ios_simulator=yes arch=arm64
```

*Note: On macOS, SCons automatically performs ad-hoc codesigning (`codesign -s - --force`) on compiled libraries and test binaries.*

---

## 3. Running Automated Tests via SCons

All tests are integrated into SCons and should be run using the following targets:

```bash
# Run the fast unit test suite (Native C++ + Python + Headless Godot)
scons tests

# Run specific test suites:
scons tests/native           # Native C++ unit tests & regression benchmark
scons tests/python           # Pytest unit tests (verifies models & coords)
scons tests/godot/headless   # Headless GDScript tests
scons tests/godot/windowed   # Windowed GPU/Compute GDScript tests
scons tests/web              # Web E2E browser automated tests
scons tests/android          # Android emulator/device E2E tests
```

- Running a test target redirects its output to `build/tests/logs/tests.<target>.log` and mirrors it to the terminal.
- Benchmark reports comparing execution to checked-in baselines are written to `build/tests/artifacts/gaze_benchmark_report.md`.
- SCons fails the build if error tolerances shift beyond established limits.

---

## 4. API Documentation

To generate and view the API documentation locally:

```bash
./scripts/generate_docs.sh
```

The documentation will be generated in `docs/doxygen/html/index.html`.
