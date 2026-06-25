# TODO & Future Scope

## 1. Multi-Platform GDExtension Binding Target Isolation

### Issue
Godot GDExtension bindings generator (`godot-cpp`) originally generated target headers inside `thirdparty/godot-cpp/gen/` that were shared across all platforms. However, these headers contain platform-specific type sizes and alignments (e.g. `StringName` has `STRING_NAME_SIZE = 4` on 32-bit Web/WASM and `STRING_NAME_SIZE = 8` on 64-bit macOS/Windows/Linux). 

If you built the Web target and then immediately built the Desktop target without regenerating `godot-cpp` bindings, the Desktop build compiled using the stale 32-bit headers. This caused mismatched structure sizes and memory offsets, leading to silent segmentation faults (Signal 11) during ClassDB registration on startup.

### Solution
We have **automated and isolated** this dependency by configuring `godot-cpp` and `godot-gaze` SCons build scripts to output and read generated bindings from platform-isolated directories:
- macOS builds generate inside: `thirdparty/godot-cpp/gen_macos/gen/`
- Web builds generate inside: `thirdparty/godot-cpp/gen_web/gen/`

This completely eliminates compile conflicts and allows you to build back-to-back targets without manual cleans or forced binding regenerations:
```bash
# Build macOS target
scons platform=macos target=template_debug

# Build Web/WASM target
scons platform=javascript target=template_debug
```


---

## 2. Web-based Automated Testing [COMPLETED]
We have successfully established a zero-dependency headless Chrome browser integration test runner (`tools/run_gaze_offline_tests.js` and `tests/test_gaze_offline.html`) utilizing a local static node server and POST reporting. This runner verifies:
- Loading of the custom compiled `opencv.js` binary.
- Writing models to the VFS and initializing `cv.FaceDetectorYN`.
- Correct face detection coordinates on `self_center.jpg`.
- solvePnP convergence, eye cropping, and Gaze ADAS network forward pass execution.

---

## 3. Unify Native Desktop OpenCV Build with OpenCV Submodule
- **Objective**: Eventually replace the Homebrew-linked `thirdparty/opencv-brew` dependency with native C++ libraries compiled directly from our local `thirdparty/opencv` git submodule (matching how we build OpenCV.js for web).
- **Benefits**: This will ensure 100% dependency self-containment, caching parity across platforms, and prevent developers from needing to install system-wide package managers (Homebrew, Apt, Chocolatey) to build the extension.

