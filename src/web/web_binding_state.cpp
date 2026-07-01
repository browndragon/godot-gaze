#include "web_binding_state.hpp"
#include "gaze_tracker.hpp"
#include "gaze_server.hpp"
#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#ifdef WEB_ENABLED
namespace godot
{
    void WebBindingState::_bind_methods()
    {
        ClassDB::bind_method(D_METHOD("feed_gaze_web_raw", "args"), &WebBindingState::feed_gaze_web_raw);
        ClassDB::bind_method(D_METHOD("on_sidecar_ready", "args"), &WebBindingState::on_sidecar_ready);
        ClassDB::bind_method(D_METHOD("on_permission_result", "args"), &WebBindingState::on_permission_result);
    }

    WebBindingState::WebBindingState() {}

    WebBindingState::~WebBindingState()
    {
        cleanup();
    }

    void WebBindingState::setup_callbacks(GazeTracker *tracker)
    {
        tracker_ptr = tracker;
        JavaScriptBridge *js = JavaScriptBridge::get_singleton();
        if (!js)
            return;

        permission_callback = js->create_callback(Callable(this, "on_permission_result"));
        feed_callback = js->create_callback(Callable(this, "feed_gaze_web_raw"));
        ready_callback = js->create_callback(Callable(this, "on_sidecar_ready"));

        Ref<JavaScriptObject> window = js->get_interface("window");
        js->eval("window.godotGaze = {};");
        Ref<JavaScriptObject> godotGaze = window->get("godotGaze");
        if (godotGaze.is_valid())
        {
            godotGaze->set("on_permission", permission_callback);
            godotGaze->set("feed_gaze", feed_callback);
            godotGaze->set("on_ready", ready_callback);
        }
    }

    void WebBindingState::start_tracking_loop(GazeTracker *tracker, const String &yunet_path, const String &gaze_onnx_path, int camera_width, int camera_height)
    {
        tracker_ptr = tracker;
        JavaScriptBridge *js = JavaScriptBridge::get_singleton();
        if (!js)
            return;

        bool is_debug = OS::get_singleton()->is_debug_build();
        String debug_str = is_debug ? "true" : "false";
        String w_str = String::num_int64(camera_width);
        String h_str = String::num_int64(camera_height);

        int throttle_interval = 1;
        ProjectSettings *ps = ProjectSettings::get_singleton();
        if (ps && ps->has_setting("gaze/config/debug_image_throttle_interval"))
        {
            throttle_interval = ps->get_setting("gaze/config/debug_image_throttle_interval");
        }
        String throttle_str = String::num_int64(throttle_interval);

        String eval_str = String(
                              "if (!window.gazeTracker) {"
                              "    var s = document.createElement('script');"
                              "    s.src = 'gaze_sidecar.js';"
                              "    s.onload = function() {"
                              "        if (window.gazeTracker) {"
                              "            window.gazeTracker.startTracking(\"") +
                          yunet_path + "\", \"" + gaze_onnx_path + "\", " + debug_str + ", " + w_str + ", " + h_str + ", " + throttle_str + ");"
                                                                                                                                            "        }"
                                                                                                                                            "    };"
                                                                                                                                            "    document.head.appendChild(s);"
                                                                                                                                            "} else {"
                                                                                                                                            "    window.gazeTracker.startTracking(\"" +
                          yunet_path + "\", \"" + gaze_onnx_path + "\", " + debug_str + ", " + w_str + ", " + h_str + ", " + throttle_str + ");"
                                                                                                                                            "}";
        js->eval(eval_str);
    }

    void WebBindingState::cleanup()
    {
        JavaScriptBridge *js = JavaScriptBridge::get_singleton();
        if (js)
        {
            js->eval("if (window.gazeTracker) { window.gazeTracker.stopTracking(); }");
            js->eval("delete window.godotGaze;");
        }
        permission_callback.unref();
        feed_callback.unref();
        ready_callback.unref();
    }

    void WebBindingState::feed_gaze_web_raw(const Array& args)
    {
        GazeServer *gs = GazeServer::get_singleton();
        if (gs)
        {
            gs->feed_gaze_web_raw(args);
        }
    }

    void WebBindingState::on_sidecar_ready(const Array& args)
    {
        if (tracker_ptr)
        {
            tracker_ptr->on_sidecar_ready(args);
        }
    }

    void WebBindingState::on_permission_result(const Array& args)
    {
        if (tracker_ptr && args.size() > 0)
        {
            bool granted = args[0];
            tracker_ptr->on_permission_result(granted);
        }
    }
}
#endif
