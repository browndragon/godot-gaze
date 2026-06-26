#include "gaze_tracker.hpp"
#include "log.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

// Platform-specific headers are included in gaze_tracker_native.cpp and gaze_tracker_web.cpp


namespace godot {

void GazeTracker::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("initialize_tracker"), &GazeTracker::initialize_tracker);
    ClassDB::bind_method(D_METHOD("stop_tracker", "emit_signal"), &GazeTracker::stop_tracker, DEFVAL(true));
    ClassDB::bind_method(D_METHOD("complete_initialization"), &GazeTracker::complete_initialization);
    ClassDB::bind_method(D_METHOD("trigger_permission_request"), &GazeTracker::trigger_permission_request);
    ClassDB::bind_method(D_METHOD("on_permission_result", "granted"), &GazeTracker::on_permission_result);
    ClassDB::bind_method(D_METHOD("calibrate_3d", "target_pixel"), &GazeTracker::calibrate_3d);
    ClassDB::bind_method(D_METHOD("calibrate_2d", "target_pixel"), &GazeTracker::calibrate_2d);
    ClassDB::bind_method(D_METHOD("clear_calibration"), &GazeTracker::clear_calibration);
    ClassDB::bind_method(D_METHOD("feed_gaze", "face_detected", "origin", "direction"), &GazeTracker::feed_gaze);
    ClassDB::bind_method(D_METHOD("feed_gaze_web_raw", "args"), &GazeTracker::feed_gaze_web_raw);
    ClassDB::bind_method(D_METHOD("on_sidecar_ready", "args"), &GazeTracker::on_sidecar_ready);

    ClassDB::bind_method(D_METHOD("get_latest_projected_gaze"), &GazeTracker::get_latest_projected_gaze);
    ClassDB::bind_method(D_METHOD("get_latest_filtered_gaze"), &GazeTracker::get_latest_filtered_gaze);
    ClassDB::bind_method(D_METHOD("is_face_detected"), &GazeTracker::is_face_detected);

    // Properties Setters & Getters
    ClassDB::bind_method(D_METHOD("get_lifecycle_state"), &GazeTracker::get_lifecycle_state);
    ClassDB::bind_method(D_METHOD("set_autostart", "autostart"), &GazeTracker::set_autostart);
    ClassDB::bind_method(D_METHOD("get_autostart"), &GazeTracker::get_autostart);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "lifecycle_state", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "", "get_lifecycle_state");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autostart"), "set_autostart", "get_autostart");

    ClassDB::bind_method(D_METHOD("set_pipeline_config", "res"), &GazeTracker::set_pipeline_config);
    ClassDB::bind_method(D_METHOD("get_pipeline_config"), &GazeTracker::get_pipeline_config);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "pipeline_config", PROPERTY_HINT_RESOURCE_TYPE, "GazePipelineConfig"), "set_pipeline_config", "get_pipeline_config");

    ClassDB::bind_method(D_METHOD("set_calibration_resource", "res"), &GazeTracker::set_calibration_resource);
    ClassDB::bind_method(D_METHOD("get_calibration_resource"), &GazeTracker::get_calibration_resource);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "calibration_resource", PROPERTY_HINT_RESOURCE_TYPE, "GazeCalibrationResource"), "set_calibration_resource", "get_calibration_resource");

    ClassDB::bind_method(D_METHOD("set_camera_offset", "offset"), &GazeTracker::set_camera_offset);
    ClassDB::bind_method(D_METHOD("get_camera_offset"), &GazeTracker::get_camera_offset);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "camera_offset"), "set_camera_offset", "get_camera_offset");

    ClassDB::bind_method(D_METHOD("set_camera_tilt", "tilt"), &GazeTracker::set_camera_tilt);
    ClassDB::bind_method(D_METHOD("get_camera_tilt"), &GazeTracker::get_camera_tilt);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "camera_tilt"), "set_camera_tilt", "get_camera_tilt");

    ClassDB::bind_method(D_METHOD("set_screen_size_pixels", "size"), &GazeTracker::set_screen_size_pixels);
    ClassDB::bind_method(D_METHOD("get_screen_size_pixels"), &GazeTracker::get_screen_size_pixels);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "screen_size_pixels"), "set_screen_size_pixels", "get_screen_size_pixels");

    ClassDB::bind_method(D_METHOD("set_screen_size_mm", "size"), &GazeTracker::set_screen_size_mm);
    ClassDB::bind_method(D_METHOD("get_screen_size_mm"), &GazeTracker::get_screen_size_mm);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "screen_size_mm"), "set_screen_size_mm", "get_screen_size_mm");

    ClassDB::bind_method(D_METHOD("set_camera_focal_length_px", "f"), &GazeTracker::set_camera_focal_length_px);
    ClassDB::bind_method(D_METHOD("get_camera_focal_length_px"), &GazeTracker::get_camera_focal_length_px);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "camera_focal_length_px"), "set_camera_focal_length_px", "get_camera_focal_length_px");

    ClassDB::bind_method(D_METHOD("set_camera_device_id", "id"), &GazeTracker::set_camera_device_id);
    ClassDB::bind_method(D_METHOD("get_camera_device_id"), &GazeTracker::get_camera_device_id);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "camera_device_id"), "set_camera_device_id", "get_camera_device_id");

    ClassDB::bind_method(D_METHOD("set_filter_min_cutoff", "val"), &GazeTracker::set_filter_min_cutoff);
    ClassDB::bind_method(D_METHOD("get_filter_min_cutoff"), &GazeTracker::get_filter_min_cutoff);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "filter_min_cutoff"), "set_filter_min_cutoff", "get_filter_min_cutoff");

    ClassDB::bind_method(D_METHOD("set_filter_beta", "val"), &GazeTracker::set_filter_beta);
    ClassDB::bind_method(D_METHOD("get_filter_beta"), &GazeTracker::get_filter_beta);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "filter_beta"), "set_filter_beta", "get_filter_beta");

    ClassDB::bind_method(D_METHOD("set_filter_d_cutoff", "val"), &GazeTracker::set_filter_d_cutoff);
    ClassDB::bind_method(D_METHOD("get_filter_d_cutoff"), &GazeTracker::get_filter_d_cutoff);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "filter_d_cutoff"), "set_filter_d_cutoff", "get_filter_d_cutoff");

    ClassDB::bind_method(D_METHOD("set_yunet_model_path", "path"), &GazeTracker::set_yunet_model_path);
    ClassDB::bind_method(D_METHOD("get_yunet_model_path"), &GazeTracker::get_yunet_model_path);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "yunet_model_path", PROPERTY_HINT_FILE), "set_yunet_model_path", "get_yunet_model_path");

    ClassDB::bind_method(D_METHOD("set_gaze_onnx_path", "path"), &GazeTracker::set_gaze_onnx_path);
    ClassDB::bind_method(D_METHOD("get_gaze_onnx_path"), &GazeTracker::get_gaze_onnx_path);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "gaze_onnx_path", PROPERTY_HINT_FILE), "set_gaze_onnx_path", "get_gaze_onnx_path");

    ClassDB::bind_method(D_METHOD("set_expression_tracking_enabled", "enabled"), &GazeTracker::set_expression_tracking_enabled);
    ClassDB::bind_method(D_METHOD("get_expression_tracking_enabled"), &GazeTracker::get_expression_tracking_enabled);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "expression_tracking_enabled"), "set_expression_tracking_enabled", "get_expression_tracking_enabled");

    ClassDB::bind_method(D_METHOD("get_head_transform"), &GazeTracker::get_head_transform);
    ClassDB::bind_method(D_METHOD("get_camera_to_screen_transform"), &GazeTracker::get_camera_to_screen_transform);
    ClassDB::bind_method(D_METHOD("get_gaze_origin"), &GazeTracker::get_gaze_origin);
    ClassDB::bind_method(D_METHOD("get_gaze_direction", "apply_calibration"), &GazeTracker::get_gaze_direction, DEFVAL(true));

    ClassDB::bind_method(D_METHOD("get_raw_head_rotation"), &GazeTracker::get_raw_head_rotation);
    ClassDB::bind_method(D_METHOD("get_raw_head_translation"), &GazeTracker::get_raw_head_translation);
    ClassDB::bind_method(D_METHOD("get_head_position"), &GazeTracker::get_head_position);
    ClassDB::bind_method(D_METHOD("get_head_forward"), &GazeTracker::get_head_forward);
    ClassDB::bind_method(D_METHOD("project_gaze_ray_to_viewport", "origin", "direction"), &GazeTracker::project_gaze_ray_to_viewport);
    ClassDB::bind_method(D_METHOD("get_raw_left_eye_center"), &GazeTracker::get_raw_left_eye_center);
    ClassDB::bind_method(D_METHOD("get_raw_right_eye_center"), &GazeTracker::get_raw_right_eye_center);
    ClassDB::bind_method(D_METHOD("get_raw_gaze_direction"), &GazeTracker::get_raw_gaze_direction);

    // Enums
    BIND_ENUM_CONSTANT(LIFECYCLE_UNKNOWN);
    BIND_ENUM_CONSTANT(LIFECYCLE_PERM_REQ);
    BIND_ENUM_CONSTANT(LIFECYCLE_INITIALIZING);
    BIND_ENUM_CONSTANT(LIFECYCLE_RUNNING);
    BIND_ENUM_CONSTANT(LIFECYCLE_ERROR);

    ADD_SIGNAL(MethodInfo("gaze_updated", PropertyInfo(Variant::VECTOR2, "screen_pixel")));
    ADD_SIGNAL(MethodInfo("face_detection_changed", PropertyInfo(Variant::BOOL, "detected")));
    ADD_SIGNAL(MethodInfo("face_frame_ready"));
    ADD_SIGNAL(MethodInfo("lifecycle_changed", PropertyInfo(Variant::INT, "state")));
}

