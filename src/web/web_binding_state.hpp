#pragma once

#ifdef WEB_ENABLED
#include <godot_cpp/classes/java_script_object.hpp>
#include <godot_cpp/classes/ref.hpp>

namespace godot {

class GazeTracker;

struct WebBindingState {
    Ref<JavaScriptObject> permission_callback;
    Ref<JavaScriptObject> feed_callback;
    Ref<JavaScriptObject> ready_callback;

    void setup_callbacks(GazeTracker* tracker);
    void start_tracking_loop(GazeTracker* tracker, const String& yunet_path, const String& gaze_onnx_path);
    void cleanup();
};

} // namespace godot
#endif
