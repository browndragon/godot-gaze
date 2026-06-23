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

## 2. Web-based Automated Testing
Since the web build relies on browser-specific Web APIs (`navigator.mediaDevices.getUserMedia`) and Javascript callbacks via `JavaScriptBridge`, automated integration testing is challenging on local native test suites.

- **Objective**: Establish headless browser automation tests (e.g., using Puppeteer, Playwright, or Selenium) to verify camera request permission prompts and WebGaze pipeline stubs.
- **Challenges**: Mocking media devices (camera feeds) inside headless Chromium, and synchronizing Godot/WASM lifecycle states with browser test runner assertions.
