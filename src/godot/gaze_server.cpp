#include "gaze_server.hpp"
#include "vision_server.hpp"
#include "log.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/templates/rid_owner.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/main_loop.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <cmath>
#include <vector>
#include <algorithm>
#include "godot_files.hpp"
#include "../core/opencv_space_conversions.hpp"

#ifdef WEB_ENABLED
#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/java_script_object.hpp>
#endif

static_assert(sizeof(Gaze::GazeVector3) == sizeof(godot::Vector3), "Size of GazeVector3 must match godot::Vector3");
static_assert(alignof(Gaze::GazeVector3) == alignof(godot::Vector3), "Alignment of GazeVector3 must match godot::Vector3");

namespace godot {

// Structs definitions inside GazeServerImpl for Pimpl idiom
struct GazeServerImpl {
    struct DisplayInfo {
        Vector2 logical_size_px = Vector2(1920, 1080);
        Vector2 physical_size_mm = Vector2(345.0, 215.0);
        Ref<DeviceCalibration> device_calibration;
        Ref<BioCalibration> bio_calibration;

        struct BioCalibrationData {
            bool is_valid = false;
            double bias_pitch = 0.0;
            double bias_yaw = 0.0;
            double scale_yaw = 1.0;
            double scale_pitch = 1.0;
        } bio_data;

        Vector2 window_position_px = Vector2(0.0, 0.0);
        Transform2D viewport_transform;
    };

    struct CameraInfo {
        RID parent_display_rid;
        Vector3 offset = Vector3(0.0, 107.5, 0.0);
        double tilt = 0.0;
        RID vision_camera_rid;
    };

    struct FaceInfo {
        RID parent_camera_rid;
        Transform3D relative_transform;
        bool detected = false;
        
        Gaze::GazeVector3 head_pose_translation;
        Gaze::GazeVector3 head_pose_rotation;
        PackedVector2Array landmarks_2d;
    };

    struct EyeInfo {
        RID parent_face_rid;
        Transform3D relative_transform;
        
        Vector3 gaze_origin_cam;
        Vector3 gaze_direction_cam;
        
        Ref<Smoother> screen_smoother;
        Array smoother_state;
        
        Vector2 latest_projected_gaze;
        Vector2 latest_filtered_gaze;
        Vector2 latest_projected_gaze_mm;
        Vector2 latest_filtered_gaze_mm;

        float left_eye_openness = 1.0f;
        float right_eye_openness = 1.0f;

        Ref<Image> left_eye_crop;
        Ref<Image> right_eye_crop;
        bool crop_requested = false;
    };

    RID_PtrOwner<DisplayInfo, true> display_owner;
    RID_PtrOwner<CameraInfo, true> camera_owner;
    RID_PtrOwner<FaceInfo, true> face_owner;
    RID_PtrOwner<EyeInfo, true> eye_owner;

    std::vector<RID> allocated_displays;
    std::vector<RID> allocated_cameras;
    std::vector<RID> allocated_faces;
    std::vector<RID> allocated_eyes;