GazeTracker::GazeTracker() {
    update_projection_parameters();
}

GazeTracker::~GazeTracker() {
    stop_tracker(false);
}

void GazeTracker::_ready() {
    if (autostart) {
        initialize_tracker();
    }
}

void GazeTracker::_process(double delta) {
    if (!tracker_initialized) return;

    // Global Watchdog: check if frame updates timed out (e.g. webcam stopped, face lost without active push)
    if (is_face_tracked) {
        uint64_t current_time = Time::get_singleton()->get_ticks_msec();
        if (current_time - last_frame_time > 1000) {
            is_face_tracked = false;
            emit_signal("face_detection_changed", false);
        }
    }

    platform_process(delta);
}

void GazeTracker::set_lifecycle_state(GazeLifecycle p_state) {
    if (lifecycle_state != p_state) {
        lifecycle_state = p_state;
        emit_signal("lifecycle_changed", lifecycle_state);
    }
}

int GazeTracker::get_lifecycle_state() const {
    return (int)lifecycle_state;
}

void GazeTracker::set_autostart(bool p_autostart) {
    autostart = p_autostart;
}

bool GazeTracker::get_autostart() const {
    return autostart;
}

void GazeTracker::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_EXIT_TREE: {
            stop_tracker(true);
            break;
        }
        case NOTIFICATION_APPLICATION_FOCUS_IN:
        case NOTIFICATION_APPLICATION_RESUMED: {
            if (lifecycle_state == LIFECYCLE_PERM_REQ) {
                // Verify Android permission
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
                    on_permission_result(has_camera);
                }
            }
            break;
        }
    }
}

