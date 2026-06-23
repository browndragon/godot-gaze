# GazeTracker Project Backlog & Design Journal

This document serves as a persistent backlog, design journal, and architectural tracker for the `godot-gaze` project. It records work items that are out of scope for current tasks, design implications to carry forward, and stubs requiring full implementations in future iterations.

---

## Out-of-Scope Roadmap Work

### 1. Headless Browser Automated Testing (CI/CD Web)
To test the web-based permission flow and gaze inference without manual browser interaction:
- **Framework**: Use Selenium, Puppeteer, or Playwright.
- **Chrome Mock Flags**: Configure the browser instance with the following arguments:
  - `--use-fake-ui-for-media-stream`: Automatically grants camera and microphone permissions, skipping the browser dialog.
  - `--use-fake-device-for-media-stream`: Feeds a fake generated test pattern or video file as the webcam stream.
- **Verification Assertions**:
  - Assert that the GDExtension transitions `lifecycle_state` to `LIFECYCLE_RUNNING`.
  - Assert that `feed_gaze_web()` receives mock vectors and propagates them to the viewport projection engine.

### 2. Android Emulator Permission Automation
To test the Android runtime permission check automatically:
- **Framework**: Use Espresso and UI Automator.
- **UI Automator Script**: Write a hook to wait for the Android system permission popup and click the "Allow" button:
  ```kotlin
  val device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
  val allowButton = device.findObject(UiSelector().textMatches("(?i)allow|while using the app"))
  if (allowButton.exists()) { allowButton.click() }
  ```
- **ADB Command Option**: Alternatively, pre-grant the permission before launching the test via ADB:
  ```bash
  adb shell pm grant <package_name> android.permission.CAMERA
  ```

### 3. C++ State Machine Unit Tests
To unit test the permission states and callback triggers in isolation:
- Create a mock `OS` and `JavaScriptBridge` interface wrapper layer in C++.
- Assert that calling `trigger_permission_request()` transitions the state variables correctly when the mocked responses are resolved.

---

## Design Backlog & Future Cleanup

### 1. Web Blendshape & Expression Tracking Stub
- **Status**: Currently, `feed_expression_web()` is a stub in `gaze_tracker.cpp`.
- **Roadmap**: Implement web-side blendshape extraction (e.g. mouth open, smile, frown) using MediaPipe Face Mesh in the browser, and map it back into C++ to drive face avatars in Godot.

### 2. Calibration State Serialization
- **Status**: The calibration is saved dynamically in the `calibration_resource`, but it requires manual save triggers in GDScript.
- **Roadmap**: Integrate automatic serialization and persistence of the calibrated OLS bias weights across application launches.

### 3. Buffer-Based Model Loading
- **Status**: The model loading process requires "unwrapping" (copying model files from virtual `res://` paths to `user://` physical storage) at runtime so OpenCV can read them from a physical filesystem.
- **Roadmap**: Examine whether OpenCV's loading process has hooks (like memory buffer loading) to load models directly from Godot's virtual filesystem arrays, avoiding this copying stage entirely.
