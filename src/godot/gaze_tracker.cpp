#include "gaze_tracker.hpp"
#include "screen_projector.hpp"
#include "log.hpp"
#include "camera_sensor.hpp"
#include "face_estimator.hpp"
#include "eye_estimator.hpp"
#include "smoother.hpp"
#include "one_euro_smoother.hpp"
#include "display_profile.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#ifdef WEB_ENABLED
#include <godot_cpp/classes/java_script_bridge.hpp>
#endif

// Platform-specific headers are included in gaze_tracker_native.cpp and gaze_tracker_web.cpp


namespace godot {

static Ref<GazeCalibration> get_default_calibration() {
    GazeDeviceEstimatedCalibration* sing = Object::cast_to<GazeDeviceEstimatedCalibration>(Engine::get_singleton()->get_singleton("GazeDeviceEstimatedCalibration"));
    if (sing) {
        return sing->get_calibration();
    }
    return Ref<GazeCalibration>();
}

void GazeTracker::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("initialize_tracker"), &GazeTracker::initialize_tracker);
    ClassDB::bind_method(D_METHOD("stop_tracker", "emit_signal"), &GazeTracker::stop_tracker, DEFVAL(true));
    ClassDB::bind_method(D_METHOD("complete_initialization"), &GazeTracker::complete_initialization);
    ClassDB::bind_method(D_METHOD("trigger_permission_request"), &GazeTracker::trigger_permission_request);
    ClassDB::bind_method(D_METHOD("on_permission_result", "granted"), &GazeTracker::on_permission_result);
    ClassDB::bind_method(D_METHOD("clear_calibration"), &GazeTracker::clear_calibration);
    ClassDB::bind_method(D_METHOD("map_viewport_to_screen", "logical_pixel"), &GazeTracker::map_viewport_to_screen);
    ClassDB::bind_method(D_METHOD("filter_gaze_coordinate", "raw"), &GazeTracker::filter_gaze_coordinate);
    ClassDB::bind_method(D_METHOD("feed_gaze", "face_detected", "origin", "direction"), &GazeTracker::feed_gaze);
    ClassDB::bind_method(D_METHOD("feed_gaze_web_raw", "args"), &GazeTracker::feed_gaze_web_raw);
    ClassDB::bind_method(D_METHOD("on_sidecar_ready", "args"), &GazeTracker::on_sidecar_ready);
    ClassDB::bind_method(D_METHOD("get_camera_sensor"), &GazeTracker::get_camera_sensor);
    ClassDB::bind_method(D_METHOD("get_face_estimator"), &GazeTracker::get_face_estimator);
    ClassDB::bind_method(D_METHOD("get_eye_estimator"), &GazeTracker::get_eye_estimator);
    ClassDB::bind_method(D_METHOD("get_screen_smooth"), &GazeTracker::get_screen_smooth);
    ClassDB::bind_method(D_METHOD("set_screen_smooth", "smoother"), &GazeTracker::set_screen_smooth);

    ClassDB::bind_method(D_METHOD("get_latest_projected_gaze"), &GazeTracker::get_latest_projected_gaze);
    ClassDB::bind_method(D_METHOD("get_latest_filtered_gaze"), &GazeTracker::get_latest_filtered_gaze);
    ClassDB::bind_method(D_METHOD("is_face_detected"), &GazeTracker::is_face_detected);

    ClassDB::bind_method(D_METHOD("get_derived_camera_offset"), &GazeTracker::get_derived_camera_offset);
    ClassDB::bind_method(D_METHOD("get_derived_camera_tilt"), &GazeTracker::get_derived_camera_tilt);

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
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "calibration_resource", PROPERTY_HINT_RESOURCE_TYPE, "GazeCalibration"), "set_calibration_resource", "get_calibration_resource");

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "screen_smooth", PROPERTY_HINT_RESOURCE_TYPE, "Smoother"), "set_screen_smooth", "get_screen_smooth");

    ClassDB::bind_method(D_METHOD("set_display_profile", "profile"), &GazeTracker::set_display_profile);
    ClassDB::bind_method(D_METHOD("get_display_profile"), &GazeTracker::get_display_profile);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "display_profile", PROPERTY_HINT_RESOURCE_TYPE, "DisplayProfile"), "set_display_profile", "get_display_profile");

    ClassDB::bind_method(D_METHOD("set_camera_device_id", "id"), &GazeTracker::set_camera_device_id);
    ClassDB::bind_method(D_METHOD("get_camera_device_id"), &GazeTracker::get_camera_device_id);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "camera_device_id"), "set_camera_device_id", "get_camera_device_id");

    ClassDB::bind_method(D_METHOD("set_debug_logging_frames", "frames"), &GazeTracker::set_debug_logging_frames);
    ClassDB::bind_method(D_METHOD("get_debug_logging_frames"), &GazeTracker::get_debug_logging_frames);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "debug_logging_frames"), "set_debug_logging_frames", "get_debug_logging_frames");

    ClassDB::bind_method(D_METHOD("set_window_position_override", "pos"), &GazeTracker::set_window_position_override);
    ClassDB::bind_method(D_METHOD("get_window_position_override"), &GazeTracker::get_window_position_override);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "window_position_override"), "set_window_position_override", "get_window_position_override");

    ClassDB::bind_method(D_METHOD("get_head_transform"), &GazeTracker::get_head_transform);
    ClassDB::bind_method(D_METHOD("get_camera_to_screen_transform"), &GazeTracker::get_camera_to_screen_transform);
    ClassDB::bind_method(D_METHOD("get_gaze_origin"), &GazeTracker::get_gaze_origin);
    ClassDB::bind_method(D_METHOD("get_gaze_direction", "apply_calibration"), &GazeTracker::get_gaze_direction, DEFVAL(true));

    ClassDB::bind_method(D_METHOD("get_head_rotation_opencv_space"), &GazeTracker::get_head_rotation_opencv_space);
    ClassDB::bind_method(D_METHOD("get_head_translation_opencv_space"), &GazeTracker::get_head_translation_opencv_space);
    ClassDB::bind_method(D_METHOD("get_head_position"), &GazeTracker::get_head_position);
    ClassDB::bind_method(D_METHOD("get_head_forward"), &GazeTracker::get_head_forward);
    ClassDB::bind_method(D_METHOD("project_gaze_ray_to_viewport", "origin", "direction", "apply_calibration"), &GazeTracker::project_gaze_ray_to_viewport, DEFVAL(true));
    ClassDB::bind_method(D_METHOD("get_left_eye_center_opencv_space"), &GazeTracker::get_left_eye_center_opencv_space);
    ClassDB::bind_method(D_METHOD("get_right_eye_center_opencv_space"), &GazeTracker::get_right_eye_center_opencv_space);
    ClassDB::bind_method(D_METHOD("get_gaze_direction_opencv_space"), &GazeTracker::get_gaze_direction_opencv_space);

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
}