void GazeTracker::trigger_permission_request() {
    platform_trigger_permission_request();
}

void GazeTracker::on_permission_result(bool granted) {
    platform_on_permission_result(granted);

    if (!granted) {
        Gaze::log_error("CameraPermissionDenied");
        set_lifecycle_state(LIFECYCLE_ERROR);
        return;
    }

    // Permission verified, transition to INITIALIZING and complete setup
    set_lifecycle_state(LIFECYCLE_INITIALIZING);
    complete_initialization();
}

String GazeTracker::copy_model_to_user_dir(const String &res_path) {
    if (res_path.is_empty()) return "";
    
    // Only copy if it is a res:// virtual path
    if (!res_path.begins_with("res://")) {
        return ProjectSettings::get_singleton()->globalize_path(res_path);
    }
    
    String file_name = res_path.get_file();
    String user_path = "user://models/" + file_name;
    
    Ref<DirAccess> dir = DirAccess::open("user://");
    if (dir.is_valid() && !dir->dir_exists("models")) {
        dir->make_dir("models");
    }
    
    copy_individual_file(res_path, user_path);
    
    // If it's OpenVINO XML, we also copy the companion .bin file
    if (res_path.ends_with(".xml")) {
        String bin_res_path = res_path.replace(".xml", ".bin");
        String bin_user_path = user_path.replace(".xml", ".bin");
        copy_individual_file(bin_res_path, bin_user_path);
    }
    
    return ProjectSettings::get_singleton()->globalize_path(user_path);
}