    ~GazeServerImpl() {
        for (RID rid : allocated_eyes) {
            EyeInfo *info = eye_owner.get_or_null(rid);
            if (info) {
                memdelete(info);
            }
        }
        allocated_eyes.clear();

        for (RID rid : allocated_faces) {
            FaceInfo *info = face_owner.get_or_null(rid);
            if (info) {
                memdelete(info);
            }
        }
        allocated_faces.clear();

        for (RID rid : allocated_cameras) {
            CameraInfo *info = camera_owner.get_or_null(rid);
            if (info) {
                memdelete(info);
            }
        }
        allocated_cameras.clear();

        for (RID rid : allocated_displays) {
            DisplayInfo *info = display_owner.get_or_null(rid);
            if (info) {
                memdelete(info);
            }
        }
        allocated_displays.clear();
    }
};

// Aliases for shorter syntax inside this implementation file
using DisplayInfo = GazeServerImpl::DisplayInfo;
using CameraInfo = GazeServerImpl::CameraInfo;
using FaceInfo = GazeServerImpl::FaceInfo;
using EyeInfo = GazeServerImpl::EyeInfo;

GazeServer *GazeServer::singleton = nullptr;

void GazeServer::_bind_methods() {
    // High level lifecycle
    ClassDB::bind_method(D_METHOD("start_tracking"), &GazeServer::start_tracking);
    ClassDB::bind_method(D_METHOD("stop_tracking", "immediate"), &GazeServer::stop_tracking, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("_deferred_stop_check"), &GazeServer::_deferred_stop_check);
    ClassDB::bind_method(D_METHOD("is_tracking_active"), &GazeServer::is_tracking_active);
    ClassDB::bind_method(D_METHOD("get_most_recent_event"), &GazeServer::get_most_recent_event);

    // Calibrations
    ClassDB::bind_method(D_METHOD("set_display_profile", "profile"), &GazeServer::set_display_profile);
    ClassDB::bind_method(D_METHOD("get_display_profile"), &GazeServer::get_display_profile);
    ClassDB::bind_method(D_METHOD("set_device_calibration", "calibration"), &GazeServer::set_device_calibration);
    ClassDB::bind_method(D_METHOD("get_device_calibration"), &GazeServer::get_device_calibration);
    ClassDB::bind_method(D_METHOD("set_bio_calibration", "calibration"), &GazeServer::set_bio_calibration);
    ClassDB::bind_method(D_METHOD("get_bio_calibration"), &GazeServer::get_bio_calibration);

    // Emulation
    ClassDB::bind_method(D_METHOD("set_emulate_gaze_from_mouse", "enable"), &GazeServer::set_emulate_gaze_from_mouse);
    ClassDB::bind_method(D_METHOD("get_emulate_gaze_from_mouse"), &GazeServer::get_emulate_gaze_from_mouse);
    ClassDB::bind_method(D_METHOD("set_emulate_mouse_from_gaze", "enable"), &GazeServer::set_emulate_mouse_from_gaze);
    ClassDB::bind_method(D_METHOD("get_emulate_mouse_from_gaze"), &GazeServer::get_emulate_mouse_from_gaze);

    // Debug Frame Access
    ClassDB::bind_method(D_METHOD("get_camera_texture"), &GazeServer::get_camera_texture);
    ClassDB::bind_method(D_METHOD("get_debug_landmarks"), &GazeServer::get_debug_landmarks);

    // RID Resource Management
    ClassDB::bind_method(D_METHOD("display_create"), &GazeServer::display_create);
    ClassDB::bind_method(D_METHOD("display_set_geometry", "display_rid", "logical_size", "physical_size"), &GazeServer::display_set_geometry);
    ClassDB::bind_method(D_METHOD("display_set_device_calibration", "display_rid", "calibration"), &GazeServer::display_set_device_calibration);
    ClassDB::bind_method(D_METHOD("display_set_bio_calibration", "display_rid", "calibration"), &GazeServer::display_set_bio_calibration);
    ClassDB::bind_method(D_METHOD("display_set_window_parameters", "display_rid", "window_pos", "viewport_transform"), &GazeServer::display_set_window_parameters);
    ClassDB::bind_method(D_METHOD("display_free", "display_rid"), &GazeServer::display_free);

    ClassDB::bind_method(D_METHOD("camera_create", "display_rid"), &GazeServer::camera_create);
    ClassDB::bind_method(D_METHOD("camera_set_offsets", "camera_rid", "offset", "tilt"), &GazeServer::camera_set_offsets);
    ClassDB::bind_method(D_METHOD("camera_set_vision_rid", "camera_rid", "vision_camera_rid"), &GazeServer::camera_set_vision_rid);
    ClassDB::bind_method(D_METHOD("camera_free", "camera_rid"), &GazeServer::camera_free);

    ClassDB::bind_method(D_METHOD("face_tracker_create", "camera_rid"), &GazeServer::face_tracker_create);
    ClassDB::bind_method(D_METHOD("get_face_model_points"), &GazeServer::get_face_model_points);
    ClassDB::bind_method(D_METHOD("face_tracker_set_pose", "face_rid", "translation", "rotation", "detected"), &GazeServer::face_tracker_set_pose);
    ClassDB::bind_method(D_METHOD("face_tracker_free", "face_rid"), &GazeServer::face_tracker_free);
    ClassDB::bind_method(D_METHOD("get_head_rotation_from_face_tracker", "face_rid"), &GazeServer::get_head_rotation_from_face_tracker);
    ClassDB::bind_method(D_METHOD("get_head_translation_from_face_tracker", "face_rid"), &GazeServer::get_head_translation_from_face_tracker);
    ClassDB::bind_method(D_METHOD("get_head_pose_origin_mm", "face_rid"), &GazeServer::get_head_pose_origin_mm);
    ClassDB::bind_method(D_METHOD("get_head_pose_euler_deg", "face_rid"), &GazeServer::get_head_pose_euler_deg);

    ClassDB::bind_method(D_METHOD("eye_tracker_create", "face_rid"), &GazeServer::eye_tracker_create);
    ClassDB::bind_method(D_METHOD("eye_tracker_set_gaze", "eye_rid", "origin_cam", "direction_cam"), &GazeServer::eye_tracker_set_gaze);
    ClassDB::bind_method(D_METHOD("eye_tracker_set_openness", "eye_rid", "left", "right"), &GazeServer::eye_tracker_set_openness);
    ClassDB::bind_method(D_METHOD("get_left_eye_openness", "eye_rid"), &GazeServer::get_left_eye_openness);
    ClassDB::bind_method(D_METHOD("get_right_eye_openness", "eye_rid"), &GazeServer::get_right_eye_openness);
    ClassDB::bind_method(D_METHOD("eye_tracker_set_smoother", "eye_rid", "smoother"), &GazeServer::eye_tracker_set_smoother);
    ClassDB::bind_method(D_METHOD("eye_tracker_set_crop_requested", "eye_rid", "requested"), &GazeServer::eye_tracker_set_crop_requested);
    ClassDB::bind_method(D_METHOD("eye_tracker_is_crop_requested", "eye_rid"), &GazeServer::eye_tracker_is_crop_requested);
    ClassDB::bind_method(D_METHOD("tracker_get_eye_crops", "eye_rid"), &GazeServer::tracker_get_eye_crops);
    ClassDB::bind_method(D_METHOD("eye_tracker_free", "eye_rid"), &GazeServer::eye_tracker_free);

    ClassDB::bind_method(D_METHOD("get_gaze_origin_from_eye_tracker", "eye_rid"), &GazeServer::get_gaze_origin_from_eye_tracker);
    ClassDB::bind_method(D_METHOD("get_gaze_direction_from_eye_tracker", "eye_rid"), &GazeServer::get_gaze_direction_from_eye_tracker);
    ClassDB::bind_method(D_METHOD("get_projected_gaze_from_eye_tracker", "eye_rid", "smoothed"), &GazeServer::get_projected_gaze_from_eye_tracker, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("get_projected_gaze_mm_from_eye_tracker", "eye_rid", "smoothed"), &GazeServer::get_projected_gaze_mm_from_eye_tracker, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("set_crops_on_eye_tracker", "eye_rid", "left_crop", "right_crop"), &GazeServer::set_crops_on_eye_tracker);
    ClassDB::bind_method(D_METHOD("reset_eye_tracker", "eye_rid"), &GazeServer::reset_eye_tracker);

    ClassDB::bind_method(D_METHOD("emit_camera_frame_ready", "vision_camera_rid"), &GazeServer::emit_camera_frame_ready);
    ClassDB::bind_method(D_METHOD("get_relative_transform", "entity_rid"), &GazeServer::get_relative_transform);
    ClassDB::bind_method(D_METHOD("get_gaze_screen", "display_rid", "smoothed"), &GazeServer::get_gaze_screen, DEFVAL(true));
    ClassDB::bind_method(D_METHOD("is_face_detected", "face_rid"), &GazeServer::is_face_detected);

    ClassDB::bind_method(D_METHOD("set_pipeline_config", "config"), &GazeServer::set_pipeline_config);
    ClassDB::bind_method(D_METHOD("trigger_process"), &GazeServer::trigger_process);
    ClassDB::bind_method(D_METHOD("start_processing"), &GazeServer::start_processing);
    ClassDB::bind_method(D_METHOD("stop_processing"), &GazeServer::stop_processing);
    ClassDB::bind_method(D_METHOD("ref_tracker"), &GazeServer::ref_tracker);
    ClassDB::bind_method(D_METHOD("unref_tracker"), &GazeServer::unref_tracker);
    ClassDB::bind_method(D_METHOD("get_active_tracker_count"), &GazeServer::get_active_tracker_count);

    ClassDB::bind_method(D_METHOD("set_verbosity", "level"), &GazeServer::set_verbosity);
    ClassDB::bind_method(D_METHOD("get_verbosity"), &GazeServer::get_verbosity);

    ClassDB::bind_static_method("GazeServer", D_METHOD("get_build_info"), &GazeServer::get_build_info);
    ClassDB::bind_static_method("GazeServer", D_METHOD("get_build_timestamp"), &GazeServer::get_build_timestamp);

    ADD_SIGNAL(MethodInfo("gaze_data_ready", PropertyInfo(Variant::RID, "vision_camera_rid")));
    ADD_SIGNAL(MethodInfo("gaze_frame_began", PropertyInfo(Variant::OBJECT, "gaze_frame", PROPERTY_HINT_RESOURCE_TYPE, "GazeFrame")));
    ADD_SIGNAL(MethodInfo("gaze_frame_ready", PropertyInfo(Variant::OBJECT, "gaze_frame", PROPERTY_HINT_RESOURCE_TYPE, "GazeFrame")));
}

GazeServer::GazeServer() {
    singleton = this;
    impl = std::make_unique<GazeServerImpl>();
#ifndef WEB_ENABLED
    pipeline = std::make_unique<Gaze::GazeTrackingPipeline>();
#endif

    // Setup Default Spatial Hierarchy
    default_display_rid = display_create();
    default_camera_rid = camera_create(default_display_rid);
    default_face_rid = face_tracker_create(default_camera_rid);
    default_eye_rid = eye_tracker_create(default_face_rid);

    Ref<OneEuroSmoother> sm;
    sm.instantiate();
    eye_tracker_set_smoother(default_eye_rid, sm);

    default_display_profile = DisplayProfile::estimate_from_os();
    display_set_geometry(default_display_rid, default_display_profile->get_logical_size_px(), default_display_profile->get_physical_size_mm());

    ProjectSettings *ps = ProjectSettings::get_singleton();
    if (ps) {
        if (ps->has_setting("gaze/config/pitch_t_gain")) active_config.pitch_t_gain = ps->get_setting("gaze/config/pitch_t_gain");
        if (ps->has_setting("gaze/config/yaw_t_gain")) active_config.yaw_t_gain = ps->get_setting("gaze/config/yaw_t_gain");
        if (ps->has_setting("gaze/config/nose_y")) active_config.nose_y = ps->get_setting("gaze/config/nose_y");
        if (ps->has_setting("gaze/config/nose_z")) active_config.nose_z = ps->get_setting("gaze/config/nose_z");
        if (ps->has_setting("gaze/config/ipd_mm")) active_config.ipd_mm = ps->get_setting("gaze/config/ipd_mm");
        if (ps->has_setting("gaze/config/debug_image_throttle_interval")) active_config.debug_image_throttle_interval = ps->get_setting("gaze/config/debug_image_throttle_interval");
        if (ps->has_setting("gaze/config/desired_camera_width")) active_config.desired_camera_width = ps->get_setting("gaze/config/desired_camera_width");
        if (ps->has_setting("gaze/config/desired_camera_height")) active_config.desired_camera_height = ps->get_setting("gaze/config/desired_camera_height");

        if (ps->has_setting("gaze/pointing/emulate_gaze_from_mouse")) {
            emulate_gaze_from_mouse = ps->get_setting("gaze/pointing/emulate_gaze_from_mouse");
        }
        if (ps->has_setting("gaze/pointing/emulate_mouse_from_gaze")) {
            emulate_mouse_from_gaze = ps->get_setting("gaze/pointing/emulate_mouse_from_gaze");
        }
    }

#ifndef WEB_ENABLED
    if (pipeline) {
        for (size_t i = 0; i < pipeline->frame_pool.size; ++i) {
            Gaze::GazeFrameData* data = pipeline->frame_pool.get_frame(i);
            if (data) {
                GazeFrame* wrapper = memnew(GazeFrame);
                data->userdata = wrapper;
            }
        }
    }
#endif

    // Autostart tracking if configured
    bool autostart = true;
    if (ps && ps->has_setting("gaze/general/autostart")) {
        autostart = ps->get_setting("gaze/general/autostart");
    }
    if (autostart) {
        start_tracking();
    }
}

GazeServer::~GazeServer() {
    Gaze::log_info(2, "GazeServer_Destructor_Began");
    {
        std::lock_guard<std::recursive_mutex> lock(state_mutex);
        active_trackers = 0;
    }
    stop_processing();
#ifndef WEB_ENABLED
    if (pipeline) {
        for (size_t i = 0; i < pipeline->frame_pool.size; ++i) {
            Gaze::GazeFrameData* data = pipeline->frame_pool.get_frame(i);
            if (data && data->userdata) {
                GazeFrame* wrapper = static_cast<GazeFrame*>(data->userdata);
                memdelete(wrapper);
                data->userdata = nullptr;
            }
        }
    }
    Gaze::log_info(2, "GazeServer_Destructor_PipelineReset_Began");
    pipeline.reset();
    Gaze::log_info(2, "GazeServer_Destructor_PipelineReset_Finished");
#endif
    singleton = nullptr;
    Gaze::log_info(2, "GazeServer_Destructor_Finished");
}

bool GazeServer::start_tracking() {
    bool was_zero = false;
    {
        std::lock_guard<std::recursive_mutex> lock(state_mutex);
        active_trackers++;
        if (active_trackers == 1) {
            was_zero = true;
            start_processing();
        }
    }
    return was_zero;
}

void GazeServer::stop_tracking(bool p_immediate) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    if (active_trackers > 0) {
        active_trackers--;
    }
    if (active_trackers == 0) {
        if (p_immediate) {
            stop_processing();
        } else {
            Callable(this, "_deferred_stop_check").call_deferred();
        }
    }
}

