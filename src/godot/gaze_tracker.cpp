#include "gaze_tracker.hpp"
#include "godot_files.hpp"
#include "screen_projector.hpp"
#include "log.hpp"

namespace Gaze { extern bool g_is_exiting; }
#include "camera_sensor.hpp"
#include "face_estimator.hpp"
#include "eye_estimator.hpp"
#include "smoother.hpp"
#include "one_euro_smoother.hpp"
#include "display_profile.hpp"
#include "vision_server.hpp"
#include "gaze_server.hpp"


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
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/callable.hpp>

#ifdef WEB_ENABLED
#include <godot_cpp/classes/java_script_object.hpp>
#include <godot_cpp/classes/java_script_bridge.hpp>
#endif

namespace godot {

static Ref<DeviceCalibration> get_default_device_calibration() {
    GazeDeviceEstimatedCalibration* sing = Object::cast_to<GazeDeviceEstimatedCalibration>(Engine::get_singleton()->get_singleton("GazeDeviceEstimatedCalibration"));
    if (sing) {
        return sing->get_calibration();
    }
    return Ref<DeviceCalibration>();
}

void GazeTracker::_bind_methods() {
    ClassDB::bind_method(D_METHOD("initialize_tracker"), &GazeTracker::initialize_tracker);
    ClassDB::bind_method(D_METHOD("stop_tracker", "emit_signal"), &GazeTracker::stop_tracker, DEFVAL(true));
    ClassDB::bind_method(D_METHOD("complete_initialization"), &GazeTracker::complete_initialization);
    ClassDB::bind_method(D_METHOD("trigger_permission_request"), &GazeTracker::trigger_permission_request);
    ClassDB::bind_method(D_METHOD("clear_calibration"), &GazeTracker::clear_calibration);
    ClassDB::bind_method(D_METHOD("update_projection_parameters"), &GazeTracker::update_projection_parameters);
    ClassDB::bind_method(D_METHOD("map_viewport_to_screen", "logical_pixel"), &GazeTracker::map_viewport_to_screen);
    ClassDB::bind_method(D_METHOD("filter_gaze_coordinate", "raw"), &GazeTracker::filter_gaze_coordinate);
    ClassDB::bind_method(D_METHOD("feed_gaze", "face_detected", "origin", "direction"), &GazeTracker::feed_gaze);
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
    ClassDB::bind_method(D_METHOD("_on_gaze_data_ready", "rid"), &GazeTracker::_on_gaze_data_ready);

    ClassDB::bind_method(D_METHOD("get_lifecycle_state"), &GazeTracker::get_lifecycle_state);
    ClassDB::bind_method(D_METHOD("set_autostart", "autostart"), &GazeTracker::set_autostart);
    ClassDB::bind_method(D_METHOD("get_autostart"), &GazeTracker::get_autostart);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "lifecycle_state", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "", "get_lifecycle_state");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autostart"), "set_autostart", "get_autostart");