void GazeTracker::copy_individual_file(const String &src, const String &dest) {
    bool dest_exists = FileAccess::file_exists(dest);
    uint64_t src_len = 0;
    uint64_t dest_len = 0;

    if (dest_exists) {
        Ref<FileAccess> f_dest = FileAccess::open(dest, FileAccess::READ);
        if (f_dest.is_valid()) {
            dest_len = f_dest->get_length();
        }
    }

    Ref<FileAccess> file_in = FileAccess::open(src, FileAccess::READ);
    if (file_in.is_valid()) {
        src_len = file_in->get_length();
    } else {
        Error err_in = FileAccess::get_open_error();
        Gaze::log_error("GazeTrackerCopyModelFailed_SrcInvalid", 
                        "src", src.utf8().get_data(), 
                        "err_in", (int)err_in);
        return;
    }

    if (!dest_exists || dest_len != src_len || dest_len == 0) {
        Ref<FileAccess> file_out = FileAccess::open(dest, FileAccess::WRITE);
        Error err_out = FileAccess::get_open_error();
        if (file_out.is_valid()) {
            PackedByteArray buffer = file_in->get_buffer(src_len);
            file_out->store_buffer(buffer);
            Gaze::log_info("GazeTrackerCopiedModel", 
                           "src", src.utf8().get_data(), 
                           "dest", dest.utf8().get_data(), 
                           "size", (int)src_len);
        } else {
            Gaze::log_error("GazeTrackerCopyModelFailed_DestWrite", 
                            "src", src.utf8().get_data(), 
                            "dest", dest.utf8().get_data(), 
                            "err_out", (int)err_out);
        }
    }
}

