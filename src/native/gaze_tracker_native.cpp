#include "gaze_tracker.hpp"
#include "log.hpp"
#include "opencv_camera.hpp"
#include "yunet_pipeline.hpp"
#include "opencv_gaze_model.hpp"
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/os.hpp>

namespace godot {

void GazeTracker::platform_initialize() {
    // Check platform camera permission
    bool has_permission = false;
    OS *os = OS::get_singleton();
    if (os && os->has_feature("android")) {
        PackedStringArray granted_perms = os->get_granted_permissions();
        for (int i = 0; i < granted_perms.size(); ++i) {
            if (granted_perms[i] == "android.permission.CAMERA") {
                has_permission = true;
                break;
            }
        }
    } else {
        has_permission = true;
    }

    if (!has_permission) {
        set_lifecycle_state(LIFECYCLE_PERM_REQ);
        trigger_permission_request();
        return;
    }

    set_lifecycle_state(LIFECYCLE_INITIALIZING);
    complete_initialization();
}

void GazeTracker::platform_terminate() {
    if (camera) {
        delete camera;
        camera = nullptr;
    }
    if (pipeline) {
        delete pipeline;
        pipeline = nullptr;
    }
    if (model) {
        delete model;
        model = nullptr;
    }
}

void GazeTracker::platform_process(double delta) {
    if (camera && pipeline && model) {
        Gaze::Frame frame;
        if (camera->grab_frame(frame)) {
            Gaze::EyeCrops crops;
            if (pipeline->process_frame(frame, crops)) {
                if (crops.face_detected) {
                    latest_crops = crops;
                    Gaze::GazeVector3 raw_gaze_dir_cam;
                    if (model->estimate_raw_gaze(crops, raw_gaze_dir_cam)) {
                        // Calculate midpoint of eyes as gaze origin in OpenCV camera space
                        Gaze::GazeVector3 gaze_origin_cv = (crops.left_eye_center_cam + crops.right_eye_center_cam) * 0.5;

                        // Convert eye center origin and raw gaze direction to Camera Space (180 deg rotation about X: X=X, Y=-Y, Z=-Z)
                        Gaze::GazeVector3 origin_cam = projection_engine.opencv_to_camera_space(gaze_origin_cv);
                        Gaze::GazeVector3 dir_cam = projection_engine.opencv_to_camera_space(raw_gaze_dir_cam);

                        feed_gaze(true, Vector3(origin_cam.x, origin_cam.y, origin_cam.z), Vector3(dir_cam.x, dir_cam.y, dir_cam.z));
                    }
                } else {
                    feed_gaze(false, Vector3(), Vector3());
                }
            } else {
                feed_gaze(false, Vector3(), Vector3());
            }
        }
    }
}

void GazeTracker::platform_on_permission_result(bool granted) {
    OS *os = OS::get_singleton();
    if (os && os->has_feature("android")) {
        PackedStringArray granted_perms = os->get_granted_permissions();
        bool has_camera = false;
        for (int i = 0; i < granted_perms.size(); ++i) {
            if (granted_perms[i] == "android.permission.CAMERA") {
                has_camera = true;
                break;
            }
        }
        if (!has_camera) {
            set_lifecycle_state(LIFECYCLE_ERROR);
        }
    }
}

void GazeTracker::platform_trigger_permission_request() {
    OS *os = OS::get_singleton();
    if (os) {
        if (os->has_feature("android")) {
            os->request_permission("android.permission.CAMERA");
        }
    }
}

Vector2 GazeTracker::platform_get_window_position() const {
    DisplayServer* ds = DisplayServer::get_singleton();
    if (ds) {
        return Vector2(ds->window_get_position());
    }
    return Vector2(0.0, 0.0);
}

Vector2i GazeTracker::platform_get_screen_size() const {
    DisplayServer* ds = DisplayServer::get_singleton();
    if (ds) {
        return ds->screen_get_size(ds->window_get_current_screen());
    }
    return Vector2i(1920, 1080);
}

Vector2 GazeTracker::platform_get_screen_size_mm() const {
    DisplayServer* ds = DisplayServer::get_singleton();
    if (ds) {
        int screen_id = ds->window_get_current_screen();
        double dpi = ds->screen_get_dpi(screen_id);
        Vector2i size_px = platform_get_screen_size();
        if (dpi > 0.0) {
            return Vector2((size_px.x / dpi) * 25.4, (size_px.y / dpi) * 25.4);
        }
    }
    return Vector2(345.0, 215.0); // MacBook 15" fallback
}

bool GazeTracker::complete_initialization() {
    if (!camera) camera = new Gaze::OpenCVCamera(camera_device_id);
    if (!pipeline) {
        String resolved_yunet = yunet_model_path.is_empty() ? "res://models/face_detection_yunet_2023mar.onnx" : yunet_model_path;
        String global_yunet_path = copy_model_to_user_dir(resolved_yunet);
        pipeline = new Gaze::YuNetPipeline(global_yunet_path.utf8().get_data());
        pipeline->set_camera_focal_length_px(camera_focal_length_px);
    }
    if (!model) {
        String resolved_gaze = gaze_onnx_path.is_empty() ? "res://models/gaze-estimation-adas-0002.xml" : gaze_onnx_path;
        String global_gaze_path = copy_model_to_user_dir(resolved_gaze);
        model = new Gaze::OpenCVGazeModel(global_gaze_path.utf8().get_data());
    }

    if (!camera->initialize()) {
        Gaze::log_error("GazeTrackerInitFailed", "reason", "camera initialization failed");
        stop_tracker();
        set_lifecycle_state(LIFECYCLE_ERROR);
        return false;
    }
    if (!pipeline->initialize()) {
        Gaze::log_error("GazeTrackerInitFailed", "reason", "face pipeline initialization failed");
        stop_tracker();
        set_lifecycle_state(LIFECYCLE_ERROR);
        return false;
    }
    if (!model->initialize()) {
        Gaze::log_error("GazeTrackerInitFailed", "reason", "gaze model initialization failed");
        stop_tracker();
        set_lifecycle_state(LIFECYCLE_ERROR);
        return false;
    }

    update_pipeline_config();

    tracker_initialized = true;
    set_lifecycle_state(LIFECYCLE_RUNNING);
    Gaze::log_info("GazeTrackerInitNativeSuccess");
    return true;
}

// Native stubs for web callback handlers
void GazeTracker::feed_gaze_web_raw(const Array& args) {}
void GazeTracker::on_sidecar_ready(const Array& args) {}

} // namespace godot
