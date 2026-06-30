#include "gaze_tracker.hpp"
#include "log.hpp"
#include "opencv_camera.hpp"
#include "yunet_pipeline.hpp"
#include "opencv_gaze_model.hpp"
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
    log_this_frame = false;
    if (camera && pipeline && model) {
        Gaze::Frame frame;
        if (camera->grab_frame(frame)) {
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
            if (pipeline->process_frame(frame, crops)) {
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
                    if (model->estimate_raw_gaze(crops, raw_gaze_dir_cam)) {
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
                            
                            UtilityFunctions::print("[GazeTracker Telemetry] Screen Size pixels: (", geom.screen_size_ppix.x, ", ", geom.screen_size_ppix.y, ")");
                            UtilityFunctions::print("[GazeTracker Telemetry] Screen Size mm: (", geom.screen_size_mm.x, ", ", geom.screen_size_mm.y, ")");
                            double calculated_dpi = 0.0;
                            if (geom.screen_size_mm.x > 0.0) {
                                calculated_dpi = (geom.screen_size_ppix.x / geom.screen_size_mm.x) * MM_PER_INCH;
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
        geom.logical_to_physical_pixel_ratio = scale;

        Vector2i size_ppix = ds->screen_get_size(screen_id);
        geom.screen_size_ppix = size_ppix;
        geom.screen_size_lpix = Vector2i((int)(size_ppix.x / scale), (int)(size_ppix.y / scale));

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

        geom.window_position_ppix = window_pos_ppix;

        // Query the ProjectSettings display/window/dpi/allow_hidpi setting to determine Godot's window backing scale
        double godot_window_scale = 1.0;
        bool allow_hidpi = true; // Godot 4 default
        ProjectSettings* ps = ProjectSettings::get_singleton();
        if (ps && ps->has_setting("display/window/dpi/allow_hidpi")) {
            allow_hidpi = ps->get_setting("display/window/dpi/allow_hidpi");
        }

        if (allow_hidpi) {
            godot_window_scale = scale;
        } else {
            godot_window_scale = 1.0;
        }

        double dpi = ds->screen_get_dpi(screen_id);
        // Heuristic: If DisplayServer reports logical layout DPI (72/96) on a high-DPI display,
        // estimate the real physical density using a continuous linear function of logical width:
        // dpi_logical = max(96.0, 172.0 - 0.03 * width_points).
        if (dpi < 120.0 || dpi <= 0.0) {
            double w_lpix = geom.screen_size_lpix.x;
            double dpi_lpix = 172.0 - 0.03 * w_lpix;
            if (dpi_lpix < 96.0) {
                dpi_lpix = 96.0;
            }
            dpi = dpi_lpix * scale; // Convert logical DPI to physical DPI
        }

        if (dpi > 0.0) {
            geom.screen_size_mm = Vector2((size_ppix.x / dpi) * MM_PER_INCH, (size_ppix.y / dpi) * MM_PER_INCH);
        }

        if (scale > 0.0) {
            geom.window_to_screen_scale_ratio = godot_window_scale / scale;
        }
    }

    // Merge user overrides
    geom.merge_overrides(overrides);

    // Enforce sensible logical fallbacks if platform queries returned invalid data
    if (geom.screen_size_lpix.x <= 0 || geom.screen_size_lpix.y <= 0) {
        geom.screen_size_lpix = Vector2i(DEFAULT_SCREEN_SIZE_PIXELS.x, DEFAULT_SCREEN_SIZE_PIXELS.y);
    }
    if (geom.logical_to_physical_pixel_ratio <= 0.0) {
        geom.logical_to_physical_pixel_ratio = 1.0;
    }

    // Enforce sensible millimeter defaults if platform queries returned invalid data
    if (geom.screen_size_mm.x <= 0.0 || geom.screen_size_mm.y <= 0.0) {
        geom.screen_size_mm = Vector2(DEFAULT_SCREEN_SIZE_MM.x, DEFAULT_SCREEN_SIZE_MM.y);
    }

    // Compute final effective physical screen size
    geom.screen_size_ppix = Vector2i(
        (int)(geom.screen_size_lpix.x * geom.logical_to_physical_pixel_ratio),
        (int)(geom.screen_size_lpix.y * geom.logical_to_physical_pixel_ratio)
    );

    return geom;
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

    if (pipeline_config.is_valid()) {
        Gaze::PipelineConfig core_cfg = pipeline_config->get_config();
        camera->set_resolution(core_cfg.desired_camera_width, core_cfg.desired_camera_height);
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