bool GazeTracker::initialize_tracker() {
    if (lifecycle_state == LIFECYCLE_RUNNING || lifecycle_state == LIFECYCLE_INITIALIZING) return true;

    if (screen_size_pixels.x <= 0 || screen_size_pixels.y <= 0) {
        Gaze::GazeVector2i plat_size = platform_get_screen_size();
        if (!plat_size) {
            screen_size_pixels = Vector2i(DEFAULT_SCREEN_SIZE_PIXELS.x, DEFAULT_SCREEN_SIZE_PIXELS.y);
            Gaze::log_warning("GazeTrackerScreenSizePixelsFallback", "width", screen_size_pixels.x, "height", screen_size_pixels.y);
        } else {
            screen_size_pixels = Vector2i(plat_size.x, plat_size.y);
        }
    }
    String pixels_src = (screen_size_pixels.x == DEFAULT_SCREEN_SIZE_PIXELS.x && screen_size_pixels.y == DEFAULT_SCREEN_SIZE_PIXELS.y) ? "default_fallback" : "platform_api";

    if (screen_size_mm.x <= 0.0 || screen_size_mm.y <= 0.0) {
        Gaze::GazeVector2 plat_size_mm = platform_get_screen_size_mm();
        if (!plat_size_mm) {
            screen_size_mm = Vector2(DEFAULT_SCREEN_SIZE_MM.x, DEFAULT_SCREEN_SIZE_MM.y);
            Gaze::log_warning("GazeTrackerScreenSizeMmFallback", "width", screen_size_mm.x, "height", screen_size_mm.y);
        } else {
            screen_size_mm = Vector2(plat_size_mm.x, plat_size_mm.y);
        }
    }
    String mm_src = (screen_size_mm.x == DEFAULT_SCREEN_SIZE_MM.x && screen_size_mm.y == DEFAULT_SCREEN_SIZE_MM.y) ? "default_fallback" : "platform_api";

    if (camera_offset.y == 148.0 && screen_size_mm.y != 296.0) {
        camera_offset.y = screen_size_mm.y * 0.5;
    }

    if (screen_size_pixels.x <= 0 || screen_size_pixels.y <= 0 ||
        screen_size_mm.x <= 0.0 || screen_size_mm.y <= 0.0) {
        Gaze::log_error("GazeTrackerInitFailed", "reason", "invalid screen geometry");
        set_lifecycle_state(LIFECYCLE_ERROR);
        return false;
    }

    Gaze::log_info("GazeTrackerGeometryInitialized",
                   "pixels_x", screen_size_pixels.x,
                   "pixels_y", screen_size_pixels.y,
                   "mm_x", screen_size_mm.x,
                   "mm_y", screen_size_mm.y,
                   "pixels_source", pixels_src.utf8().get_data(),
                   "mm_source", mm_src.utf8().get_data());

    update_projection_parameters();
    update_filter_parameters();

    if (!filter_x) filter_x = new OneEuroFilter(60.0, filter_min_cutoff, filter_beta, filter_d_cutoff);
    if (!filter_y) filter_y = new OneEuroFilter(60.0, filter_min_cutoff, filter_beta, filter_d_cutoff);

    if (calibration_resource.is_valid()) {
        projection_engine.set_calibration(calibration_resource->get_calibration());
    }

    platform_initialize();
    return true;
}


void GazeTracker::stop_tracker(bool p_emit_signal) {
    tracker_initialized = false;
    is_face_tracked = false;

    platform_terminate();

    if (filter_x) {
        delete filter_x;
        filter_x = nullptr;
    }
    if (filter_y) {
        delete filter_y;
        filter_y = nullptr;
    }

    if (p_emit_signal) {
        set_lifecycle_state(LIFECYCLE_UNKNOWN);
    } else {
        lifecycle_state = LIFECYCLE_UNKNOWN;
    }
}

void GazeTracker::calibrate_3d(Vector2 target_pixel) {
    if (!tracker_initialized) return;

    Vector2 target_scr = target_pixel + platform_get_window_position();

    Gaze::GazeCalibration new_calib = projection_engine.get_calibration();
    if (projection_engine.calibrate_3d_bias(
            latest_gaze_origin,
            latest_gaze_dir,
            Gaze::GazeVector2(target_scr.x, target_scr.y),
            new_calib)) {
        
        projection_engine.set_calibration(new_calib);
        if (calibration_resource.is_valid()) {
            calibration_resource->set_calibration(new_calib);
        }
        Gaze::log_info("Calibrate3DSuccess", "bias_pitch", new_calib.bias_pitch, "bias_yaw", new_calib.bias_yaw);
    }
}

void GazeTracker::calibrate_2d(Vector2 target_pixel) {
    if (!tracker_initialized) return;

    Vector2 target_scr = target_pixel + platform_get_window_position();

    Gaze::GazeCalibration new_calib = projection_engine.get_calibration();
    if (projection_engine.calibrate_2d_bias(
            latest_gaze_origin,
            latest_gaze_dir,
            Gaze::GazeVector2(target_scr.x, target_scr.y),
            new_calib)) {
        
        projection_engine.set_calibration(new_calib);
        if (calibration_resource.is_valid()) {
            calibration_resource->set_calibration(new_calib);
        }
        Gaze::log_info("Calibrate2DSuccess", "bias_x", new_calib.bias_pixel_x, "bias_y", new_calib.bias_pixel_y);
    }
}

