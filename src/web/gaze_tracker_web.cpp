#include "gaze_tracker.hpp"
#include "log.hpp"
#include "math_defs.hpp"
#include "camera_sensor.hpp"
#include "face_estimator.hpp"
#include "eye_estimator.hpp"
#include "smoother.hpp"
#include "one_euro_smoother.hpp"

#ifdef WEB_ENABLED
#include "web_binding_state.hpp"
#include "web_gaze_model.hpp"
#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
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

PlatformGeometry GazeTracker::platform_get_geometry() const {
    PlatformGeometry geom;
    geom.window_position_px = web_canvas_pos;

    if (window_position_override.x >= 0.0 && window_position_override.y >= 0.0) {
        geom.window_position_px = window_position_override;
    }

    if (log_this_frame) {
        UtilityFunctions::print("[GazeTracker Web Debug] platform_get_geometry return: window_pos_lpix: ", geom.window_position_px);
    }

    return geom;
}


bool GazeTracker::complete_initialization() {
    if (!camera_sensor) {
        camera_sensor = memnew(CameraSensor);
        camera_sensor->set_name("CameraSensor");
        add_child(camera_sensor);
    }
    if (!face_estimator) {
        face_estimator = memnew(FaceEstimator);
        face_estimator->set_name("FaceEstimator");
        camera_sensor->add_child(face_estimator);
    }
    if (!eye_estimator) {
        eye_estimator = memnew(EyeEstimator);
        eye_estimator->set_name("EyeEstimator");
        face_estimator->add_child(eye_estimator);
    }

    camera_sensor->set_camera_device_id(camera_device_id);
    face_estimator->set_yunet_model_path(yunet_model_path);
    face_estimator->set_focal_length(camera_focal_length_px);
    eye_estimator->set_gaze_model_path(gaze_onnx_path);

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
        camera_sensor->set_resolution(desired_camera_width, desired_camera_height);
        face_estimator->set_pipeline_config(core_cfg);
        eye_estimator->set_pipeline_config(core_cfg);
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
    log_this_frame = false;
    if (args.size() == 0) return;
    
    if (debug_logging_frames > 0) {
        log_this_frame = true;
        debug_logging_frames--;
    } else if (debug_logging_frames < 0) {
        debug_log_frame_counter++;
        if (debug_log_frame_counter % (-debug_logging_frames) == 0) {
            log_this_frame = true;
        }
    }

    bool face_detected = args[0];
    if (!face_detected) {
        if (log_this_frame) {
            UtilityFunctions::print("[GazeTracker Telemetry] ========================================");
            UtilityFunctions::print("[GazeTracker Telemetry] [Web Target] Frame Received");
            UtilityFunctions::print("[GazeTracker Telemetry] Face Detected: FALSE");
            UtilityFunctions::print("[GazeTracker Telemetry] ========================================");
        }
        if (face_estimator) {
            face_estimator->feed_pose_web(false, Vector3(), Vector3());
        }
        feed_gaze(false, Vector3(), Vector3());
        return;
    }

    if (args.size() >= 18) {
        // Unpack raw OpenCV camera space coordinates (in mm)
        // args[1..3] is lex (anatomical left eye / viewer's right)
        // args[4..6] is rex (anatomical right eye / viewer's left)
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

        if (face_estimator) {
            face_estimator->feed_pose_web(true, 
                Vector3(latest_crops.head_pose_translation.x, latest_crops.head_pose_translation.y, latest_crops.head_pose_translation.z), 
                Vector3(latest_crops.head_pose_rotation.x, latest_crops.head_pose_rotation.y, latest_crops.head_pose_rotation.z));
        }
        if (eye_estimator) {
            eye_estimator->feed_gaze_web(Vector3(dir_cv.x, dir_cv.y, dir_cv.z));
        }

        if (log_this_frame) {
            UtilityFunctions::print("[GazeTracker Telemetry] ========================================");
            UtilityFunctions::print("[GazeTracker Telemetry] [Web Target] Frame Received");
            UtilityFunctions::print("[GazeTracker Telemetry] Face Detected: TRUE");
            UtilityFunctions::print("[GazeTracker Telemetry] YuNet Left Eye Landmark: (", left_eye_cv.x, ", ", left_eye_cv.y, ", ", left_eye_cv.z, ")");
            UtilityFunctions::print("[GazeTracker Telemetry] YuNet Right Eye Landmark: (", right_eye_cv.x, ", ", right_eye_cv.y, ", ", right_eye_cv.z, ")");
            
            Gaze::GazeBasis3D R_basis = Gaze::rodrigues_to_basis(latest_crops.head_pose_rotation);
            Gaze::GazeVector3 euler = R_basis.get_euler_gaze_model_deg();
            
            UtilityFunctions::print("[GazeTracker Telemetry] PnP Head Translation (mm): (", latest_crops.head_pose_translation.x, ", ", latest_crops.head_pose_translation.y, ", ", latest_crops.head_pose_translation.z, ")");
            UtilityFunctions::print("[GazeTracker Telemetry] PnP Head Euler Rotation (deg): (", euler.x, ", ", euler.y, ", ", euler.z, ")");
            UtilityFunctions::print("[GazeTracker Telemetry] Gaze Model raw_gaze_dir_cv: (", dir_cv.x, ", ", dir_cv.y, ", ", dir_cv.z, ")");
            UtilityFunctions::print("[GazeTracker Telemetry] Camera Space Origin (mm): (", origin_cam.x, ", ", origin_cam.y, ", ", origin_cam.z, ")");
            UtilityFunctions::print("[GazeTracker Telemetry] Camera Space Direction: (", dir_cam.x, ", ", dir_cam.y, ", ", dir_cam.z, ")");
            
            PlatformGeometry geom = platform_get_geometry();
            Transform2D vp_xform = get_adjusted_viewport_transform();
            
            Vector2i display_size_px = Vector2i(DEFAULT_SCREEN_SIZE_PIXELS.x, DEFAULT_SCREEN_SIZE_PIXELS.y);
            Vector2 display_size_mm = Vector2(DEFAULT_SCREEN_SIZE_MM.x, DEFAULT_SCREEN_SIZE_MM.y);
            if (display_profile.is_valid()) {
                display_size_px = display_profile->get_logical_size_px();
                display_size_mm = display_profile->get_physical_size_mm();
            }
            UtilityFunctions::print("[GazeTracker Telemetry] Screen Size pixels: (", display_size_px.x, ", ", display_size_px.y, ")");
            UtilityFunctions::print("[GazeTracker Telemetry] Screen Size mm: (", display_size_mm.x, ", ", display_size_mm.y, ")");
            double calculated_dpi = 0.0;
            if (display_profile.is_valid()) {
                Vector2 dpi_v = display_profile->get_dpi();
                calculated_dpi = dpi_v.x;
            }
            UtilityFunctions::print("[GazeTracker Telemetry] Screen derived DPI: ", calculated_dpi);
            UtilityFunctions::print("[GazeTracker Telemetry] Viewport Scale: (", vp_xform.get_scale().x, ", ", vp_xform.get_scale().y, ") Origin: (", vp_xform.get_origin().x, ", ", vp_xform.get_origin().y, ")");
            UtilityFunctions::print("[GazeTracker Telemetry] ========================================");
        }

        feed_gaze(true, Vector3(origin_cam.x, origin_cam.y, origin_cam.z), Vector3(dir_cam.x, dir_cam.y, dir_cam.z));
    }
}