void GazeServer::_deferred_stop_check() {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    if (active_trackers == 0) {
        stop_processing();
    }
}

bool GazeServer::is_tracking_active() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return (active_trackers > 0);
}

Ref<InputEventGazeBase> GazeServer::get_most_recent_event() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return most_recent_event;
}

void GazeServer::set_display_profile(const Ref<DisplayProfile>& p_profile) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    default_display_profile = p_profile.is_valid() ? p_profile : DisplayProfile::estimate_from_os();
    if (default_display_rid.is_valid() && default_display_profile.is_valid()) {
        display_set_geometry(default_display_rid, default_display_profile->get_logical_size_px(), default_display_profile->get_physical_size_mm());
    }
}

Ref<DisplayProfile> GazeServer::get_display_profile() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return default_display_profile;
}

void GazeServer::set_device_calibration(const Ref<DeviceCalibration>& p_calibration) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    default_device_calibration = p_calibration;
    if (default_display_rid.is_valid()) {
        display_set_device_calibration(default_display_rid, p_calibration);
    }
}

Ref<DeviceCalibration> GazeServer::get_device_calibration() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return default_device_calibration;
}

void GazeServer::set_bio_calibration(const Ref<BioCalibration>& p_calibration) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    default_bio_calibration = p_calibration;
    if (default_display_rid.is_valid()) {
        display_set_bio_calibration(default_display_rid, p_calibration);
    }
}

Ref<BioCalibration> GazeServer::get_bio_calibration() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return default_bio_calibration;
}

void GazeServer::set_emulate_gaze_from_mouse(bool p_enable) {
    emulate_gaze_from_mouse = p_enable;
}

bool GazeServer::get_emulate_gaze_from_mouse() const {
    return emulate_gaze_from_mouse;
}

void GazeServer::set_emulate_mouse_from_gaze(bool p_enable) {
    emulate_mouse_from_gaze = p_enable;
}

bool GazeServer::get_emulate_mouse_from_gaze() const {
    return emulate_mouse_from_gaze;
}