void GazeTracker::clear_calibration() {
    Gaze::GazeCalibration empty_calib;
    projection_engine.set_calibration(empty_calib);
    if (calibration_resource.is_valid()) {
        calibration_resource->set_calibration(empty_calib);
    }
    Gaze::log_info("CalibrationCleared");
}

void GazeTracker::feed_gaze(bool face_detected, Vector3 origin, Vector3 direction) {
    last_frame_time = Time::get_singleton()->get_ticks_msec();

    bool state_changed = (face_detected != is_face_tracked);
    is_face_tracked = face_detected;

    if (state_changed) {
        emit_signal("face_detection_changed", is_face_tracked);
    }

    if (face_detected) {
        latest_gaze_origin = Gaze::GazeVector3(origin.x, origin.y, origin.z);
        latest_gaze_dir = Gaze::GazeVector3(direction.x, direction.y, direction.z);
        emit_signal("face_frame_ready");
    } else {
        latest_crops.face_detected = false;
    }
}

void GazeTracker::update_projection_parameters() {
    Gaze::CameraPlacement placement(
        Gaze::GazeVector3(camera_offset.x, camera_offset.y, camera_offset.z),
        camera_tilt
    );
    projection_engine.set_camera_placement(placement);
    projection_engine.set_screen_size_pixels(Gaze::GazeVector2(screen_size_pixels.x, screen_size_pixels.y));
    projection_engine.set_screen_size_mm(Gaze::GazeVector2(screen_size_mm.x, screen_size_mm.y));
    projection_engine.set_camera_focal_length_px(camera_focal_length_px);
    if (pipeline) {
        pipeline->set_camera_focal_length_px(camera_focal_length_px);
    }
}

void GazeTracker::update_filter_parameters() {
    if (filter_x) {
        filter_x->set_min_cutoff(filter_min_cutoff);
        filter_x->set_beta(filter_beta);
        filter_x->set_d_cutoff(filter_d_cutoff);
    }
    if (filter_y) {
        filter_y->set_min_cutoff(filter_min_cutoff);
        filter_y->set_beta(filter_beta);
        filter_y->set_d_cutoff(filter_d_cutoff);
    }
}

// Getters / Setters property bindings
void GazeTracker::set_calibration_resource(const Ref<GazeCalibrationResource>& res) {
    calibration_resource = res;
    if (calibration_resource.is_valid()) {
        projection_engine.set_calibration(calibration_resource->get_calibration());
    }
}

Ref<GazeCalibrationResource> GazeTracker::get_calibration_resource() const {
    return calibration_resource;
}

void GazeTracker::set_camera_offset(Vector3 offset) {
    camera_offset = offset;
    update_projection_parameters();
}

Vector3 GazeTracker::get_camera_offset() const {
    return camera_offset;
}

void GazeTracker::set_camera_tilt(double tilt) {
    camera_tilt = tilt;
    update_projection_parameters();
}

double GazeTracker::get_camera_tilt() const {
    return camera_tilt;
}

void GazeTracker::set_screen_size_pixels(Vector2i size) {
    screen_size_pixels = size;
    update_projection_parameters();
}

Vector2i GazeTracker::get_screen_size_pixels() const {
    return screen_size_pixels;
}

void GazeTracker::set_screen_size_mm(Vector2 size) {
    screen_size_mm = size;
    update_projection_parameters();
}

Vector2 GazeTracker::get_screen_size_mm() const {
    return screen_size_mm;
}

void GazeTracker::set_camera_focal_length_px(double f) {
    camera_focal_length_px = f;
    update_projection_parameters();
}

double GazeTracker::get_camera_focal_length_px() const {
    return camera_focal_length_px;
}

void GazeTracker::set_camera_device_id(int id) {
    camera_device_id = id;
}

int GazeTracker::get_camera_device_id() const {
    return camera_device_id;
}

void GazeTracker::set_filter_min_cutoff(double val) {
    filter_min_cutoff = val;
    update_filter_parameters();
}

double GazeTracker::get_filter_min_cutoff() const {
    return filter_min_cutoff;
}