void GazeTracker::on_sidecar_ready(const Array& args) {
    if (args.size() == 0) return;
    Ref<JavaScriptObject> sidecar = args[0];
    if (sidecar.is_null() || !sidecar.is_valid()) return;

    // Load yunet bytes
    String resolved_yunet = face_estimator ? face_estimator->get_yunet_model_path() : "";
    if (resolved_yunet.is_empty()) {
        resolved_yunet = "res://models/face_detection_yunet_2023mar.onnx";
    }
    Ref<FileAccess> f_yunet = FileAccess::open(resolved_yunet, FileAccess::READ);
    PackedByteArray yunet_bytes;
    if (f_yunet.is_valid()) {
        yunet_bytes = f_yunet->get_buffer(f_yunet->get_length());
    } else {
        Gaze::log_error("WebSidecarLoadModelFailed", "path", resolved_yunet.utf8().get_data());
    }

    // Load gaze bytes (web target requires ONNX format, converting .xml suffix to .onnx)
    String resolved_gaze = eye_estimator ? eye_estimator->get_gaze_model_path() : "";
    if (resolved_gaze.is_empty()) {
        resolved_gaze = "res://models/gaze-estimation-adas-0002.xml";
    }
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

    double focal = camera_sensor ? camera_sensor->get_focal_length() : 1000.0;

    // Call setModels on the sidecar JavaScriptObject
    sidecar->call("setModels", hex_yunet, hex_gaze, fd_width, fd_height, focal);
}

} // namespace godot
#endif