Ref<Texture2D> GazeServer::get_camera_texture() {
    VisionServer* vs = VisionServer::get_singleton();
    if (!vs) return Ref<Texture2D>();
    CameraInfo* cam = impl->camera_owner.get_or_null(default_camera_rid);
    if (!cam || !cam->vision_camera_rid.is_valid()) return Ref<Texture2D>();
    return vs->get_camera_current_texture(cam->vision_camera_rid);
}

PackedVector2Array GazeServer::get_debug_landmarks() const {
    return get_face_landmarks_2d(default_face_rid);
}

// Display RID Resource Management
RID GazeServer::display_create() {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    DisplayInfo *info = memnew(DisplayInfo);
    RID rid = impl->display_owner.make_rid(info);
    impl->allocated_displays.push_back(rid);
    return rid;
}

void GazeServer::display_set_geometry(RID p_display, Vector2 p_logical_size, Vector2 p_physical_size) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    DisplayInfo *info = impl->display_owner.get_or_null(p_display);
    ERR_FAIL_NULL(info);
    info->logical_size_px = p_logical_size;
    info->physical_size_mm = p_physical_size;
}

void GazeServer::display_set_device_calibration(RID p_display, const Ref<DeviceCalibration>& p_calibration) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    DisplayInfo *info = impl->display_owner.get_or_null(p_display);
    ERR_FAIL_NULL(info);
    info->device_calibration = p_calibration;
}

void GazeServer::display_set_bio_calibration(RID p_display, const Ref<BioCalibration>& p_calibration) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    DisplayInfo *info = impl->display_owner.get_or_null(p_display);
    ERR_FAIL_NULL(info);
    info->bio_calibration = p_calibration;
    if (p_calibration.is_valid()) {
        info->bio_data.is_valid = true;
        info->bio_data.bias_pitch = p_calibration->get_bias_pitch();
        info->bio_data.bias_yaw = p_calibration->get_bias_yaw();
        info->bio_data.scale_pitch = p_calibration->get_scale_pitch();
        info->bio_data.scale_yaw = p_calibration->get_scale_yaw();
    } else {
        info->bio_data.is_valid = false;
    }
}

void GazeServer::display_set_window_parameters(RID p_display, Vector2 p_window_pos, Transform2D p_viewport_transform) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    DisplayInfo *info = impl->display_owner.get_or_null(p_display);
    ERR_FAIL_NULL(info);
    info->window_position_px = p_window_pos;
    info->viewport_transform = p_viewport_transform;
}

void GazeServer::display_free(RID p_display) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    DisplayInfo *info = impl->display_owner.get_or_null(p_display);
    if (info) {
        impl->allocated_displays.erase(std::remove(impl->allocated_displays.begin(), impl->allocated_displays.end(), p_display), impl->allocated_displays.end());
        impl->display_owner.free(p_display);
        memdelete(info);
    }
}

// Camera Model RID Management
RID GazeServer::camera_create(RID p_display) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    CameraInfo *info = memnew(CameraInfo);
    info->parent_display_rid = p_display;
    RID rid = impl->camera_owner.make_rid(info);
    impl->allocated_cameras.push_back(rid);
    return rid;
}

void GazeServer::camera_set_offsets(RID p_camera, Vector3 p_offset, double p_tilt) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    CameraInfo *info = impl->camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL(info);
    info->offset = p_offset;
    info->tilt = p_tilt;
}

void GazeServer::camera_set_vision_rid(RID p_camera, RID p_vision_camera) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    CameraInfo *info = impl->camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL(info);
    info->vision_camera_rid = p_vision_camera;
}

void GazeServer::camera_free(RID p_camera) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    CameraInfo *info = impl->camera_owner.get_or_null(p_camera);
    if (info) {
        impl->allocated_cameras.erase(std::remove(impl->allocated_cameras.begin(), impl->allocated_cameras.end(), p_camera), impl->allocated_cameras.end());
        impl->camera_owner.free(p_camera);
        memdelete(info);
    }
}

// Face Tracker RID Management
RID GazeServer::face_tracker_create(RID p_camera) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    FaceInfo *info = memnew(FaceInfo);
    info->parent_camera_rid = p_camera;
    RID rid = impl->face_owner.make_rid(info);
    impl->allocated_faces.push_back(rid);
    return rid;
}

PackedVector3Array GazeServer::get_face_model_points() const {
    PackedVector3Array arr;
    const auto& points = Gaze::FaceModelGeometry::get_canonical_35pt_model_points();
    arr.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        arr[i] = Vector3(points[i].x, points[i].y, points[i].z);
    }
    return arr;
}

void GazeServer::face_tracker_set_pose(RID p_face, Vector3 p_translation, Vector3 p_rotation, bool p_detected) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    FaceInfo *info = impl->face_owner.get_or_null(p_face);
    ERR_FAIL_NULL(info);
    info->detected = p_detected;
    info->head_pose_translation = Gaze::GazeVector3(p_translation.x, p_translation.y, p_translation.z);
    info->head_pose_rotation = Gaze::GazeVector3(p_rotation.x, p_rotation.y, p_rotation.z);
    
    if (p_detected) {
        Basis b = Basis::from_euler(p_rotation);
        info->relative_transform = Transform3D(b, p_translation);
    } else {
        info->relative_transform = Transform3D();
    }
}

void GazeServer::face_tracker_free(RID p_face) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    FaceInfo *info = impl->face_owner.get_or_null(p_face);
    if (info) {
        impl->allocated_faces.erase(std::remove(impl->allocated_faces.begin(), impl->allocated_faces.end(), p_face), impl->allocated_faces.end());
        impl->face_owner.free(p_face);
        memdelete(info);
    }
}

Vector3 GazeServer::get_head_rotation_from_face_tracker(RID p_face) const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    FaceInfo *info = impl->face_owner.get_or_null(p_face);
    ERR_FAIL_NULL_V(info, Vector3());
    return Vector3(info->head_pose_rotation.x, info->head_pose_rotation.y, info->head_pose_rotation.z);
}

Vector3 GazeServer::get_head_translation_from_face_tracker(RID p_face) const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    FaceInfo *info = impl->face_owner.get_or_null(p_face);
    ERR_FAIL_NULL_V(info, Vector3());
    return Vector3(info->head_pose_translation.x, info->head_pose_translation.y, info->head_pose_translation.z);
}

Vector3 GazeServer::get_head_pose_origin_mm(RID p_face) const {
    return get_head_translation_from_face_tracker(p_face);
}

Vector3 GazeServer::get_head_pose_euler_deg(RID p_face) const {
    Vector3 rad = get_head_rotation_from_face_tracker(p_face);
    return Vector3(Math::rad_to_deg(rad.x), Math::rad_to_deg(rad.y), Math::rad_to_deg(rad.z));
}