void GazeTracker::set_filter_beta(double val) {
    filter_beta = val;
    update_filter_parameters();
}

double GazeTracker::get_filter_beta() const {
    return filter_beta;
}

void GazeTracker::set_filter_d_cutoff(double val) {
    filter_d_cutoff = val;
    update_filter_parameters();
}

double GazeTracker::get_filter_d_cutoff() const {
    return filter_d_cutoff;
}

void GazeTracker::set_yunet_model_path(String path) {
    yunet_model_path = path;
}

String GazeTracker::get_yunet_model_path() const {
    return yunet_model_path;
}

void GazeTracker::set_gaze_onnx_path(String path) {
    gaze_onnx_path = path;
}

String GazeTracker::get_gaze_onnx_path() const {
    return gaze_onnx_path;
}

void GazeTracker::set_expression_tracking_enabled(bool enabled) {
    expression_tracking_enabled = enabled;
}

bool GazeTracker::get_expression_tracking_enabled() const {
    return expression_tracking_enabled;
}

Transform3D GazeTracker::get_head_transform() const {
    if (!is_face_tracked) {
        return Transform3D();
    }
    Gaze::GazeTransform3D gt = projection_engine.get_head_transform_in_camera_space(
        latest_crops.head_pose_translation,
        latest_crops.head_pose_rotation
    );

    Basis basis(
        Vector3(gt.basis.x.x, gt.basis.x.y, gt.basis.x.z),
        Vector3(gt.basis.y.x, gt.basis.y.y, gt.basis.y.z),
        Vector3(gt.basis.z.x, gt.basis.z.y, gt.basis.z.z)
    );

    Vector3 translation(gt.origin.x, gt.origin.y, gt.origin.z);

    return Transform3D(basis, translation);
}

Transform3D GazeTracker::get_camera_to_screen_transform() const {
    double scale_x = screen_size_pixels.x / screen_size_mm.x;
    double scale_y = -screen_size_pixels.y / screen_size_mm.y;
    double W_half = screen_size_pixels.x / 2.0;
    double H_half = screen_size_pixels.y / 2.0;

    double theta_rad = camera_tilt * Gaze::DEG_TO_RAD;
    double cos_t = std::cos(theta_rad);
    double sin_t = std::sin(theta_rad);

    Basis basis(
        Vector3(-scale_x, 0.0, 0.0),
        Vector3(0.0, cos_t * scale_y, sin_t),
        Vector3(0.0, sin_t * scale_y, -cos_t)
    );

    double Cx = camera_offset.x;
    double Cy = camera_offset.y;
    double Cz = camera_offset.z;

    Vector2 window_pos = platform_get_window_position();
    Vector3 translation(Cx * scale_x + W_half - window_pos.x, Cy * scale_y + H_half - window_pos.y, Cz);

    Viewport* vp = const_cast<GazeTracker*>(this)->get_viewport();
    if (vp) {
        Transform2D final_xform = vp->get_final_transform().affine_inverse();
        
        Vector2 col0_xy(basis[0].x, basis[0].y);
        col0_xy = final_xform.basis_xform(col0_xy);
        basis[0].x = col0_xy.x;
        basis[0].y = col0_xy.y;
        
        Vector2 col1_xy(basis[1].x, basis[1].y);
        col1_xy = final_xform.basis_xform(col1_xy);
        basis[1].x = col1_xy.x;
        basis[1].y = col1_xy.y;
        
        Vector2 trans_xy(translation.x, translation.y);
        trans_xy = final_xform.xform(trans_xy);
        translation.x = trans_xy.x;
        translation.y = trans_xy.y;
    }

    return Transform3D(basis, translation);
}

Vector3 GazeTracker::get_head_position() const {
    if (!is_face_tracked) {
        return Vector3(0.0, 0.0, 0.0);
    }
    Gaze::GazeTransform3D gt = projection_engine.get_head_transform_in_camera_space(
        latest_crops.head_pose_translation,
        latest_crops.head_pose_rotation
    );
    return Vector3(gt.origin.x, gt.origin.y, gt.origin.z);
}

