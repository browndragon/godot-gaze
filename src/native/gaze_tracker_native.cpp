#include "gaze_tracker.hpp"
#include "log.hpp"
#include "camera_sensor.hpp"
#include "face_estimator.hpp"
#include "eye_estimator.hpp"
#include "smoother.hpp"
#include "one_euro_smoother.hpp"
#include "opencv_camera.hpp"
#include "yunet_pipeline.hpp"
#include "opencv_gaze_model.hpp"
#include "display_profile.hpp"
#include "math_defs.hpp"
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

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
}

void GazeTracker::platform_process(double delta) {
    log_this_frame = false;
    if (camera_sensor && face_estimator && eye_estimator) {
        Gaze::Frame frame;
        if (camera_sensor->grab_frame(frame)) {
            if (debug_logging_frames > 0) {
                log_this_frame = true;
                debug_logging_frames--;
            } else if (debug_logging_frames < 0) {
                debug_log_frame_counter++;
                if (debug_log_frame_counter % (-debug_logging_frames) == 0) {
                    log_this_frame = true;
                }
            }

            if (log_this_frame) {
                UtilityFunctions::print("[GazeTracker Telemetry] ========================================");
                UtilityFunctions::print("[GazeTracker Telemetry] Capture Timestamp: ", frame.timestamp);
                UtilityFunctions::print("[GazeTracker Telemetry] Frame Resolution: (", frame.width, ", ", frame.height, ")");
            }

            Gaze::EyeCrops crops;
            if (face_estimator->process_frame(frame, crops)) {
                if (crops.face_detected) {
                    latest_crops = crops;
                    if (log_this_frame) {
                        UtilityFunctions::print("[GazeTracker Telemetry] Face Detected: TRUE");
                        UtilityFunctions::print("[GazeTracker Telemetry] YuNet Left Eye Landmark: (", crops.left_eye_center_cam.x, ", ", crops.left_eye_center_cam.y, ")");
                        UtilityFunctions::print("[GazeTracker Telemetry] YuNet Right Eye Landmark: (", crops.right_eye_center_cam.x, ", ", crops.right_eye_center_cam.y, ")");
                        
                        Gaze::GazeBasis3D R_basis = Gaze::rodrigues_to_basis(crops.head_pose_rotation);
                        Gaze::GazeVector3 euler = R_basis.get_euler_gaze_model_deg();
                        
                        UtilityFunctions::print("[GazeTracker Telemetry] PnP Head Translation (mm): (", crops.head_pose_translation.x, ", ", crops.head_pose_translation.y, ", ", crops.head_pose_translation.z, ")");
                        UtilityFunctions::print("[GazeTracker Telemetry] PnP Head Euler Rotation (deg): (", euler.x, ", ", euler.y, ", ", euler.z, ")");
                    }

                    Gaze::GazeVector3 raw_gaze_dir_cam;
                    if (eye_estimator->opencv_gaze(crops, raw_gaze_dir_cam)) {
                        if (log_this_frame) {
                            UtilityFunctions::print("[GazeTracker Telemetry] Gaze Model raw_gaze_dir_cv: (", raw_gaze_dir_cam.x, ", ", raw_gaze_dir_cam.y, ", ", raw_gaze_dir_cam.z, ")");
                        }

                        // Calculate midpoint of eyes as gaze origin in OpenCV camera space
                        Gaze::GazeVector3 gaze_origin_cv = (crops.left_eye_center_cam + crops.right_eye_center_cam) * 0.5;

                        // Convert eye center origin and raw gaze direction to Camera Space (180 deg rotation about X: X=X, Y=-Y, Z=-Z)
                        Gaze::GazeVector3 origin_cam = projection_engine.opencv_to_camera_space(gaze_origin_cv);
                        Gaze::GazeVector3 dir_cam = projection_engine.opencv_to_camera_space(raw_gaze_dir_cam);

                        if (log_this_frame) {
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
                } else {
                    if (log_this_frame) {
                        UtilityFunctions::print("[GazeTracker Telemetry] Face Detected: FALSE");
                        UtilityFunctions::print("[GazeTracker Telemetry] ========================================");
                    }
                    feed_gaze(false, Vector3(), Vector3());
                }
            } else {
                if (log_this_frame) {
                    UtilityFunctions::print("[GazeTracker Telemetry] Frame processing failed");
                    UtilityFunctions::print("[GazeTracker Telemetry] ========================================");
                }
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

PlatformGeometry GazeTracker::platform_get_geometry() const {
    PlatformGeometry geom;
    DisplayServer* ds = DisplayServer::get_singleton();
    if (ds) {
        int screen_id = ds->window_get_current_screen();
        double scale = ds->screen_get_scale(screen_id);

        OS* os = OS::get_singleton();
        Vector2i window_pos_ppix = ds->window_get_position();
        bool is_standalone = os ? os->has_feature("standalone") : true;
        DisplayServer::WindowMode mode = ds->window_get_mode();
        bool is_fullscreen = (mode == DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN || 
                              mode == DisplayServer::WINDOW_MODE_FULLSCREEN);

        // Fallback: If running windowed in the editor/test runner (non-standalone) 
        // and the position is masked as (0, 0), assume the window is centered on the screen.
        if (!is_fullscreen && window_pos_ppix == Vector2i(0, 0) && !is_standalone) {
            Vector2i screen_size_ppix = ds->screen_get_size(screen_id);
            Vector2i window_size_ppix = ds->window_get_size(); // Physical pixels in Godot 4
            window_pos_ppix = (screen_size_ppix - window_size_ppix) / 2;
        }

        geom.window_position_px = Vector2(window_pos_ppix.x / scale, window_pos_ppix.y / scale);
    }

    if (window_position_override.x >= 0.0 && window_position_override.y >= 0.0) {
        geom.window_position_px = window_position_override;
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
    face_estimator->set_focal_length(camera_sensor->get_focal_length());

    if (pipeline_config.is_valid()) {
        Gaze::PipelineConfig core_cfg = pipeline_config->get_config();
        camera_sensor->set_resolution(core_cfg.desired_camera_width, core_cfg.desired_camera_height);
        face_estimator->set_pipeline_config(core_cfg);
        eye_estimator->set_pipeline_config(core_cfg);
    }

    if (!camera_sensor->initialize_sensor()) {
        Gaze::log_error("GazeTrackerInitFailed", "reason", "camera initialization failed");
        stop_tracker();
        set_lifecycle_state(LIFECYCLE_ERROR);
        return false;
    }
    if (!face_estimator->initialize_estimator()) {
        Gaze::log_error("GazeTrackerInitFailed", "reason", "face pipeline initialization failed");
        stop_tracker();
        set_lifecycle_state(LIFECYCLE_ERROR);
        return false;
    }
    if (!eye_estimator->initialize_estimator()) {
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