void GazeServer::face_tracker_set_landmarks_2d(RID p_face, const PackedVector2Array &p_landmarks) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    FaceInfo *info = impl->face_owner.get_or_null(p_face);
    ERR_FAIL_NULL(info);
    info->landmarks_2d = p_landmarks;
}

PackedVector2Array GazeServer::get_face_landmarks_2d(RID p_face) const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    FaceInfo *info = impl->face_owner.get_or_null(p_face);
    ERR_FAIL_NULL_V(info, PackedVector2Array());
    return info->landmarks_2d;
}

// Eye Tracker RID Management
RID GazeServer::eye_tracker_create(RID p_face) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *info = memnew(EyeInfo);
    info->parent_face_rid = p_face;
    RID rid = impl->eye_owner.make_rid(info);
    impl->allocated_eyes.push_back(rid);
    return rid;
}

void GazeServer::eye_tracker_set_gaze(RID p_eye, Vector3 p_origin_cam, Vector3 p_direction_cam) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL(eye);
    
    eye->gaze_origin_cam = p_origin_cam;
    eye->gaze_direction_cam = p_direction_cam;

    FaceInfo *face = impl->face_owner.get_or_null(eye->parent_face_rid);
    if (!face) return;
    CameraInfo *cam = impl->camera_owner.get_or_null(face->parent_camera_rid);
    if (!cam) return;
    DisplayInfo *disp = impl->display_owner.get_or_null(cam->parent_display_rid);
    if (!disp) return;

    Vector3 effective_offset = cam->offset;
    double effective_tilt = cam->tilt;
    if (disp->device_calibration.is_valid()) {
        effective_offset = disp->device_calibration->get_camera_offset(nullptr);
        effective_tilt = disp->device_calibration->get_camera_tilt(nullptr);
    }

    Vector3 calibrated_dir = p_direction_cam;
    if (disp->bio_data.is_valid) {
        Gaze::GazeVector3 raw_dir(p_direction_cam.x, p_direction_cam.y, p_direction_cam.z);
        Gaze::GazeVector3 calib_v = Gaze::apply_3d_bias_vector(
            raw_dir,
            Gaze::GazeVector2(disp->bio_data.bias_pitch, disp->bio_data.bias_yaw),
            Gaze::GazeVector2(disp->bio_data.scale_pitch, disp->bio_data.scale_yaw)
        );
        calibrated_dir = Vector3(calib_v.x, calib_v.y, calib_v.z);
    }

    Gaze::GazeVector2 pos_mm;
    Gaze::GazeVector3 origin_godot(p_origin_cam.x, p_origin_cam.y, p_origin_cam.z);
    Gaze::GazeVector3 dir_godot(calibrated_dir.x, calibrated_dir.y, calibrated_dir.z);
    if (Gaze::project_ray_to_screen_mm(
            origin_godot,
            dir_godot,
            Gaze::GazeVector3(effective_offset.x, effective_offset.y, effective_offset.z),
            effective_tilt,
            Gaze::GazeVector2(disp->physical_size_mm.x, disp->physical_size_mm.y),
            pos_mm
        )) {
        double scale_x = disp->logical_size_px.x / disp->physical_size_mm.x;
        double scale_y = disp->logical_size_px.y / disp->physical_size_mm.y;
        Vector2 px(pos_mm.x * scale_x, pos_mm.y * scale_y);

        px = disp->viewport_transform.affine_inverse().xform(px - disp->window_position_px);

        eye->latest_projected_gaze = px;
        Vector2 pos_mm_center(pos_mm.x - disp->physical_size_mm.x * 0.5, pos_mm.y - disp->physical_size_mm.y * 0.5);
        eye->latest_projected_gaze_mm = pos_mm_center;
        
        if (eye->screen_smoother.is_valid()) {
            auto now = std::chrono::steady_clock::now();
            double tstamp = std::chrono::duration<double>(now.time_since_epoch()).count();
            eye->latest_filtered_gaze = eye->screen_smoother->_smoother_next(eye->smoother_state, tstamp, px);
            eye->latest_filtered_gaze_mm = eye->latest_filtered_gaze;
        } else {
            eye->latest_filtered_gaze = px;
            eye->latest_filtered_gaze_mm = pos_mm_center;
        }
    }
}

void GazeServer::eye_tracker_set_openness(RID p_eye, float p_left, float p_right) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL(eye);
    eye->left_eye_openness = p_left;
    eye->right_eye_openness = p_right;
}

float GazeServer::get_left_eye_openness(RID p_eye) const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL_V(eye, 0.0f);
    return eye->left_eye_openness;
}

float GazeServer::get_right_eye_openness(RID p_eye) const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL_V(eye, 0.0f);
    return eye->right_eye_openness;
}

void GazeServer::eye_tracker_set_smoother(RID p_eye, const Ref<Smoother>& p_smoother) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL(eye);
    eye->screen_smoother = p_smoother;
    eye->smoother_state.clear();
}

void GazeServer::eye_tracker_set_crop_requested(RID p_eye, bool p_requested) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL(eye);
    eye->crop_requested = p_requested;
}

bool GazeServer::eye_tracker_is_crop_requested(RID p_eye) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL_V(eye, false);
    return eye->crop_requested;
}

Array GazeServer::tracker_get_eye_crops(RID p_eye) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL_V(eye, Array());
    Array ret;
    ret.push_back(eye->left_eye_crop);
    ret.push_back(eye->right_eye_crop);
    return ret;
}

void GazeServer::eye_tracker_free(RID p_eye) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *info = impl->eye_owner.get_or_null(p_eye);
    if (info) {
        impl->allocated_eyes.erase(std::remove(impl->allocated_eyes.begin(), impl->allocated_eyes.end(), p_eye), impl->allocated_eyes.end());
        impl->eye_owner.free(p_eye);
        memdelete(info);
    }
}

Vector3 GazeServer::get_gaze_origin_from_eye_tracker(RID p_eye) const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL_V(eye, Vector3());
    return eye->gaze_origin_cam;
}

Vector3 GazeServer::get_gaze_direction_from_eye_tracker(RID p_eye) const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL_V(eye, Vector3());
    return eye->gaze_direction_cam;
}

Vector2 GazeServer::get_projected_gaze_from_eye_tracker(RID p_eye, bool p_smoothed) const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL_V(eye, Vector2());
    return p_smoothed ? eye->latest_filtered_gaze : eye->latest_projected_gaze;
}

Vector2 GazeServer::get_projected_gaze_mm_from_eye_tracker(RID p_eye, bool p_smoothed) const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL_V(eye, Vector2());
    return p_smoothed ? eye->latest_filtered_gaze_mm : eye->latest_projected_gaze_mm;
}

void GazeServer::set_crops_on_eye_tracker(RID p_eye, const Ref<Image>& p_left_crop, const Ref<Image>& p_right_crop) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL(eye);
    eye->left_eye_crop = p_left_crop;
    eye->right_eye_crop = p_right_crop;
}