Vector3 GazeTracker::get_head_forward() const {
    if (!is_face_tracked) {
        return Vector3(0.0, 0.0, 1.0); // Facing straight toward screen (+Z)
    }
    Gaze::GazeTransform3D gt = projection_engine.get_head_transform_in_camera_space(
        latest_crops.head_pose_translation,
        latest_crops.head_pose_rotation
    );
    Gaze::GazeVector3 forward = gt.basis.multiply_vector(Gaze::GazeVector3(0, 0, -1));
    return Vector3(forward.x, forward.y, forward.z);
}

Vector2 GazeTracker::project_gaze_ray_to_viewport(Vector3 origin, Vector3 direction) const {
    Gaze::GazeVector3 origin_cam(origin.x, origin.y, origin.z);
    Gaze::GazeVector3 dir_cam(direction.x, direction.y, direction.z);
    Gaze::GazeVector2 pixel;
    if (projection_engine.project_gaze(origin_cam, dir_cam, pixel)) {
        Vector2 window_pos = platform_get_window_position();
        Vector2 local_pos(pixel.x - window_pos.x, pixel.y - window_pos.y);
        Viewport* vp = const_cast<GazeTracker*>(this)->get_viewport();
        if (vp) {
            local_pos = vp->get_final_transform().affine_inverse().xform(local_pos);
        }
        return local_pos;
    }
    // Return unattainable infinity if the ray doesn't intersect or points away
    return Vector2(INFINITY, INFINITY);
}

Vector3 GazeTracker::get_gaze_origin() const {
    if (!is_face_tracked) {
        return Vector3(0.0, 0.0, 0.0);
    }
    return Vector3(latest_gaze_origin.x, latest_gaze_origin.y, latest_gaze_origin.z);
}

Vector3 GazeTracker::get_gaze_direction(bool apply_calibration) const {
    if (!is_face_tracked) {
        return Vector3(0.0, 0.0, -1.0);
    }
    Gaze::GazeVector3 dir = apply_calibration ? projection_engine.apply_3d_bias(latest_gaze_dir) : latest_gaze_dir;
    return Vector3(dir.x, dir.y, dir.z);
}

Vector3 GazeTracker::get_raw_head_rotation() const {
    if (!is_face_tracked) {
        return Vector3(0.0, 0.0, 0.0);
    }
    Gaze::GazeTransform3D gt = projection_engine.get_head_transform_in_camera_space(
        latest_crops.head_pose_translation,
        latest_crops.head_pose_rotation
    );
    Gaze::GazeVector3 euler = gt.basis.get_euler_deg();
    return Vector3(euler.x, euler.y, euler.z);
}

Vector3 GazeTracker::get_raw_head_translation() const {
    return get_head_position();
}

Vector3 GazeTracker::get_raw_left_eye_center() const {
    return Vector3(latest_crops.left_eye_center_cam.x, latest_crops.left_eye_center_cam.y, latest_crops.left_eye_center_cam.z);
}

Vector3 GazeTracker::get_raw_right_eye_center() const {
    return Vector3(latest_crops.right_eye_center_cam.x, latest_crops.right_eye_center_cam.y, latest_crops.right_eye_center_cam.z);
}

Vector3 GazeTracker::get_raw_gaze_direction() const {
    return Vector3(latest_gaze_dir.x, latest_gaze_dir.y, latest_gaze_dir.z);
}

void GazeTracker::set_pipeline_config(const Ref<GazePipelineConfig>& res) {
    pipeline_config = res;
    update_pipeline_config();
}

Ref<GazePipelineConfig> GazeTracker::get_pipeline_config() const {
    return pipeline_config;
}

void GazeTracker::update_pipeline_config() {
    if (pipeline_config.is_valid()) {
        Gaze::PipelineConfig core_cfg = pipeline_config->get_config();
        if (pipeline) {
            pipeline->set_config(core_cfg);
        }
        if (model) {
            model->set_config(core_cfg);
        }
    }
}

} // namespace godot