    ClassDB::bind_method(D_METHOD("set_device_calibration", "res"), &GazeTracker::set_device_calibration);
    ClassDB::bind_method(D_METHOD("get_device_calibration"), &GazeTracker::get_device_calibration);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "device_calibration", PROPERTY_HINT_RESOURCE_TYPE, "DeviceCalibration"), "set_device_calibration", "get_device_calibration");

    ClassDB::bind_method(D_METHOD("set_bio_calibration", "res"), &GazeTracker::set_bio_calibration);
    ClassDB::bind_method(D_METHOD("get_bio_calibration"), &GazeTracker::get_bio_calibration);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "bio_calibration", PROPERTY_HINT_RESOURCE_TYPE, "BioCalibration"), "set_bio_calibration", "get_bio_calibration");

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "screen_smooth", PROPERTY_HINT_RESOURCE_TYPE, "Smoother"), "set_screen_smooth", "get_screen_smooth");

    ClassDB::bind_method(D_METHOD("set_display_profile", "profile"), &GazeTracker::set_display_profile);
    ClassDB::bind_method(D_METHOD("get_display_profile"), &GazeTracker::get_display_profile);
    ClassDB::bind_method(D_METHOD("get_adjusted_viewport_transform"), &GazeTracker::get_adjusted_viewport_transform);
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

    ClassDB::bind_method(D_METHOD("get_head_rotation_inference_space"), &GazeTracker::get_head_rotation_inference_space);
    ClassDB::bind_method(D_METHOD("get_head_translation_inference_space"), &GazeTracker::get_head_translation_inference_space);
    ClassDB::bind_method(D_METHOD("get_head_position"), &GazeTracker::get_head_position);
    ClassDB::bind_method(D_METHOD("get_head_forward"), &GazeTracker::get_head_forward);
    ClassDB::bind_method(D_METHOD("project_gaze_ray_to_viewport", "origin", "direction", "apply_calibration"), &GazeTracker::project_gaze_ray_to_viewport, DEFVAL(true));
    ClassDB::bind_method(D_METHOD("get_left_eye_center_inference_space"), &GazeTracker::get_left_eye_center_inference_space);
    ClassDB::bind_method(D_METHOD("get_right_eye_center_inference_space"), &GazeTracker::get_right_eye_center_inference_space);
    ClassDB::bind_method(D_METHOD("get_gaze_direction_inference_space"), &GazeTracker::get_gaze_direction_inference_space);

    BIND_ENUM_CONSTANT(LIFECYCLE_UNKNOWN);
    BIND_ENUM_CONSTANT(LIFECYCLE_PERM_REQ);
    BIND_ENUM_CONSTANT(LIFECYCLE_INITIALIZING);
    BIND_ENUM_CONSTANT(LIFECYCLE_RUNNING);
    BIND_ENUM_CONSTANT(LIFECYCLE_ERROR);

    ADD_SIGNAL(MethodInfo("gaze_updated", PropertyInfo(Variant::VECTOR2, "screen_pixel")));
    ADD_SIGNAL(MethodInfo("face_detection_changed", PropertyInfo(Variant::BOOL, "detected")));
    ADD_SIGNAL(MethodInfo("face_frame_ready"));
    ADD_SIGNAL(MethodInfo("lifecycle_changed", PropertyInfo(Variant::INT, "state")));

    ClassDB::bind_method(D_METHOD("set_verbosity", "level"), &GazeTracker::set_verbosity);
    ClassDB::bind_method(D_METHOD("get_verbosity"), &GazeTracker::get_verbosity);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "verbosity"), "set_verbosity", "get_verbosity");
}

GazeTracker::GazeTracker() {
    ::Gaze::g_is_exiting = false;
    update_projection_parameters();
}
GazeTracker::~GazeTracker() {
    Gaze::log_info(2, "GazeTracker_Destructor_Began");
    ::Gaze::g_is_exiting = true;
    camera_sensor = nullptr;
    face_estimator = nullptr;
    eye_estimator = nullptr;
    stop_tracker(false);
    Gaze::log_info(2, "GazeTracker_Destructor_Finished");
}

void GazeTracker::_ready() {
    if (autostart && !Engine::get_singleton()->is_editor_hint()) {
        initialize_tracker();
    }
}