void GazeServer::reset_eye_tracker(RID p_eye) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL(eye);
    eye->latest_projected_gaze = Vector2(-1000.0, -1000.0);
    eye->latest_filtered_gaze = Vector2(-1000.0, -1000.0);
    eye->left_eye_openness = 0.0f;
    eye->right_eye_openness = 0.0f;
    eye->smoother_state.clear();
}

void GazeServer::emit_camera_frame_ready(RID p_vision_camera) {
    emit_signal("gaze_data_ready", p_vision_camera);
}

Transform3D GazeServer::get_relative_transform(RID p_entity) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    FaceInfo *face = impl->face_owner.get_or_null(p_entity);
    if (face) return face->relative_transform;
    EyeInfo *eye = impl->eye_owner.get_or_null(p_entity);
    if (eye) return eye->relative_transform;
    return Transform3D();
}

Vector2 GazeServer::get_gaze_screen(RID p_display, bool p_smoothed) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    DisplayInfo *disp = impl->display_owner.get_or_null(p_display);
    ERR_FAIL_NULL_V(disp, Vector2());

    RID target_camera;
    List<RID> cameras;
    impl->camera_owner.get_owned_list(&cameras);
    for (const RID &r : cameras) {
        CameraInfo *c = impl->camera_owner.get_or_null(r);
        if (c && c->parent_display_rid == p_display) {
            target_camera = r;
            break;
        }
    }
    if (!target_camera.is_valid()) return Vector2();

    RID target_face;
    List<RID> faces;
    impl->face_owner.get_owned_list(&faces);
    for (const RID &r : faces) {
        FaceInfo *f = impl->face_owner.get_or_null(r);
        if (f && f->parent_camera_rid == target_camera) {
            target_face = r;
            break;
        }
    }
    if (!target_face.is_valid()) return Vector2();

    RID target_eye;
    List<RID> eyes;
    impl->eye_owner.get_owned_list(&eyes);
    for (const RID &r : eyes) {
        EyeInfo *e = impl->eye_owner.get_or_null(r);
        if (e && e->parent_face_rid == target_face) {
            target_eye = r;
            break;
        }
    }
    if (!target_eye.is_valid()) return Vector2();

    EyeInfo *eye = impl->eye_owner.get_or_null(target_eye);
    if (!eye) return Vector2();

    return p_smoothed ? eye->latest_filtered_gaze : eye->latest_projected_gaze;
}

bool GazeServer::is_face_detected(RID p_face) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    FaceInfo *face = impl->face_owner.get_or_null(p_face);
    ERR_FAIL_NULL_V(face, false);
    return face->detected;
}

// Background Worker Loop
void GazeServer::start_processing() {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
#ifndef WEB_ENABLED
    if (pipeline) {
        ProjectSettings *ps = ProjectSettings::get_singleton();
        if (ps) {
            String face_detector_path = ps->has_setting("gaze/models/face_detector_prefix") ? (String)ps->get_setting("gaze/models/face_detector_prefix") : String("face_detection_yunet_2023mar");
            face_detector_path = resolve_model_path(face_detector_path);
            if (face_detector_path.is_empty()) {
                face_detector_path = resolve_model_path("face_detection_yunet_2023mar");
            }

            String eye_openness_path = ps->has_setting("gaze/models/eye_openness_prefix") ? (String)ps->get_setting("gaze/models/eye_openness_prefix") : String("open_closed_eye");
            eye_openness_path = resolve_model_path(eye_openness_path);
            if (eye_openness_path.is_empty()) {
                eye_openness_path = resolve_model_path("open_closed_eye");
            }

            String gaze_path = ps->has_setting("gaze/models/gaze_prefix") ? (String)ps->get_setting("gaze/models/gaze_prefix") : String("gaze-estimation-adas-0002");
            gaze_path = resolve_model_path(gaze_path);
            if (gaze_path.is_empty()) {
                gaze_path = resolve_model_path("gaze-estimation-adas-0002");
            }

            String landmarks_path = ps->has_setting("gaze/models/landmarks_prefix") ? (String)ps->get_setting("gaze/models/landmarks_prefix") : String("facial-landmarks-35-adas-0002");
            landmarks_path = resolve_model_path(landmarks_path);
            if (landmarks_path.is_empty()) {
                landmarks_path = resolve_model_path("facial-landmarks-35-adas-0002");
            }

            std::vector<uint8_t> face_detector_buffer = load_file_buffer(face_detector_path);
            std::vector<uint8_t> eye_openness_buffer = load_file_buffer(eye_openness_path);
            std::vector<uint8_t> gaze_buffer = load_file_buffer(gaze_path);
            std::vector<uint8_t> landmarks_buffer = load_file_buffer(landmarks_path);

            bool init_ok = pipeline->initialize(face_detector_buffer, gaze_buffer, eye_openness_buffer, landmarks_buffer);
            if (!init_ok) {
                Gaze::log_error("GazeServer_StartProcessing_FailedModelInit");
                return;
            }
        }
        pipeline->set_config(active_config);
        pipeline->start();
    }
#endif

    // Connect to Main Loop process_frame
    SceneTree *st = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (st && !st->is_connected("process_frame", Callable(this, "trigger_process"))) {
        st->connect("process_frame", Callable(this, "trigger_process"));
    }
}

void GazeServer::stop_processing() {
    Gaze::log_info(2, "GazeServer_StopProcessing_Began", "active_trackers", active_trackers);
    std::lock_guard<std::recursive_mutex> lock(state_mutex);

#ifndef WEB_ENABLED
    if (pipeline) {
        Gaze::log_info(2, "GazeServer_StopProcessing_PipelineStop_Began");
        pipeline->stop();
        Gaze::log_info(2, "GazeServer_StopProcessing_PipelineStop_Finished");
    }
#endif

    SceneTree *st = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (st && st->is_connected("process_frame", Callable(this, "trigger_process"))) {
        st->disconnect("process_frame", Callable(this, "trigger_process"));
    }

    Gaze::log_info(2, "GazeServer_StopProcessing_Finished");
}

void GazeServer::ref_tracker() {
    start_tracking();
}

void GazeServer::unref_tracker() {
    stop_tracking(false);
}

int GazeServer::get_active_tracker_count() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return active_trackers;
}

void GazeServer::trigger_process() {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);

    Input *input = Input::get_singleton();
    bool processed_camera_frame = false;
    bool face_detected_this_frame = false;