void GazeTracker::_ready() {
    if (autostart && !Engine::get_singleton()->is_editor_hint()) {
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

std::vector<uint8_t> GazeTracker::load_file_buffer(const String &path) {
    std::vector<uint8_t> buffer;
    if (path.is_empty()) {
        return buffer;
    }
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (file.is_null()) {
        Gaze::log_error("GazeTrackerLoadFileBufferFailed_Open", "path", path.utf8().get_data());
        return buffer;
    }
    uint64_t length = file->get_length();
    if (length == 0) {
        Gaze::log_error("GazeTrackerLoadFileBufferFailed_Empty", "path", path.utf8().get_data());
        return buffer;
    }
    PackedByteArray godot_buffer = file->get_buffer(length);
    buffer.resize(length);
    std::memcpy(buffer.data(), godot_buffer.ptr(), length);
    return buffer;
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

    // Hierarchy discovery / dynamic instantiation
    camera_sensor = Object::cast_to<CameraSensor>(get_node_or_null("CameraSensor"));
    if (!camera_sensor) {
        camera_sensor = memnew(CameraSensor);
        camera_sensor->set_name("CameraSensor");
        add_child(camera_sensor);
    }

    if (camera_sensor) {
        face_estimator = Object::cast_to<FaceEstimator>(camera_sensor->get_node_or_null("FaceEstimator"));
        if (!face_estimator) {
            face_estimator = memnew(FaceEstimator);
            face_estimator->set_name("FaceEstimator");
            camera_sensor->add_child(face_estimator);
        }
    }

    if (face_estimator) {
        eye_estimator = Object::cast_to<EyeEstimator>(face_estimator->get_node_or_null("EyeEstimator"));
        if (!eye_estimator) {
            eye_estimator = memnew(EyeEstimator);
            eye_estimator->set_name("EyeEstimator");
            face_estimator->add_child(eye_estimator);
        }
    }

    // Initialize DisplayProfile Resource
    if (display_profile.is_null()) {
        display_profile = DisplayProfile::estimate_from_os();
    } else {
        if (display_profile->get_logical_size_px().x <= 0 || display_profile->get_logical_size_px().y <= 0 ||
            display_profile->get_physical_size_mm().x <= 0.0 || display_profile->get_physical_size_mm().y <= 0.0) {
            Ref<DisplayProfile> default_prof = DisplayProfile::estimate_from_os();
            if (display_profile->get_logical_size_px().x <= 0 || display_profile->get_logical_size_px().y <= 0) {
                display_profile->set_logical_size_px(default_prof->get_logical_size_px());
            }
            if (display_profile->get_physical_size_mm().x <= 0.0 || display_profile->get_physical_size_mm().y <= 0.0) {
                display_profile->set_physical_size_mm(default_prof->get_physical_size_mm());
            }
        }
    }

    // Initialize Smoother Resource
    if (screen_smooth.is_null()) {
        Ref<OneEuroSmoother> default_smoother;
        default_smoother.instantiate();
        screen_smooth = default_smoother;
    }
    smoother_state = screen_smooth->_smoother_init();

    // Default CameraSensor position if unconfigured
    if (camera_sensor && camera_sensor->get_position() == Vector3(0.0, 0.0, 0.0)) {
        camera_sensor->set_position(Vector3(0.0, display_profile->get_physical_size_mm().y * 0.5, 0.0));
    }

    platform_initialize();

    if (display_profile->get_logical_size_px().x <= 0 || display_profile->get_logical_size_px().y <= 0 ||
        display_profile->get_physical_size_mm().x <= 0.0 || display_profile->get_physical_size_mm().y <= 0.0) {
        Gaze::log_error("GazeTrackerInitFailed", "reason", "invalid screen geometry");
        set_lifecycle_state(LIFECYCLE_ERROR);
        return false;
    }

    Gaze::log_info("GazeTrackerGeometryInitialized",
                   "pixels_x", display_profile->get_logical_size_px().x,
                   "pixels_y", display_profile->get_logical_size_px().y,
                   "mm_x", display_profile->get_physical_size_mm().x,
                   "mm_y", display_profile->get_physical_size_mm().y);

    update_projection_parameters();

    return true;
}


void GazeTracker::stop_tracker(bool p_emit_signal) {
    tracker_initialized = false;
    is_face_tracked = false;

    platform_terminate();

    if (camera_sensor && ObjectDB::get_instance(camera_sensor->get_instance_id())) {
        camera_sensor->stop_sensor();
    }
    if (face_estimator && ObjectDB::get_instance(face_estimator->get_instance_id())) {
        face_estimator->stop_estimator();
    }
    if (eye_estimator && ObjectDB::get_instance(eye_estimator->get_instance_id())) {
        eye_estimator->stop_estimator();
    }
    if (screen_smooth.is_valid()) {
        smoother_state = screen_smooth->_smoother_init();
    }

    if (p_emit_signal) {
        set_lifecycle_state(LIFECYCLE_UNKNOWN);
    } else {
        lifecycle_state = LIFECYCLE_UNKNOWN;
    }
}

void GazeTracker::clear_calibration() {
    calibration_resource.unref();
    update_projection_parameters();
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

        Vector2 raw_proj = project_gaze_ray_to_viewport(origin, direction);
        if (raw_proj.x != INFINITY && raw_proj.y != INFINITY) {
            latest_projected_gaze_px = raw_proj;
            latest_filtered_gaze_px = filter_gaze_coordinate(raw_proj);
            emit_signal("gaze_updated", latest_filtered_gaze_px);
        }
    } else {
        latest_crops.face_detected = false;
        latest_projected_gaze_px = Vector2();
        latest_filtered_gaze_px = Vector2();
    }
}

void GazeTracker::update_projection_parameters() {
    Vector2i size_px = Vector2i(DEFAULT_SCREEN_SIZE_PIXELS.x, DEFAULT_SCREEN_SIZE_PIXELS.y);
    Vector2 size_mm = Vector2(DEFAULT_SCREEN_SIZE_MM.x, DEFAULT_SCREEN_SIZE_MM.y);

    if (display_profile.is_valid()) {
        size_px = display_profile->get_logical_size_px();
        size_mm = display_profile->get_physical_size_mm();
    }

    Ref<GazeCalibration> cal = calibration_resource;
    if (cal.is_null()) {
        cal = get_default_calibration();
    }

    Vector3 cam_off = get_derived_camera_offset();
    double cam_tilt = get_derived_camera_tilt();

    if (cal.is_valid()) {
        cam_off = cal->get_camera_offset(const_cast<GazeTracker*>(this));
        cam_tilt = cal->get_camera_tilt(const_cast<GazeTracker*>(this));
    }

    Gaze::CameraPlacement placement(
        Gaze::GazeVector3(cam_off.x, cam_off.y, cam_off.z),
        cam_tilt
    );
    projection_engine.set_camera_placement(placement);

    projection_engine.set_screen_size_pixels(Gaze::GazeVector2(size_px.x, size_px.y));
    projection_engine.set_screen_size_mm(Gaze::GazeVector2(size_mm.x, size_mm.y));

    Gaze::GazeCalibration c;
    if (cal.is_valid()) {
        c.bias_pitch = cal->get_bias_pitch();
        c.bias_yaw = cal->get_bias_yaw();
        if (cal->get_impl()) {
            c.scale_yaw = cal->get_impl()->get_scale_yaw();
            c.scale_pitch = cal->get_impl()->get_scale_pitch();
        }
        c.bias_pixel_x = cal->get_bias_pixel_x();
        c.bias_pixel_y = cal->get_bias_pixel_y();
    }
    projection_engine.set_calibration(c);

    double f = camera_sensor ? camera_sensor->get_focal_length() : 1000.0;
    projection_engine.set_camera_focal_length_px(f);
    if (face_estimator) {
        face_estimator->set_focal_length(f);
    }
}

// Getters / Setters property bindings
void GazeTracker::set_calibration_resource(const Ref<GazeCalibration>& res) {
    calibration_resource = res;
    update_projection_parameters();
}

Ref<GazeCalibration> GazeTracker::get_calibration_resource() const {
    return calibration_resource;
}

void GazeTracker::set_display_profile(const Ref<DisplayProfile>& profile) {
    display_profile = profile;
    update_projection_parameters();
}

Ref<DisplayProfile> GazeTracker::get_display_profile() const {
    return display_profile;
}

Vector3 GazeTracker::get_derived_camera_offset() const {
    if (camera_sensor && camera_sensor->is_inside_tree()) {
        Transform3D relative_transform = get_global_transform().affine_inverse() * camera_sensor->get_global_transform();
        return relative_transform.origin;
    }
    if (camera_sensor) {
        return camera_sensor->get_position();
    }
    if (display_profile.is_valid()) {
        return Vector3(0.0, display_profile->get_physical_size_mm().y * 0.5, 0.0);
    }
    return Vector3(0.0, 107.5, 0.0);
}

double GazeTracker::get_derived_camera_tilt() const {
    if (camera_sensor && camera_sensor->is_inside_tree()) {
        Transform3D relative_transform = get_global_transform().affine_inverse() * camera_sensor->get_global_transform();
        return relative_transform.basis.get_euler().x * (180.0 / M_PI);
    }
    if (camera_sensor) {
        return camera_sensor->get_rotation().x * (180.0 / M_PI);
    }
    return 0.0;
}

Vector2 GazeTracker::get_pixel_size_mm() const {
    if (display_profile.is_null()) {
        return Vector2(
            DEFAULT_SCREEN_SIZE_MM.x / DEFAULT_SCREEN_SIZE_PIXELS.x,
            DEFAULT_SCREEN_SIZE_MM.y / DEFAULT_SCREEN_SIZE_PIXELS.y
        );
    }
    Vector2i size_px = display_profile->get_logical_size_px();
    Vector2 size_mm = display_profile->get_physical_size_mm();
    if (size_px.x <= 0 || size_px.y <= 0) {
        return Vector2(0.0, 0.0);
    }
    return Vector2(size_mm.x / size_px.x, size_mm.y / size_px.y);
}

void GazeTracker::set_camera_device_id(int id) {
    camera_device_id = id;
}

int GazeTracker::get_camera_device_id() const {
    return camera_device_id;
}

void GazeTracker::set_window_position_override(Vector2 pos) {
    window_position_override = pos;
    update_projection_parameters();
}

Vector2 GazeTracker::get_window_position_override() const {
    return window_position_override;
}

void GazeTracker::set_debug_logging_frames(int frames) {
    debug_logging_frames = frames;
    debug_log_frame_counter = 0;
}

int GazeTracker::get_debug_logging_frames() const {
    return debug_logging_frames;
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
    if (display_profile.is_null()) {
        const_cast<GazeTracker*>(this)->display_profile.instantiate();
    }
    PlatformGeometry geom = platform_get_geometry();
    double scale_x = (double)display_profile->get_logical_size_px().x / display_profile->get_physical_size_mm().x;
    double scale_y = -(double)display_profile->get_logical_size_px().y / display_profile->get_physical_size_mm().y;
    double W_half = (double)display_profile->get_logical_size_px().x / 2.0;
    double H_half = (double)display_profile->get_logical_size_px().y / 2.0;

    double theta_rad = get_derived_camera_tilt() * (M_PI / 180.0);
    double cos_t = std::cos(theta_rad);
    double sin_t = std::sin(theta_rad);

    Basis basis(
        Vector3(-scale_x, 0.0, 0.0),
        Vector3(0.0, cos_t * scale_y, sin_t),
        Vector3(0.0, sin_t * scale_y, -cos_t)
    );

    Vector3 cam_off = get_derived_camera_offset();
    double Cx = cam_off.x;
    double Cy = cam_off.y;
    double Cz = cam_off.z;

    Vector2 window_pos = geom.window_position_px;
    Vector3 translation(Cx * scale_x + W_half - window_pos.x, Cy * scale_y + H_half - window_pos.y, Cz);

    Viewport* vp = const_cast<GazeTracker*>(this)->get_viewport();
    if (vp) {
        Transform2D final_xform = get_adjusted_viewport_transform().affine_inverse();
        
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

Vector2 GazeTracker::project_gaze_ray_to_viewport(Vector3 origin, Vector3 direction, bool apply_calibration) const {
    const_cast<GazeTracker*>(this)->update_projection_parameters();
    Gaze::GazeVector3 origin_cam(origin.x, origin.y, origin.z);
    Gaze::GazeVector3 dir_cam(direction.x, direction.y, direction.z);

    if (apply_calibration) {
        dir_cam = projection_engine.apply_3d_bias(dir_cam);
    }

    PlatformGeometry geom = platform_get_geometry();
    Transform2D vp_xform = get_adjusted_viewport_transform();

    double scale = 1.0;
    DisplayServer *ds = DisplayServer::get_singleton();
    if (ds) {
        scale = ds->screen_get_scale(ds->window_get_current_screen());
    }

    Gaze::ScreenProjector projector = Gaze::ScreenProjector::from_godot_geometry(
        Gaze::GazeVector2(geom.window_position_px.x, geom.window_position_px.y),
        Gaze::GazeVector2(vp_xform.get_scale().x / scale, vp_xform.get_scale().y / scale),
        Gaze::GazeVector2(vp_xform.get_origin().x / scale, vp_xform.get_origin().y / scale)
    );

    Gaze::GazeVector2 local_pixel;
    if (projector.project_to_viewport(projection_engine, origin_cam, dir_cam, local_pixel)) {
        double px = local_pixel.x;
        double py = local_pixel.y;
        if (apply_calibration) {
            px += projection_engine.get_calibration().bias_pixel_x;
            py += projection_engine.get_calibration().bias_pixel_y;
        }
        Vector2 out_pixel(px, py);
        if (log_this_frame) {
            UtilityFunctions::print("[GazeTracker Debug] window_pos: ", geom.window_position_px,
                                    " | viewport_scale: ", vp_xform.get_scale(),
                                    " | projected_viewport_pixel: ", out_pixel);
        }
        return out_pixel;
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

Vector3 GazeTracker::get_head_rotation_opencv_space() const {
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

Vector3 GazeTracker::get_head_translation_opencv_space() const {
    return get_head_position();
}

Vector3 GazeTracker::get_left_eye_center_opencv_space() const {
    return Vector3(latest_crops.left_eye_center_cam.x, latest_crops.left_eye_center_cam.y, latest_crops.left_eye_center_cam.z);
}

Vector3 GazeTracker::get_right_eye_center_opencv_space() const {
    return Vector3(latest_crops.right_eye_center_cam.x, latest_crops.right_eye_center_cam.y, latest_crops.right_eye_center_cam.z);
}

Vector3 GazeTracker::get_gaze_direction_opencv_space() const {
    return Vector3(latest_gaze_dir.x, latest_gaze_dir.y, latest_gaze_dir.z);
}

CameraSensor* GazeTracker::get_camera_sensor() const { return camera_sensor; }
FaceEstimator* GazeTracker::get_face_estimator() const { return face_estimator; }
EyeEstimator* GazeTracker::get_eye_estimator() const { return eye_estimator; }

void GazeTracker::set_screen_smooth(const Ref<Smoother>& smoother) {
    screen_smooth = smoother;
    if (screen_smooth.is_valid()) {
        smoother_state = screen_smooth->_smoother_init();
    }
}

Ref<Smoother> GazeTracker::get_screen_smooth() const {
    return screen_smooth;
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
        if (camera_sensor) {
            camera_sensor->set_resolution(core_cfg.desired_camera_width, core_cfg.desired_camera_height);
        }
        if (face_estimator) {
            face_estimator->set_pipeline_config(core_cfg);
        }
        if (eye_estimator) {
            eye_estimator->set_pipeline_config(core_cfg);
        }
    }
}

Transform2D GazeTracker::get_adjusted_viewport_transform() const {
    Viewport* vp = const_cast<GazeTracker*>(this)->get_viewport();
    if (!vp) {
        return Transform2D();
    }
    return vp->get_final_transform();
}

Vector2 GazeTracker::map_viewport_to_screen(Vector2 logical_pixel) const {
    PlatformGeometry geom = platform_get_geometry();
    Transform2D vp_xform = get_adjusted_viewport_transform();

    double scale = 1.0;
    DisplayServer *ds = DisplayServer::get_singleton();
    if (ds) {
        scale = ds->screen_get_scale(ds->window_get_current_screen());
    }

    Gaze::ScreenProjector projector = Gaze::ScreenProjector::from_godot_geometry(
        Gaze::GazeVector2(geom.window_position_px.x, geom.window_position_px.y),
        Gaze::GazeVector2(vp_xform.get_scale().x / scale, vp_xform.get_scale().y / scale),
        Gaze::GazeVector2(vp_xform.get_origin().x / scale, vp_xform.get_origin().y / scale)
    );

    Gaze::GazeVector2 screen_px = projector.map_viewport_to_screen_px(Gaze::GazeVector2(logical_pixel.x, logical_pixel.y));
    return Vector2(screen_px.x, screen_px.y);
}

Vector2 GazeTracker::filter_gaze_coordinate(Vector2 raw) {
    if (screen_smooth.is_null() || smoother_state.is_empty()) {
        if (log_this_frame) {
            UtilityFunctions::print("[GazeTracker Filter Debug] Smoother is null/empty! Returning raw: ", raw);
        }
        return raw;
    }
    double tstamp = (double)Time::get_singleton()->get_ticks_msec() / 1000.0;
    Variant filtered = screen_smooth->_smoother_next(smoother_state, tstamp, raw);
    Vector2 out = filtered;
    if (log_this_frame) {
        UtilityFunctions::print("[GazeTracker Filter Debug] raw: ", raw, " | filtered: ", out);
    }
    return out;
}

} // namespace godot

