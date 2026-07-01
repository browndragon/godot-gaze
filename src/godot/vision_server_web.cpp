#include "vision_server.hpp"
#include "../core/log.hpp"
#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/os.hpp>

#ifdef WEB_ENABLED
namespace godot {

bool VisionServer::camera_start(RID p_camera) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL_V(data, false);

    if (data->is_active) return true;

    // Handle mock camera
    if (data->device_id == -1) {
        data->is_active = true;
        return true;
    }

    JavaScriptBridge *js = JavaScriptBridge::get_singleton();
    if (!js) return false;

    bool is_debug = OS::get_singleton()->is_debug_build();
    String debug_str = is_debug ? "true" : "false";
    String w_str = String::num_int64(data->width);
    String h_str = String::num_int64(data->height);

    // Default model paths for web
    String resolved_yunet = "res://models/face_detection_yunet_2023mar.onnx";
    String resolved_gaze = "res://models/gaze-estimation-adas-0002.onnx";

    String eval_str = String(
        "if (!window.gazeTracker) {"
        "    var s = document.createElement('script');"
        "    s.src = 'gaze_sidecar.js';"
        "    s.onload = function() {"
        "        if (window.gazeTracker) {"
        "            window.gazeTracker.startTracking(\"") + resolved_yunet + "\", \"" + resolved_gaze + "\", " + debug_str + ", " + w_str + ", " + h_str + ");"
        "        }"
        "    };"
        "    document.head.appendChild(s);"
        "} else {"
        "    window.gazeTracker.startTracking(\"" + resolved_yunet + "\", \"" + resolved_gaze + "\", " + debug_str + ", " + w_str + ", " + h_str + ");"
        "}";
    js->eval(eval_str);

    data->is_active = true;
    Gaze::log_info("VisionServerWeb_CameraStarted");
    return true;
}

void VisionServer::camera_stop(RID p_camera) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL(data);

    if (!data->is_active) return;

    if (data->device_id != -1) {
        JavaScriptBridge *js = JavaScriptBridge::get_singleton();
        if (js) {
            js->eval("if (window.gazeTracker) { window.gazeTracker.stopTracking(); }");
        }
    }

    data->is_active = false;
    data->last_frame = Gaze::Frame();
    data->last_frame_data.clear();
    Gaze::log_info("VisionServerWeb_CameraStopped");
}

bool VisionServer::get_camera_current_frame(RID p_camera, Gaze::Frame &r_frame) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL_V(data, false);

    if (!data->is_active) return false;

    // In web and mock environments, frames are injected or processed externally
    if (data->last_frame.data == nullptr) {
        return false;
    }

    r_frame = data->last_frame;
    return true;
}

} // namespace godot
#endif