#ifndef WEB_ENABLED
    Gaze::GazeFrameData* completed_data = nullptr;
    while (pipeline && pipeline->pop_result(&completed_data)) {
        if (completed_data && completed_data->userdata) {
            processed_camera_frame = true;
            GazeFrame* gaze_frame = static_cast<GazeFrame*>(completed_data->userdata);

            gaze_frame->set_face_detected(completed_data->face_detected);
            gaze_frame->set_gaze_success(completed_data->gaze_success);
            gaze_frame->set_left_eye_openness(completed_data->left_eye_openness);
            gaze_frame->set_right_eye_openness(completed_data->right_eye_openness);
            gaze_frame->set_timestamp(completed_data->timestamp);

            const Vector3& head_t = reinterpret_cast<const Vector3&>(completed_data->head_translation);
            const Vector3& head_r = reinterpret_cast<const Vector3&>(completed_data->head_rotation);
            gaze_frame->set_head_translation(head_t);
            gaze_frame->set_head_rotation(head_r);

            const Vector3& gaze_o = reinterpret_cast<const Vector3&>(completed_data->gaze_origin);
            const Vector3& gaze_d = reinterpret_cast<const Vector3&>(completed_data->gaze_direction);
            gaze_frame->set_gaze_origin(gaze_o);
            gaze_frame->set_gaze_direction(gaze_d);

            PackedVector2Array lm_array;
            if (completed_data->has_landmarks_2d) {
                lm_array.resize(35);
                for (int i = 0; i < 35; ++i) {
                    lm_array[i] = Vector2(completed_data->landmarks_2d_px[i * 2 + 0], completed_data->landmarks_2d_px[i * 2 + 1]);
                }
            }
            gaze_frame->set_face_landmarks_2d(lm_array);
            gaze_frame->post_process();

            RID face_rid;
            std::memcpy(&face_rid, &completed_data->face_rid_val, sizeof(uint64_t));
            RID eye_rid;
            std::memcpy(&eye_rid, &completed_data->eye_rid_val, sizeof(uint64_t));

            if (face_rid.is_valid()) {
                if (completed_data->face_detected) {
                    face_tracker_set_pose(face_rid, head_t, head_r, true);
                    face_tracker_set_landmarks_2d(face_rid, lm_array);
                } else {
                    face_tracker_set_pose(face_rid, Vector3(), Vector3(), false);
                    face_tracker_set_landmarks_2d(face_rid, PackedVector2Array());
                }
            }
            if (eye_rid.is_valid()) {
                if (completed_data->face_detected) {
                    eye_tracker_set_openness(eye_rid, completed_data->left_eye_openness, completed_data->right_eye_openness);
                    set_crops_on_eye_tracker(eye_rid, gaze_frame->get_left_eye_crop(), gaze_frame->get_right_eye_crop());
                    if (completed_data->gaze_success) {
                        eye_tracker_set_gaze(eye_rid, gaze_o, gaze_d);
                    } else {
                        reset_eye_tracker(eye_rid);
                    }
                } else {
                    reset_eye_tracker(eye_rid);
                }
            }

            emit_signal("gaze_frame_began", gaze_frame);

            if (completed_data->face_detected) {
                face_detected_this_frame = true;
                Ref<InputEventGaze> event;
                event.instantiate();
                event->set_frame_id(++current_frame_id);
                event->set_timestamp_usec(Time::get_singleton()->get_ticks_usec());
                event->set_left_eye_openness(completed_data->left_eye_openness);
                event->set_right_eye_openness(completed_data->right_eye_openness);

                Vector2 local_pos = eye_rid.is_valid() ? get_projected_gaze_from_eye_tracker(eye_rid, true) : Vector2(0, 0);
                Vector2 screen_pos = local_pos;
                DisplayServer* ds = DisplayServer::get_singleton();
                if (ds) {
                    screen_pos += ds->window_get_position();
                }

                uint64_t now_usec = Time::get_singleton()->get_ticks_usec();
                float dt = (last_event_time_usec > 0 && now_usec > last_event_time_usec) ? (float)(now_usec - last_event_time_usec) / 1000000.0f : 0.016667f;
                if (dt < 0.0001f) dt = 0.0001f;

                Vector2 rel = local_pos - last_gaze_pos;
                Vector2 screen_rel = screen_pos - last_screen_pos;
                Vector2 vel = rel / dt;
                Vector2 screen_vel = screen_rel / dt;

                last_gaze_pos = local_pos;
                last_screen_pos = screen_pos;
                last_event_time_usec = now_usec;

                event->set_position(local_pos);
                event->set_global_position(local_pos);
                event->set_screen_position(screen_pos);
                event->set_relative(rel);
                event->set_screen_relative(screen_rel);
                event->set_velocity(vel);
                event->set_screen_velocity(screen_vel);

                Basis head_basis = Basis::from_euler(head_r);
                event->set_head_transform(Transform3D(head_basis, head_t));

                Vector3 norm_gaze_dir = gaze_d.is_normalized() ? gaze_d : gaze_d.normalized();
                Basis gaze_basis = Basis::looking_at(norm_gaze_dir, Vector3(0, 1, 0));
                event->set_gaze_transform(Transform3D(gaze_basis, gaze_o));

                most_recent_event = event;
                if (input) {
                    input->parse_input_event(event);
                }

                if (emulate_mouse_from_gaze && input) {
                    Ref<InputEventMouseMotion> mm;
                    mm.instantiate();
                    mm->set_position(local_pos);
                    mm->set_global_position(local_pos);
                    mm->set_relative(rel);
                    mm->set_velocity(vel);
                    input->parse_input_event(mm);

                    bool left_closed = (completed_data->left_eye_openness < 0.25f);
                    bool right_closed = (completed_data->right_eye_openness < 0.25f);
                    if (left_closed && right_closed) {
                        if (!was_both_closed) {
                            Ref<InputEventMouseButton> mb;
                            mb.instantiate();
                            mb->set_button_index(MouseButton::MOUSE_BUTTON_LEFT);
                            mb->set_pressed(true);
                            mb->set_position(local_pos);
                            mb->set_global_position(local_pos);
                            input->parse_input_event(mb);
                            was_both_closed = true;
                        }
                    } else if (was_both_closed) {
                        Ref<InputEventMouseButton> mb;
                        mb.instantiate();
                        mb->set_button_index(MouseButton::MOUSE_BUTTON_LEFT);
                        mb->set_pressed(false);
                        mb->set_position(local_pos);
                        mb->set_global_position(local_pos);
                        input->parse_input_event(mb);
                        was_both_closed = false;
                    }
                }
            } else {
                Ref<InputEventGazeMissing> missing;
                missing.instantiate();
                missing->set_frame_id(++current_frame_id);
                missing->set_timestamp_usec(Time::get_singleton()->get_ticks_usec());
                missing->set_reason(InputEventGazeMissing::REASON_NO_FACE_DETECTED);
                most_recent_event = missing;
                if (input) {
                    input->parse_input_event(missing);
                }
            }

            if (active_read_data) {
                pipeline->frame_pool.release(active_read_data);
            }
            active_read_data = completed_data;

            emit_signal("gaze_frame_ready", gaze_frame);
        }
    }

    // Grab current frames from VisionServer and push to GazeTrackingPipeline
    if (pipeline && !pipeline->is_busy()) {
        List<RID> cameras;
        impl->camera_owner.get_owned_list(&cameras);
        for (const RID &r : cameras) {
            CameraInfo *cam = impl->camera_owner.get_or_null(r);
            if (cam && cam->vision_camera_rid.is_valid()) {
                Gaze::Frame current_frame;
                bool has_frame = VisionServer::get_singleton()->get_camera_current_frame(cam->vision_camera_rid, current_frame);
                if (has_frame) {
                    Gaze::GazeFrameData* write_data = pipeline->frame_pool.take();
                    if (write_data) {
                        size_t frame_bytes = current_frame.width * current_frame.height * 3;
                        write_data->camera_raw_bgr.resize(frame_bytes);
                        std::memcpy(write_data->camera_raw_bgr.data(), current_frame.data, frame_bytes);
                        write_data->camera_width = current_frame.width;
                        write_data->camera_height = current_frame.height;
                        write_data->timestamp = current_frame.timestamp;
                        write_data->camera_focal_length_px = VisionServer::get_singleton()->camera_get_focal_length(cam->vision_camera_rid);
                        write_data->camera_fov_degrees = VisionServer::get_singleton()->camera_get_fov(cam->vision_camera_rid);

                        RID face_rid;
                        List<RID> faces;
                        impl->face_owner.get_owned_list(&faces);
                        for (const RID &f : faces) {
                            FaceInfo *face = impl->face_owner.get_or_null(f);
                            if (face && face->parent_camera_rid == r) {
                                face_rid = f;
                                break;
                            }
                        }

                        RID eye_rid;
                        if (face_rid.is_valid()) {
                            List<RID> eyes;
                            impl->eye_owner.get_owned_list(&eyes);
                            for (const RID &e : eyes) {
                                EyeInfo *eye = impl->eye_owner.get_or_null(e);
                                if (eye && eye->parent_face_rid == face_rid) {
                                    eye_rid = e;
                                    break;
                                }
                            }
                        }

                        write_data->face_rid_val = face_rid.get_id();
                        write_data->eye_rid_val = eye_rid.get_id();

                        GazeFrame* wrapper = static_cast<GazeFrame*>(write_data->userdata);
                        if (wrapper) {
                            wrapper->set_camera_size(Vector2i(current_frame.width, current_frame.height));
                            write_data->left_eye_buffer = wrapper->get_left_eye_buffer_ptr();
                            write_data->right_eye_buffer = wrapper->get_right_eye_buffer_ptr();
                            wrapper->resize_full_crop(160, 128);
                            write_data->full_crop_buffer = wrapper->get_full_crop_buffer_ptr();
                            write_data->full_crop_bytes = wrapper->get_full_crop_bytes_size();
                        }

                        pipeline->push_frame_request(write_data);
                        emit_camera_frame_ready(cam->vision_camera_rid);
                    }
                }
            }
        }
    }
