/**
 * @file web_binding_state.hpp
 * @brief Web Emscripten JavaScript Bindings structure (Web only)
 */
#pragma once

#ifdef WEB_ENABLED
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/java_script_object.hpp>
#include <godot_cpp/variant/array.hpp>

namespace godot
{
    class GazeTracker;

    class WebBindingState : public RefCounted
    {
        GDCLASS(WebBindingState, RefCounted);

    private:
        GazeTracker *tracker_ptr = nullptr;
        Ref<JavaScriptObject> permission_callback;
        Ref<JavaScriptObject> feed_callback;
        Ref<JavaScriptObject> ready_callback;

    protected:
        static void _bind_methods();

    public:
        WebBindingState();
        virtual ~WebBindingState();

        void setup_callbacks(GazeTracker *tracker);
        void start_tracking_loop(GazeTracker *tracker, const String &yunet_path, const String &gaze_onnx_path, int camera_width, int camera_height);
        void cleanup();

        // Web callbacks invoked from JavaScript
        void feed_gaze_web_raw(const Array& args);
        void on_sidecar_ready(const Array& args);
        void on_permission_result(const Array& args);
    };
}
#endif
