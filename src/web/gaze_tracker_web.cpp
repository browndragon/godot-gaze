#include "gaze_tracker.hpp"
#include "log.hpp"

#ifdef WEB_ENABLED
#include "web_binding_state.hpp"
#include "web_gaze_model.hpp"
#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {


void GazeTracker::platform_initialize() {
    if (!opaque) {
        WebBindingState* state = new WebBindingState();
        opaque = state;
        state->setup_callbacks(this);
    }

    set_lifecycle_state(LIFECYCLE_INITIALIZING);
    complete_initialization();
}

void GazeTracker::platform_terminate() {
    if (opaque) {
        WebBindingState* state = static_cast<WebBindingState*>(opaque);
        state->cleanup();
        delete state;
        opaque = nullptr;
    }
}

void GazeTracker::platform_process(double delta) {
    // Web handles updates asynchronously via callback, so no CPU polling is needed here
}

void GazeTracker::platform_on_permission_result(bool granted) {
    // MediaDevices.getUserMedia handles browser permission prompts asynchronously, so no-op here
}

void GazeTracker::platform_trigger_permission_request() {
    // Handled browser-side via standard getUserMedia prompt when starting tracking loop
}

Vector2 GazeTracker::platform_get_window_position() const {
    return web_canvas_pos;
}

Gaze::GazeVector2i GazeTracker::platform_get_screen_size() const {
    JavaScriptBridge *js = JavaScriptBridge::get_singleton();
    if (js) {
        int w = (int)js->eval("window.screen.width");
        int h = (int)js->eval("window.screen.height");
        if (w > 0 && h > 0) {
            return Gaze::GazeVector2i(w, h);
        }
    }
    return Gaze::GazeVector2i(0, 0);
}

Gaze::GazeVector2 GazeTracker::platform_get_screen_size_mm() const {
    JavaScriptBridge *js = JavaScriptBridge::get_singleton();
    if (js) {
        double w = (double)js->eval("window.screen.width");
        double h = (double)js->eval("window.screen.height");
        if (w > 0.0 && h > 0.0) {
            return Gaze::GazeVector2((w / CSS_PIXELS_PER_INCH) * MM_PER_INCH, (h / CSS_PIXELS_PER_INCH) * MM_PER_INCH);
        }
    }
    return Gaze::GazeVector2(0.0, 0.0);
}

bool GazeTracker::complete_initialization() {
    String resolved_yunet = yunet_model_path.is_empty() ? "res://models/face_detection_yunet_2023mar.onnx" : yunet_model_path;
    String resolved_gaze = gaze_onnx_path.is_empty() ? "res://models/gaze-estimation-adas-0002.xml" : gaze_onnx_path;
    if (resolved_gaze.ends_with(".xml")) {
        resolved_gaze = resolved_gaze.replace(".xml", ".onnx");
    }

    int desired_camera_width = 640;
    int desired_camera_height = 480;
    if (pipeline_config.is_valid()) {
        Gaze::PipelineConfig core_cfg = pipeline_config->get_config();
        desired_camera_width = core_cfg.desired_camera_width;
        desired_camera_height = core_cfg.desired_camera_height;
    }

    if (opaque) {
        WebBindingState* state = static_cast<WebBindingState*>(opaque);
        state->start_tracking_loop(this, resolved_yunet, resolved_gaze, desired_camera_width, desired_camera_height);
    }

    tracker_initialized = true;
    set_lifecycle_state(LIFECYCLE_RUNNING);
    Gaze::log_info("GazeTrackerInitWebSuccess");
    return true;
}

void GazeTracker::feed_gaze_web_raw(const Array& args) {
    if (args.size() == 0) return;
    bool face_detected = args[0];
    if (!face_detected) {
        feed_gaze(false, Vector3(), Vector3());
        return;
    }

    if (args.size() >= 18) {
        // Unpack raw OpenCV camera space coordinates (in mm)
        Gaze::GazeVector3 left_eye_cv(args[1], args[2], args[3]);
        Gaze::GazeVector3 right_eye_cv(args[4], args[5], args[6]);
        Gaze::GazeVector3 dir_cv(args[7], args[8], args[9]);

        // Midpoint of eyes as gaze origin in OpenCV camera space
        Gaze::GazeVector3 origin_cv = (left_eye_cv + right_eye_cv) * 0.5;

        // Convert eye center origin and raw gaze direction to Camera Space (180 deg rotation about X: X=X, Y=-Y, Z=-Z)
        Gaze::GazeVector3 origin_cam = projection_engine.opencv_to_camera_space(origin_cv);
        Gaze::GazeVector3 dir_cam = projection_engine.opencv_to_camera_space(dir_cv);

        // Also populate latest_crops using opencv space because other methods like get_head_transform() expect it
        latest_crops.face_detected = true;
        latest_crops.left_eye_center_cam = left_eye_cv;
        latest_crops.right_eye_center_cam = right_eye_cv;
        latest_crops.head_pose_translation = Gaze::GazeVector3(args[10], args[11], args[12]);
        latest_crops.head_pose_rotation = Gaze::GazeVector3(args[13], args[14], args[15]);

        // Extract canvas screen coordinates (displacement offset)
        web_canvas_pos = Vector2(args[16], args[17]);

        feed_gaze(true, Vector3(origin_cam.x, origin_cam.y, origin_cam.z), Vector3(dir_cam.x, dir_cam.y, dir_cam.z));
    }
}

void GazeTracker::on_sidecar_ready(const Array& args) {
    if (args.size() == 0) return;
    Ref<JavaScriptObject> sidecar = args[0];
    if (sidecar.is_null() || !sidecar.is_valid()) return;

    // Load yunet bytes
    String resolved_yunet = yunet_model_path.is_empty() ? "res://models/face_detection_yunet_2023mar.onnx" : yunet_model_path;
    Ref<FileAccess> f_yunet = FileAccess::open(resolved_yunet, FileAccess::READ);
    PackedByteArray yunet_bytes;
    if (f_yunet.is_valid()) {
        yunet_bytes = f_yunet->get_buffer(f_yunet->get_length());
    } else {
        Gaze::log_error("WebSidecarLoadModelFailed", "path", resolved_yunet.utf8().get_data());
    }

    // Load gaze bytes (web target requires ONNX format, converting .xml suffix to .onnx)
    String resolved_gaze = gaze_onnx_path.is_empty() ? "res://models/gaze-estimation-adas-0002.xml" : gaze_onnx_path;
    if (resolved_gaze.ends_with(".xml")) {
        resolved_gaze = resolved_gaze.replace(".xml", ".onnx");
    }
    Ref<FileAccess> f_gaze = FileAccess::open(resolved_gaze, FileAccess::READ);
    PackedByteArray gaze_bytes;
    if (f_gaze.is_valid()) {
        gaze_bytes = f_gaze->get_buffer(f_gaze->get_length());
    } else {
        Gaze::log_error("WebSidecarLoadModelFailed", "path", resolved_gaze.utf8().get_data());
    }

    String hex_yunet = yunet_bytes.hex_encode();
    String hex_gaze = gaze_bytes.hex_encode();

    int fd_width = 160;
    int fd_height = 128;
    if (pipeline_config.is_valid()) {
        Gaze::PipelineConfig core_cfg = pipeline_config->get_config();
        fd_width = core_cfg.face_detect_width;
        fd_height = core_cfg.face_detect_height;
    }

    // Call setModels on the sidecar JavaScriptObject
    sidecar->call("setModels", hex_yunet, hex_gaze, fd_width, fd_height);
}

} // namespace godot
#endif