void GazeTracker::_process(double delta) {
    if (!tracker_initialized) return;

    PlatformGeometry geom = platform_get_geometry();
    Transform2D vp_xform = get_adjusted_viewport_transform();
    if (geom.window_position_px != last_window_pos || vp_xform != last_vp_xform) {
        update_projection_parameters();
    }

    GazeServer *gs = GazeServer::get_singleton();
    if (gs) {
        gs->trigger_process();
    }

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
        case NOTIFICATION_WM_CLOSE_REQUEST:
        case NOTIFICATION_WM_GO_BACK_REQUEST: {
            Gaze::log_info("GazeTracker_Notification_WM_Close_Or_GoBack");
            stop_tracker(false);
            break;
        }
        case NOTIFICATION_APPLICATION_FOCUS_IN:
        case NOTIFICATION_APPLICATION_RESUMED: {
            if (lifecycle_state == LIFECYCLE_PERM_REQ) {
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

    set_lifecycle_state(LIFECYCLE_INITIALIZING);
    complete_initialization();
}





bool GazeTracker::initialize_tracker() {
    if (lifecycle_state == LIFECYCLE_RUNNING || lifecycle_state == LIFECYCLE_INITIALIZING) return true;

    GazeServer *gs = GazeServer::get_singleton();
    if (!gs) return false;

    display_rid = gs->display_create();

    camera_sensor = Object::cast_to<CameraSensor>(get_node_or_null("CameraSensor"));
    if (!camera_sensor) {
        camera_sensor = memnew(CameraSensor);
        camera_sensor->set_name("CameraSensor");
        camera_sensor->set_camera_device_id(camera_device_id);
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

    if (display_profile.is_null()) {
        display_profile = DisplayProfile::estimate_from_os();
    }

    if (screen_smooth.is_null()) {
        Ref<OneEuroSmoother> default_smoother;
        default_smoother.instantiate();
        screen_smooth = default_smoother;
    }
    smoother_state = screen_smooth->_smoother_init();

    if (camera_sensor && camera_sensor->get_position() == Vector3(0.0, 0.0, 0.0)) {
        camera_sensor->set_position(Vector3(0.0, display_profile->get_physical_size_mm().y * 0.5, 0.0));
    }

    platform_initialize();
    update_projection_parameters();

    return true;
}

void GazeTracker::stop_tracker(bool p_emit_signal) {
    Gaze::log_info(2, "GazeTracker_StopTracker_Began", "p_emit_signal", p_emit_signal);
    tracker_initialized = false;
    is_face_tracked = false;

    platform_terminate();

    GazeServer *gs = GazeServer::get_singleton();
    if (gs) {
        if (gs->is_connected("gaze_data_ready", Callable(this, "_on_gaze_data_ready"))) {
            gs->disconnect("gaze_data_ready", Callable(this, "_on_gaze_data_ready"));
        }
        Gaze::log_info(2, "GazeTracker_StopTracker_GazeServerStopProcessing_Began");
        gs->stop_processing();
        Gaze::log_info(2, "GazeTracker_StopTracker_GazeServerStopProcessing_Finished");
        
        if (eye_gaze_rid.is_valid()) {
            gs->eye_tracker_free(eye_gaze_rid);
            eye_gaze_rid = RID();
        }
        if (face_gaze_rid.is_valid()) {
            gs->face_tracker_free(face_gaze_rid);
            face_gaze_rid = RID();
        }
        if (camera_gaze_rid.is_valid()) {
            gs->camera_free(camera_gaze_rid);
            camera_gaze_rid = RID();
        }
        if (display_rid.is_valid()) {
            gs->display_free(display_rid);
            display_rid = RID();
        }
    }

    if (camera_sensor && ObjectDB::get_instance(camera_sensor->get_instance_id())) {
        Gaze::log_info(2, "GazeTracker_StopTracker_CameraSensorStop_Began");
        camera_sensor->stop_sensor();
        Gaze::log_info(2, "GazeTracker_StopTracker_CameraSensorStop_Finished");
    }
    if (face_estimator && ObjectDB::get_instance(face_estimator->get_instance_id())) {
        face_estimator->stop_estimator();
    }
    if (eye_estimator && ObjectDB::get_instance(eye_estimator->get_instance_id())) {
        eye_estimator->stop_estimator();
    }

    if (p_emit_signal) {
        set_lifecycle_state(LIFECYCLE_UNKNOWN);
    } else {
        lifecycle_state = LIFECYCLE_UNKNOWN;
    }
    Gaze::log_info(2, "GazeTracker_StopTracker_Finished");
}

void GazeTracker::clear_calibration() {
    if (device_calibration.is_valid() && device_calibration->is_connected("changed", Callable(this, "update_projection_parameters"))) {
        device_calibration->disconnect("changed", Callable(this, "update_projection_parameters"));
    }
    if (bio_calibration.is_valid() && bio_calibration->is_connected("changed", Callable(this, "update_projection_parameters"))) {
        bio_calibration->disconnect("changed", Callable(this, "update_projection_parameters"));
    }
    device_calibration.unref();
    bio_calibration.unref();
    update_projection_parameters();
}

void GazeTracker::feed_gaze(bool face_detected, Vector3 origin, Vector3 direction) {
    // Legacy support
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
        latest_projected_gaze_px = Vector2();
        latest_filtered_gaze_px = Vector2();
    }
}

void GazeTracker::update_projection_parameters() {
    if (!display_rid.is_valid()) return;

    Vector2i size_px(DEFAULT_SCREEN_SIZE_PIXELS.x, DEFAULT_SCREEN_SIZE_PIXELS.y);
    Vector2 size_mm(DEFAULT_SCREEN_SIZE_MM.x, DEFAULT_SCREEN_SIZE_MM.y);

    if (display_profile.is_valid()) {
        size_px = display_profile->get_logical_size_px();
        size_mm = display_profile->get_physical_size_mm();
    }

    Ref<DeviceCalibration> dev_cal = device_calibration;
    if (dev_cal.is_null()) {
        dev_cal = get_default_device_calibration();
    }

    Ref<BioCalibration> bio_cal = bio_calibration;
    if (bio_cal.is_null()) {
        Ref<DefaultBioCalibration> def_bio;
        def_bio.instantiate();
        bio_cal = def_bio;
    }

    Vector3 cam_off = get_derived_camera_offset();
    double cam_tilt = get_derived_camera_tilt();
    Vector2 pixel_sz_mm(0.25, 0.25);

    if (dev_cal.is_valid()) {
        cam_off = dev_cal->get_camera_offset(const_cast<GazeTracker*>(this));
        cam_tilt = dev_cal->get_camera_tilt(const_cast<GazeTracker*>(this));
        pixel_sz_mm = dev_cal->get_pixel_size_mm(const_cast<GazeTracker*>(this));
        if (pixel_sz_mm.x > 0.0 && pixel_sz_mm.y > 0.0) {
            size_mm = pixel_sz_mm * Vector2(size_px.x, size_px.y);
        }
    }

    GazeServer *gs = GazeServer::get_singleton();
    if (gs) {
        gs->display_set_geometry(display_rid, Vector2(size_px.x, size_px.y), size_mm);
        gs->display_set_device_calibration(display_rid, dev_cal);
        gs->display_set_bio_calibration(display_rid, bio_cal);

        PlatformGeometry geom = platform_get_geometry();
        Transform2D vp_xform = get_adjusted_viewport_transform();
        Transform2D vp_xform_logical = vp_xform;
        Vector2 scale = DisplayProfile::get_screen_scale();
        if (scale.x > 0.0 && scale.y > 0.0) {
            vp_xform_logical.columns[0] /= scale.x;
            vp_xform_logical.columns[1] /= scale.y;
            vp_xform_logical.columns[2] /= scale;
        }
        gs->display_set_window_parameters(display_rid, geom.window_position_px, vp_xform_logical);
        
        last_window_pos = geom.window_position_px;
        last_vp_xform = vp_xform;

        if (camera_gaze_rid.is_valid()) {
            gs->camera_set_offsets(camera_gaze_rid, cam_off, cam_tilt);
        }
    }

    // Keep internal mathematical projection engine synchronized for mapping/projection helper calls
    Gaze::CameraPlacement placement(Gaze::GazeVector3(cam_off.x, cam_off.y, cam_off.z), cam_tilt);
    projection_engine.set_camera_placement(placement);
    projection_engine.set_screen_size_pixels(Gaze::GazeVector2(size_px.x, size_px.y));
    projection_engine.set_screen_size_mm(Gaze::GazeVector2(size_mm.x, size_mm.y));

    Gaze::GazeCalibration c;
    if (bio_cal.is_valid()) {
        c.bias_pitch = bio_cal->get_bias_pitch();
        c.bias_yaw = bio_cal->get_bias_yaw();
        c.scale_pitch = bio_cal->get_scale_pitch();
        c.scale_yaw = bio_cal->get_scale_yaw();
    }
    if (dev_cal.is_valid()) {
        c.pixel_size_mm = Gaze::GazeVector2(pixel_sz_mm.x, pixel_sz_mm.y);
        c.camera_offset = Gaze::GazeVector3(cam_off.x, cam_off.y, cam_off.z);
        c.camera_tilt = cam_tilt;
    }
    projection_engine.set_calibration(c);

    double f = 1000.0;
    if (camera_sensor) {
        double focal = camera_sensor->get_focal_length();
        if (focal > 0.0) {
            f = focal;
        } else {
            double fov = camera_sensor->get_camera_fov();
            int cam_w = 640;
            Ref<Image> img = camera_sensor->get_last_frame();
            if (img.is_valid() && img->get_width() > 0) {
                cam_w = img->get_width();
            }
            f = Gaze::get_focal_length_px(cam_w, fov);
        }
    }
    projection_engine.set_camera_focal_length_px(f);
}

void GazeTracker::set_device_calibration(const Ref<DeviceCalibration>& res) {
    if (device_calibration.is_valid() && device_calibration->is_connected("changed", Callable(this, "update_projection_parameters"))) {
        device_calibration->disconnect("changed", Callable(this, "update_projection_parameters"));
    }
    device_calibration = res;
    if (device_calibration.is_valid() && !device_calibration->is_connected("changed", Callable(this, "update_projection_parameters"))) {
        device_calibration->connect("changed", Callable(this, "update_projection_parameters"));
    }
    update_projection_parameters();
}

Ref<DeviceCalibration> GazeTracker::get_device_calibration() const {
    return device_calibration;
}

void GazeTracker::set_bio_calibration(const Ref<BioCalibration>& res) {
    if (bio_calibration.is_valid() && bio_calibration->is_connected("changed", Callable(this, "update_projection_parameters"))) {
        bio_calibration->disconnect("changed", Callable(this, "update_projection_parameters"));
    }
    bio_calibration = res;
    if (bio_calibration.is_valid() && !bio_calibration->is_connected("changed", Callable(this, "update_projection_parameters"))) {
        bio_calibration->connect("changed", Callable(this, "update_projection_parameters"));
    }
    update_projection_parameters();
}

Ref<BioCalibration> GazeTracker::get_bio_calibration() const {
    return bio_calibration;
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
    if (camera_sensor) {
        camera_sensor->set_camera_device_id(id);
    }
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

void GazeTracker::set_verbosity(int level) {
    Gaze::set_log_verbosity(level);
}

int GazeTracker::get_verbosity() const {
    return Gaze::get_log_verbosity().load();
}

void GazeTracker::set_debug_logging_frames(int frames) {
    debug_logging_frames = frames;
    debug_log_frame_counter = 0;
}

int GazeTracker::get_debug_logging_frames() const {
    return debug_logging_frames;
}

Transform3D GazeTracker::get_head_transform() const {
    if (!face_estimator) return Transform3D();
    return GazeServer::get_singleton()->get_relative_transform(face_estimator->get_face_rid());
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
    return get_head_transform().origin;
}

Vector3 GazeTracker::get_head_forward() const {
    return get_head_transform().basis.get_column(2).normalized();
}

Vector2 GazeTracker::project_gaze_ray_to_viewport(Vector3 origin, Vector3 direction, bool apply_calibration) const {
    const_cast<GazeTracker*>(this)->update_projection_parameters();
    const Gaze::GazeVector3& origin_cam = reinterpret_cast<const Gaze::GazeVector3&>(origin);
    Gaze::GazeVector3 dir_cam = reinterpret_cast<const Gaze::GazeVector3&>(direction);

    if (apply_calibration) {
        dir_cam = projection_engine.apply_3d_bias(dir_cam);
    }

    PlatformGeometry geom = platform_get_geometry();
    Transform2D vp_xform = get_adjusted_viewport_transform();

    Vector2 scale = DisplayProfile::get_screen_scale();

    Gaze::ScreenProjector projector = Gaze::ScreenProjector::from_godot_geometry(
        Gaze::GazeVector2(geom.window_position_px.x, geom.window_position_px.y),
        Gaze::GazeVector2(vp_xform.get_scale().x / scale.x, vp_xform.get_scale().y / scale.y),
        Gaze::GazeVector2(vp_xform.get_origin().x / scale.x, vp_xform.get_origin().y / scale.y)
    );

    Gaze::GazeVector2 local_pixel;
    if (projector.project_to_viewport(projection_engine, origin_cam, dir_cam, local_pixel)) {
        return Vector2(local_pixel.x, local_pixel.y);
    }
    return Vector2(INFINITY, INFINITY);
}

Vector3 GazeTracker::get_gaze_origin() const {
    if (!eye_gaze_rid.is_valid()) return Vector3();
    GazeServer *gs = GazeServer::get_singleton();
    if (!gs) return Vector3();
    return gs->get_gaze_origin_from_eye_tracker(eye_gaze_rid);
}

Vector3 GazeTracker::get_gaze_direction(bool apply_calibration) const {
    if (!eye_gaze_rid.is_valid()) return Vector3(0.0, 0.0, -1.0);
    GazeServer *gs = GazeServer::get_singleton();
    if (!gs) return Vector3(0.0, 0.0, -1.0);
    
    Vector3 dir = gs->get_gaze_direction_from_eye_tracker(eye_gaze_rid);
    if (apply_calibration) {
        Gaze::GazeVector3 calib_dir = projection_engine.apply_3d_bias(Gaze::GazeVector3(dir.x, dir.y, dir.z));
        return Vector3(calib_dir.x, calib_dir.y, calib_dir.z);
    }
    return dir;
}

Vector3 GazeTracker::get_head_rotation_inference_space() const {
    if (!face_gaze_rid.is_valid()) return Vector3();
    GazeServer *gs = GazeServer::get_singleton();
    if (!gs) return Vector3();
    return gs->get_head_rotation_from_face_tracker(face_gaze_rid);
}

Vector3 GazeTracker::get_head_translation_inference_space() const {
    if (!face_gaze_rid.is_valid()) return Vector3();
    GazeServer *gs = GazeServer::get_singleton();
    if (!gs) return Vector3();
    return gs->get_head_translation_from_face_tracker(face_gaze_rid);
}

Vector3 GazeTracker::get_left_eye_center_inference_space() const {
    if (!eye_gaze_rid.is_valid()) return Vector3();
    GazeServer *gs = GazeServer::get_singleton();
    if (!gs) return Vector3();
    return gs->get_gaze_origin_from_eye_tracker(eye_gaze_rid);
}

Vector3 GazeTracker::get_right_eye_center_inference_space() const {
    if (!eye_gaze_rid.is_valid()) return Vector3();
    GazeServer *gs = GazeServer::get_singleton();
    if (!gs) return Vector3();
    return gs->get_gaze_origin_from_eye_tracker(eye_gaze_rid);
}

Vector3 GazeTracker::get_gaze_direction_inference_space() const {
    if (!eye_gaze_rid.is_valid()) return Vector3();
    GazeServer *gs = GazeServer::get_singleton();
    if (!gs) return Vector3();
    return gs->get_gaze_direction_from_eye_tracker(eye_gaze_rid);
}

CameraSensor* GazeTracker::get_camera_sensor() const { return camera_sensor; }
FaceEstimator* GazeTracker::get_face_estimator() const { return face_estimator; }
EyeEstimator* GazeTracker::get_eye_estimator() const { return eye_estimator; }

void GazeTracker::set_screen_smooth(const Ref<Smoother>& smoother) {
    screen_smooth = smoother;
    if (eye_estimator) {
        GazeServer::get_singleton()->eye_tracker_set_smoother(eye_estimator->get_eye_rid(), screen_smooth);
    }
}

Ref<Smoother> GazeTracker::get_screen_smooth() const {
    return screen_smooth;
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
    Vector2 scale = DisplayProfile::get_screen_scale();
    Gaze::ScreenProjector projector = Gaze::ScreenProjector::from_godot_geometry(
        Gaze::GazeVector2(geom.window_position_px.x, geom.window_position_px.y),
        Gaze::GazeVector2(vp_xform.get_scale().x / scale.x, vp_xform.get_scale().y / scale.y),
        Gaze::GazeVector2(vp_xform.get_origin().x / scale.x, vp_xform.get_origin().y / scale.y)
    );
    Gaze::GazeVector2 screen_px = projector.map_viewport_to_screen_px(Gaze::GazeVector2(logical_pixel.x, logical_pixel.y));
    return Vector2(screen_px.x, screen_px.y);
}

Vector2 GazeTracker::filter_gaze_coordinate(Vector2 raw) {
    if (screen_smooth.is_null() || smoother_state.is_empty()) {
        return raw;
    }
    double tstamp = (double)Time::get_singleton()->get_ticks_msec() / 1000.0;
    Variant filtered = screen_smooth->_smoother_next(smoother_state, tstamp, raw);
    return filtered;
}

bool GazeTracker::complete_initialization() {
    if (camera_sensor) camera_sensor->initialize_sensor();

    GazeServer *gs = GazeServer::get_singleton();
    if (gs) {
        // Create spatial representations
        camera_gaze_rid = gs->camera_create(display_rid);
        face_gaze_rid = gs->face_tracker_create(camera_gaze_rid);
        eye_gaze_rid = gs->eye_tracker_create(face_gaze_rid);

        // Inject them
        if (face_estimator) face_estimator->set_face_rid(face_gaze_rid);
        if (eye_estimator) {
            eye_estimator->set_eye_rid(eye_gaze_rid);
            if (screen_smooth.is_valid()) {
                gs->eye_tracker_set_smoother(eye_gaze_rid, screen_smooth);
            }
        }

        // Link GazeServer camera to VisionServer camera
        if (camera_sensor) {
            gs->camera_set_vision_rid(camera_gaze_rid, camera_sensor->get_camera_rid());
        }

        // Initialize estimators (connects signals)
        if (face_estimator) face_estimator->initialize_estimator();
        if (eye_estimator) eye_estimator->initialize_estimator();

        gs->connect("gaze_data_ready", Callable(this, "_on_gaze_data_ready"));

#ifndef WEB_ENABLED
        // Push initial geometry parameters to GazeServer display
        update_projection_parameters();

        gs->start_processing();
#endif
    }

    tracker_initialized = true;
    set_lifecycle_state(LIFECYCLE_RUNNING);
    return true;
}

void GazeTracker::_on_gaze_data_ready(RID p_rid) {
    if (!eye_estimator) return;
    
    RID target_eye = eye_estimator->get_eye_rid();
    if (p_rid == target_eye) {
        GazeServer *gs = GazeServer::get_singleton();
        if (gs && face_estimator) {
            is_face_tracked = gs->is_face_detected(face_estimator->get_face_rid());
            last_frame_time = Time::get_singleton()->get_ticks_msec();
            
            latest_filtered_gaze_px = gs->get_gaze_screen(display_rid, true);
            latest_projected_gaze_px = gs->get_gaze_screen(display_rid, false);

            if (debug_logging_frames > 0 && debug_log_frame_counter < debug_logging_frames) {
                debug_log_frame_counter++;
                Vector3 origin = get_gaze_origin();
                Vector3 direction = get_gaze_direction(false); // raw uncalibrated direction
                Vector3 head_pos = get_head_translation_inference_space();
                Vector3 head_rot = get_head_rotation_inference_space();
                UtilityFunctions::print("[GazeTrackerTelemetry] frame=", debug_log_frame_counter,
                                         " head_pos=(", head_pos.x, ",", head_pos.y, ",", head_pos.z, ")",
                                         " head_rot=(", head_rot.x, ",", head_rot.y, ",", head_rot.z, ")",
                                         " origin=(", origin.x, ",", origin.y, ",", origin.z, ")",
                                         " dir=(", direction.x, ",", direction.y, ",", direction.z, ")",
                                         " proj=(", latest_projected_gaze_px.x, ",", latest_projected_gaze_px.y, ")");
            }

            emit_signal("gaze_updated", latest_filtered_gaze_px);
        }
    }
}

} // namespace godot