#endif

    // Mouse-to-Gaze Emulation when no face is actively tracked
    if (!face_detected_this_frame && emulate_gaze_from_mouse && input) {
        DisplayServer* ds = nullptr;
        if (Engine::get_singleton()->has_singleton("DisplayServer")) {
            ds = DisplayServer::get_singleton();
        }
        Vector2 mouse_pos = ds ? Vector2(ds->mouse_get_position() - ds->window_get_position()) : Vector2(0, 0);

        Ref<DisplayProfile> dp = get_display_profile();
        if (!dp.is_valid()) dp = DisplayProfile::estimate_from_os();
        Vector2i log_sz = dp.is_valid() ? dp->get_logical_size_px() : Vector2i(1920, 1080);
        Vector2 phys_sz = dp.is_valid() ? dp->get_physical_size_mm() : Vector2(345.0, 215.0);
        if (log_sz.x <= 0) log_sz.x = 1920;
        if (log_sz.y <= 0) log_sz.y = 1080;
        if (phys_sz.x <= 0.0) phys_sz.x = 345.0;
        if (phys_sz.y <= 0.0) phys_sz.y = 215.0;

        float x_s = (mouse_pos.x / log_sz.x - 0.5f) * phys_sz.x;
        float y_s = (mouse_pos.y / log_sz.y - 0.5f) * phys_sz.y;
        Vector3 target_3d(x_s, y_s, 0.0f);
        Vector3 eye_origin_3d(0.0f, 0.0f, 500.0f);
        Vector3 gaze_dir = (target_3d - eye_origin_3d).normalized();

        Ref<InputEventGaze> syn_event;
        syn_event.instantiate();
        syn_event->set_frame_id(++current_frame_id);
        syn_event->set_timestamp_usec(Time::get_singleton()->get_ticks_usec());
        syn_event->set_left_eye_openness(1.0f);
        syn_event->set_right_eye_openness(1.0f);

        Vector2 screen_pos = mouse_pos;
        if (ds) screen_pos += Vector2(ds->window_get_position());

        uint64_t now_usec = Time::get_singleton()->get_ticks_usec();
        float dt = (last_event_time_usec > 0 && now_usec > last_event_time_usec) ? (float)(now_usec - last_event_time_usec) / 1000000.0f : 0.016667f;
        if (dt < 0.0001f) dt = 0.0001f;

        Vector2 rel = mouse_pos - last_gaze_pos;
        Vector2 vel = rel / dt;

        syn_event->set_position(mouse_pos);
        syn_event->set_global_position(mouse_pos);
        syn_event->set_screen_position(screen_pos);
        syn_event->set_relative(rel);
        syn_event->set_screen_relative(rel);
        syn_event->set_velocity(vel);
        syn_event->set_screen_velocity(vel);

        last_gaze_pos = mouse_pos;
        last_screen_pos = screen_pos;
        last_event_time_usec = now_usec;

        syn_event->set_head_transform(Transform3D(Basis(), Vector3(0.0f, 0.0f, 500.0f)));
        syn_event->set_gaze_transform(Transform3D(Basis::looking_at(gaze_dir, Vector3(0, 1, 0)), eye_origin_3d));

        most_recent_event = syn_event;
        input->parse_input_event(syn_event);
    }
}

void GazeServer::set_pipeline_config(const Ref<GazePipelineConfig>& p_config) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    if (p_config.is_valid()) {
        active_config = p_config->get_config();
#ifndef WEB_ENABLED
        if (pipeline) {
            pipeline->set_config(active_config);
        }
#endif
    }
}

#ifdef WEB_ENABLED
void GazeServer::feed_gaze_web_raw(const Array& args) {
    // Web implementation handler
}
#endif

void GazeServer::set_verbosity(int level) {
    Gaze::set_log_verbosity(level);
}

int GazeServer::get_verbosity() const {
    return Gaze::get_log_verbosity();
}

String GazeServer::get_build_info() {
    return String("godot-gaze Native GDExtension v4.8");
}

String GazeServer::get_build_timestamp() {
    return String(__DATE__ " " __TIME__);
}

} // namespace godot
