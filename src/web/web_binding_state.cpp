#include "web_binding_state.hpp"
#include "gaze_tracker.hpp"
#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#ifdef WEB_ENABLED
namespace godot {

void WebBindingState::setup_callbacks(GazeTracker* tracker) {
    JavaScriptBridge *js = JavaScriptBridge::get_singleton();
    if (!js) return;

    permission_callback = js->create_callback(Callable(tracker, "on_permission_result"));
    feed_callback = js->create_callback(Callable(tracker, "feed_gaze_web_raw"));
    ready_callback = js->create_callback(Callable(tracker, "on_sidecar_ready"));

    Ref<JavaScriptObject> window = js->get_interface("window");
    window->set("gaze_permission_callback", permission_callback);
    window->set("gaze_feed_callback", feed_callback);
    window->set("gaze_ready_callback", ready_callback);
}

void WebBindingState::start_tracking_loop(GazeTracker* tracker, const String& yunet_path, const String& gaze_onnx_path) {
    JavaScriptBridge *js = JavaScriptBridge::get_singleton();
    if (!js) return;

    // Load the gaze_sidecar.js script dynamically
    String eval_str = String(
        "if (!window.gazeTracker) {"
        "    var s = document.createElement('script');"
        "    s.src = 'gaze_sidecar.js';"
        "    s.onload = function() {"
        "        if (window.gazeTracker) {"
        "            window.gazeTracker.startTracking(\"") + yunet_path + "\", \"" + gaze_onnx_path + "\");"
        "        }"
        "    };"
        "    document.head.appendChild(s);"
        "} else {"
        "    window.gazeTracker.startTracking(\"" + yunet_path + "\", \"" + gaze_onnx_path + "\");"
        "}";
    js->eval(eval_str);
}

void WebBindingState::cleanup() {
    JavaScriptBridge *js = JavaScriptBridge::get_singleton();
    if (js) {
        js->eval("if (window.gazeTracker) { window.gazeTracker.stopTracking(); }");
        Ref<JavaScriptObject> window = js->get_interface("window");
        if (window.is_valid()) {
            window->set("gaze_permission_callback", Variant());
            window->set("gaze_feed_callback", Variant());
            window->set("gaze_ready_callback", Variant());
        }
    }
    permission_callback.unref();
    feed_callback.unref();
    ready_callback.unref();
}

} // namespace godot
#endif
