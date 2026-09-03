#include "gaze_server.hpp"
#include "vision_server.hpp"
#include "log.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/main_loop.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <cmath>
#include <vector>
#include <algorithm>
#include "godot_files.hpp"
#include "../core/opencv_space_conversions.hpp"

#ifdef WEB_ENABLED
#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/java_script_object.hpp>
#endif

namespace godot {

// Structs definitions inside GazeServerImpl for direct singleton state management
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
    } display;

    struct CameraInfo {
        Vector3 offset = Vector3(0.0, 107.5, 0.0);
        double tilt = 0.0;
        RID vision_camera_rid;
    } camera;

    struct FaceInfo {
        Transform3D relative_transform;
        bool detected = false;
        
        Gaze::GodotCameraVector3 head_pose_translation;
        Gaze::GodotCameraVector3 head_pose_rotation;
        PackedVector2Array landmarks_2d;
        float roll_hint_rad = 0.0f;
        bool auto_roll_enabled = true;
    } face;

    struct EyeInfo {
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
    } eye;

    Ref<GazePipelineConfig> pipeline_config;
};

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
    ClassDB::bind_method(D_METHOD("ensure_process_connected"), &GazeServer::ensure_process_connected);
    ClassDB::bind_method(D_METHOD("trigger_process"), &GazeServer::trigger_process);
    ClassDB::bind_method(D_METHOD("start_processing"), &GazeServer::start_processing);
    ClassDB::bind_method(D_METHOD("stop_processing"), &GazeServer::stop_processing);
    ClassDB::bind_method(D_METHOD("reset"), &GazeServer::reset);
    ClassDB::bind_method(D_METHOD("get_active_tracker_count"), &GazeServer::get_active_tracker_count);

    // Calibration & Hardware Configuration
    ClassDB::bind_method(D_METHOD("set_device_calibration", "calibration"), &GazeServer::set_device_calibration);
    ClassDB::bind_method(D_METHOD("get_device_calibration"), &GazeServer::get_device_calibration);
    ClassDB::bind_method(D_METHOD("set_bio_calibration", "calibration"), &GazeServer::set_bio_calibration);
    ClassDB::bind_method(D_METHOD("get_bio_calibration"), &GazeServer::get_bio_calibration);
    ClassDB::bind_method(D_METHOD("set_camera_offsets", "offset", "tilt_deg"), &GazeServer::set_camera_offsets);
    ClassDB::bind_method(D_METHOD("get_camera_offset"), &GazeServer::get_camera_offset);
    ClassDB::bind_method(D_METHOD("get_camera_tilt"), &GazeServer::get_camera_tilt);
    ClassDB::bind_method(D_METHOD("set_camera_vision_rid", "vision_camera_rid"), &GazeServer::set_camera_vision_rid);
    ClassDB::bind_method(D_METHOD("get_camera_vision_rid"), &GazeServer::get_camera_vision_rid);
    ClassDB::bind_method(D_METHOD("set_pipeline_config", "config"), &GazeServer::set_pipeline_config);
    ClassDB::bind_method(D_METHOD("get_pipeline_config"), &GazeServer::get_pipeline_config);

    // Face Tracking & Head Pose
    ClassDB::bind_method(D_METHOD("is_face_detected"), &GazeServer::is_face_detected);
    ClassDB::bind_method(D_METHOD("get_head_transform"), &GazeServer::get_head_transform);
    ClassDB::bind_method(D_METHOD("get_head_position"), &GazeServer::get_head_position);
    ClassDB::bind_method(D_METHOD("get_head_rotation"), &GazeServer::get_head_rotation);
    ClassDB::bind_method(D_METHOD("get_head_pose_origin_mm"), &GazeServer::get_head_pose_origin_mm);
    ClassDB::bind_method(D_METHOD("get_head_pose_euler_deg"), &GazeServer::get_head_pose_euler_deg);
    ClassDB::bind_method(D_METHOD("set_face_pose", "translation", "rotation", "detected"), &GazeServer::set_face_pose);
    ClassDB::bind_method(D_METHOD("set_face_transform", "transform", "rotation", "detected"), &GazeServer::set_face_transform);
    ClassDB::bind_method(D_METHOD("get_face_landmarks_2d"), &GazeServer::get_face_landmarks_2d);
    ClassDB::bind_method(D_METHOD("get_face_landmarks"), &GazeServer::get_face_landmarks);
    ClassDB::bind_method(D_METHOD("get_debug_landmarks"), &GazeServer::get_debug_landmarks);
    ClassDB::bind_method(D_METHOD("set_face_landmarks_2d", "landmarks"), &GazeServer::set_face_landmarks_2d);
    ClassDB::bind_method(D_METHOD("get_face_model_points"), &GazeServer::get_face_model_points);
    ClassDB::bind_method(D_METHOD("set_roll_hint", "roll_hint_rad"), &GazeServer::set_roll_hint);
    ClassDB::bind_method(D_METHOD("get_roll_hint"), &GazeServer::get_roll_hint);
    ClassDB::bind_method(D_METHOD("set_auto_roll_enabled", "enabled"), &GazeServer::set_auto_roll_enabled);
    ClassDB::bind_method(D_METHOD("is_auto_roll_enabled"), &GazeServer::is_auto_roll_enabled);

    // Eye & Gaze Tracking
    ClassDB::bind_method(D_METHOD("is_gaze_detected"), &GazeServer::is_gaze_detected);
    ClassDB::bind_method(D_METHOD("get_gaze_origin"), &GazeServer::get_gaze_origin);
    ClassDB::bind_method(D_METHOD("get_gaze_direction"), &GazeServer::get_gaze_direction);
    ClassDB::bind_method(D_METHOD("set_gaze", "origin_cam", "direction_cam"), &GazeServer::set_gaze);
    ClassDB::bind_method(D_METHOD("get_gaze_screen_px", "smoothed"), &GazeServer::get_gaze_screen_px, DEFVAL(true));
    ClassDB::bind_method(D_METHOD("get_gaze_screen_mm", "smoothed"), &GazeServer::get_gaze_screen_mm, DEFVAL(true));
    ClassDB::bind_method(D_METHOD("get_gaze_screen", "smoothed"), &GazeServer::get_gaze_screen, DEFVAL(true));
    ClassDB::bind_method(D_METHOD("get_projected_gaze", "smoothed"), &GazeServer::get_projected_gaze, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("get_projected_gaze_mm", "smoothed"), &GazeServer::get_projected_gaze_mm, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("get_left_eye_openness"), &GazeServer::get_left_eye_openness);
    ClassDB::bind_method(D_METHOD("get_right_eye_openness"), &GazeServer::get_right_eye_openness);
    ClassDB::bind_method(D_METHOD("set_eye_openness", "left", "right"), &GazeServer::set_eye_openness);
    ClassDB::bind_method(D_METHOD("set_smoother", "smoother"), &GazeServer::set_smoother);
    ClassDB::bind_method(D_METHOD("get_smoother"), &GazeServer::get_smoother);
    ClassDB::bind_method(D_METHOD("set_crop_requested", "requested"), &GazeServer::set_crop_requested);
    ClassDB::bind_method(D_METHOD("is_crop_requested"), &GazeServer::is_crop_requested);
    ClassDB::bind_method(D_METHOD("get_eye_crops"), &GazeServer::get_eye_crops);
    ClassDB::bind_method(D_METHOD("set_eye_crops", "left_crop", "right_crop"), &GazeServer::set_eye_crops);
    ClassDB::bind_method(D_METHOD("get_camera_texture"), &GazeServer::get_camera_texture);
    ClassDB::bind_method(D_METHOD("set_camera_preview_requested", "requested"), &GazeServer::set_camera_preview_requested);
    ClassDB::bind_method(D_METHOD("camera_set_preview_requested", "requested"), &GazeServer::set_camera_preview_requested);
    ClassDB::bind_method(D_METHOD("is_camera_preview_requested"), &GazeServer::is_camera_preview_requested);
    ClassDB::bind_method(D_METHOD("emit_camera_frame_ready", "vision_camera_rid"), &GazeServer::emit_camera_frame_ready);

    // Ray Projection Math
    ClassDB::bind_method(D_METHOD("project_ray_to_viewport", "origin_cam", "direction_cam", "apply_bio_calibration"), &GazeServer::project_ray_to_viewport, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("project_ray_to_screen_mm", "origin_cam", "direction_cam"), &GazeServer::project_ray_to_screen_mm);

    // Event Factory & Emulation
    ClassDB::bind_method(D_METHOD("set_event_factory", "factory"), &GazeServer::set_event_factory);
    ClassDB::bind_method(D_METHOD("get_event_factory"), &GazeServer::get_event_factory);
    ClassDB::bind_method(D_METHOD("create_default_event"), &GazeServer::create_default_event);
    ClassDB::bind_method(D_METHOD("create_default_missing_event", "reason"), &GazeServer::create_default_missing_event);

    ClassDB::bind_method(D_METHOD("set_emulate_gaze_from_mouse", "enable"), &GazeServer::set_emulate_gaze_from_mouse);
    ClassDB::bind_method(D_METHOD("get_emulate_gaze_from_mouse"), &GazeServer::get_emulate_gaze_from_mouse);
    ClassDB::bind_method(D_METHOD("set_emulate_mouse_from_gaze", "enable"), &GazeServer::set_emulate_mouse_from_gaze);
    ClassDB::bind_method(D_METHOD("get_emulate_mouse_from_gaze"), &GazeServer::get_emulate_mouse_from_gaze);
    ClassDB::bind_method(D_METHOD("set_mouse_emulation_dwell_sec", "sec"), &GazeServer::set_mouse_emulation_dwell_sec);
    ClassDB::bind_method(D_METHOD("get_mouse_emulation_dwell_sec"), &GazeServer::get_mouse_emulation_dwell_sec);
    ClassDB::bind_method(D_METHOD("set_mouse_emulation_transition_sec", "sec"), &GazeServer::set_mouse_emulation_transition_sec);
    ClassDB::bind_method(D_METHOD("get_mouse_emulation_transition_sec"), &GazeServer::get_mouse_emulation_transition_sec);

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

    Ref<OneEuroSmoother> sm;
    sm.instantiate();
    impl->eye.screen_smoother = sm;

    Ref<DefaultDeviceCalibration> def_dev;
    def_dev.instantiate();
    set_device_calibration(def_dev);

    Ref<DefaultBioCalibration> def_bio;
    def_bio.instantiate();
    set_bio_calibration(def_bio);

    VisionServer *vs = VisionServer::get_singleton();
    if (vs) {
        RID default_vision_cam = vs->camera_create();
        impl->camera.vision_camera_rid = default_vision_cam;
    }

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
        if (ps->has_setting("gaze/pointing/mouse_emulation_dwell_sec")) {
            mouse_emulation.set_dwell_time_sec(ps->get_setting("gaze/pointing/mouse_emulation_dwell_sec"));
        }
        if (ps->has_setting("gaze/pointing/mouse_emulation_transition_sec")) {
            mouse_emulation.set_transition_duration_sec(ps->get_setting("gaze/pointing/mouse_emulation_transition_sec"));
        }
    }

    if (event_factory.is_null()) {
        Ref<GazeServerEventFactory> def_factory;
        def_factory.instantiate();
        event_factory = def_factory;
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
}

GazeServer::~GazeServer() {
    Gaze::log_info(2, "GazeServer_Destructor_Began");
    {
        std::lock_guard<std::recursive_mutex> lock(state_mutex);
        active_trackers = 0;
    }
    stop_processing();

#ifndef WEB_ENABLED
    Gaze::log_info(2, "GazeServer_Destructor_PipelineReset_Began");
    if (pipeline) {
        for (size_t i = 0; i < pipeline->frame_pool.size; ++i) {
            Gaze::GazeFrameData* data = pipeline->frame_pool.get_frame(i);
            if (data && data->userdata) {
                GazeFrame* wrapper = static_cast<GazeFrame*>(data->userdata);
                memdelete(wrapper);
                data->userdata = nullptr;
            }
        }
        pipeline.reset();
    }
    Gaze::log_info(2, "GazeServer_Destructor_PipelineReset_Finished");
#endif

    singleton = nullptr;
    Gaze::log_info(2, "GazeServer_Destructor_Finished");
}

void GazeServer::ensure_process_connected() {
    SceneTree *st = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (st && !st->is_connected("process_frame", Callable(this, "trigger_process"))) {
        st->connect("process_frame", Callable(this, "trigger_process"));
    }
}

void GazeServer::start_processing() {
    Gaze::log_info(2, "GazeServer_StartProcessing_Began");
    std::lock_guard<std::recursive_mutex> lock(state_mutex);

#ifndef WEB_ENABLED
    if (pipeline) {
        if (!pipeline->is_initialized()) {
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
        }
        pipeline->set_config(active_config);
        pipeline->start();
    }
#endif

    VisionServer *vs = VisionServer::get_singleton();
    if (vs && impl->camera.vision_camera_rid.is_valid()) {
        vs->camera_start(impl->camera.vision_camera_rid);
    }

    ensure_process_connected();
    Gaze::log_info(2, "GazeServer_StartProcessing_Finished");
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

    VisionServer *vs = VisionServer::get_singleton();
    if (vs && impl->camera.vision_camera_rid.is_valid()) {
        vs->camera_stop(impl->camera.vision_camera_rid);
    }

    SceneTree *st = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (st && st->is_connected("process_frame", Callable(this, "trigger_process"))) {
        st->disconnect("process_frame", Callable(this, "trigger_process"));
    }

    last_camera_face_detected_usec = 0;
    Gaze::log_info(2, "GazeServer_StopProcessing_Finished");
}

bool GazeServer::start_tracking() {
    bool was_zero = false;
    {
        std::lock_guard<std::recursive_mutex> lock(state_mutex);
        active_trackers++;
        if (active_trackers == 1) {
            was_zero = true;
            start_processing();
        } else {
            VisionServer *vs = VisionServer::get_singleton();
            if (vs && impl->camera.vision_camera_rid.is_valid() && !vs->camera_is_active(impl->camera.vision_camera_rid)) {
                vs->camera_start(impl->camera.vision_camera_rid);
            }
            ensure_process_connected();
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

void GazeServer::reset() {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->face = FaceInfo();
    impl->eye.gaze_origin_cam = Vector3();
    impl->eye.gaze_direction_cam = Vector3();
    impl->eye.smoother_state.clear();
    impl->eye.latest_projected_gaze = Vector2();
    impl->eye.latest_filtered_gaze = Vector2();
    impl->eye.latest_projected_gaze_mm = Vector2();
    impl->eye.latest_filtered_gaze_mm = Vector2();
    impl->eye.left_eye_openness = 1.0f;
    impl->eye.right_eye_openness = 1.0f;
    impl->eye.left_eye_crop.unref();
    impl->eye.right_eye_crop.unref();
}

int GazeServer::get_active_tracker_count() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return active_trackers;
}

// --- Calibrations & Hardware Setup ---

void GazeServer::set_device_calibration(const Ref<DeviceCalibration>& p_calibration) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->display.device_calibration = p_calibration;
    if (p_calibration.is_valid()) {
        impl->display.logical_size_px = p_calibration->get_logical_size_px();
        impl->display.physical_size_mm = p_calibration->get_physical_size_mm();
        impl->camera.offset = p_calibration->get_camera_offset();
        impl->camera.tilt = p_calibration->get_camera_tilt();
    }
}

Ref<DeviceCalibration> GazeServer::get_device_calibration() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->display.device_calibration;
}

void GazeServer::set_bio_calibration(const Ref<BioCalibration>& p_calibration) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->display.bio_calibration = p_calibration;
    if (p_calibration.is_valid()) {
        impl->display.bio_data.is_valid = true;
        impl->display.bio_data.bias_pitch = p_calibration->get_bias_pitch();
        impl->display.bio_data.bias_yaw = p_calibration->get_bias_yaw();
        impl->display.bio_data.scale_yaw = p_calibration->get_scale_yaw();
        impl->display.bio_data.scale_pitch = p_calibration->get_scale_pitch();
    } else {
        impl->display.bio_data.is_valid = false;
    }
}

Ref<BioCalibration> GazeServer::get_bio_calibration() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->display.bio_calibration;
}

void GazeServer::set_camera_offsets(Vector3 p_offset, double p_tilt) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->camera.offset = p_offset;
    impl->camera.tilt = p_tilt;
}

Vector3 GazeServer::get_camera_offset() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->camera.offset;
}

double GazeServer::get_camera_tilt() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->camera.tilt;
}

void GazeServer::set_camera_vision_rid(RID p_vision_camera) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->camera.vision_camera_rid = p_vision_camera;
}

RID GazeServer::get_camera_vision_rid() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->camera.vision_camera_rid;
}

void GazeServer::set_pipeline_config(const Ref<GazePipelineConfig>& p_config) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->pipeline_config = p_config;
    if (p_config.is_valid()) {
        active_config = p_config->get_config();
#ifndef WEB_ENABLED
        if (pipeline) {
            pipeline->set_config(active_config);
        }
#endif
    }
}

Ref<GazePipelineConfig> GazeServer::get_pipeline_config() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->pipeline_config;
}

// --- Face & Head Pose Tracking ---

bool GazeServer::is_face_detected() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->face.detected;
}

Transform3D GazeServer::get_head_transform() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->face.relative_transform;
}

Vector3 GazeServer::get_head_position() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return Vector3(impl->face.head_pose_translation.x, impl->face.head_pose_translation.y, impl->face.head_pose_translation.z);
}

Vector3 GazeServer::get_head_rotation() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return Vector3(impl->face.head_pose_rotation.x, impl->face.head_pose_rotation.y, impl->face.head_pose_rotation.z);
}

Vector3 GazeServer::get_head_pose_origin_mm() const {
    return get_head_position();
}

Vector3 GazeServer::get_head_pose_euler_deg() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return Vector3(Math::rad_to_deg(impl->face.head_pose_rotation.x), Math::rad_to_deg(impl->face.head_pose_rotation.y), Math::rad_to_deg(impl->face.head_pose_rotation.z));
}

void GazeServer::set_face_pose(Vector3 p_translation, Vector3 p_rotation, bool p_detected) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->face.detected = p_detected;
    impl->face.head_pose_translation = Gaze::GodotCameraVector3(p_translation.x, p_translation.y, p_translation.z);
    impl->face.head_pose_rotation = Gaze::GodotCameraVector3(p_rotation.x, p_rotation.y, p_rotation.z);
    if (p_detected) {
        Basis b = Basis::from_euler(p_rotation);
        impl->face.relative_transform = Transform3D(b, p_translation);
    } else {
        impl->face.relative_transform = Transform3D();
    }
}

void GazeServer::set_face_transform(const Transform3D &p_transform, const Vector3 &p_rotation, bool p_detected) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->face.detected = p_detected;
    impl->face.head_pose_translation = Gaze::GodotCameraVector3(p_transform.origin.x, p_transform.origin.y, p_transform.origin.z);
    impl->face.head_pose_rotation = Gaze::GodotCameraVector3(p_rotation.x, p_rotation.y, p_rotation.z);
    impl->face.relative_transform = p_detected ? p_transform : Transform3D();
}

PackedVector2Array GazeServer::get_face_landmarks_2d() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->face.landmarks_2d;
}

PackedVector2Array GazeServer::get_face_landmarks() const {
    return get_face_landmarks_2d();
}

PackedVector2Array GazeServer::get_debug_landmarks() const {
    return get_face_landmarks_2d();
}

void GazeServer::set_face_landmarks_2d(const PackedVector2Array &p_landmarks) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->face.landmarks_2d = p_landmarks;
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

void GazeServer::set_roll_hint(float p_roll_hint_rad) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->face.roll_hint_rad = p_roll_hint_rad;
    impl->face.auto_roll_enabled = false;
}

float GazeServer::get_roll_hint() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->face.roll_hint_rad;
}

void GazeServer::set_auto_roll_enabled(bool p_enabled) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->face.auto_roll_enabled = p_enabled;
}

bool GazeServer::is_auto_roll_enabled() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->face.auto_roll_enabled;
}

// --- Eye & Gaze Tracking ---

bool GazeServer::is_gaze_detected() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->face.detected && (impl->eye.gaze_direction_cam.length_squared() > 0.001f);
}

Vector3 GazeServer::get_gaze_origin() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->eye.gaze_origin_cam;
}

Vector3 GazeServer::get_gaze_direction() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->eye.gaze_direction_cam;
}

void GazeServer::set_gaze(Vector3 p_origin_cam, Vector3 p_direction_cam) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->eye.gaze_origin_cam = p_origin_cam;
    impl->eye.gaze_direction_cam = p_direction_cam;

    Ref<DeviceCalibration> dev_cal = impl->display.device_calibration;
    Vector2 physical_sz = dev_cal.is_valid() ? dev_cal->get_physical_size_mm() : Vector2(345.0, 215.0);
    Vector2i logical_sz = dev_cal.is_valid() ? dev_cal->get_logical_size_px() : Vector2i(1920, 1080);
    Vector3 effective_offset = dev_cal.is_valid() ? dev_cal->get_camera_offset() : impl->camera.offset;
    double effective_tilt = dev_cal.is_valid() ? dev_cal->get_camera_tilt() : impl->camera.tilt;
    Vector2 win_pos = dev_cal.is_valid() ? dev_cal->get_window_position_lpix() : Vector2(0.0, 0.0);

    Vector3 calibrated_dir = p_direction_cam;
    if (impl->display.bio_data.is_valid) {
        Gaze::GodotCameraVector3 raw_dir(p_direction_cam.x, p_direction_cam.y, p_direction_cam.z);
        Gaze::GodotCameraVector3 calib_v = Gaze::apply_3d_bias_vector(
            raw_dir,
            Gaze::SpacedVector2<Gaze::Space::GodotCameraEuler>(impl->display.bio_data.bias_pitch, impl->display.bio_data.bias_yaw),
            Gaze::SpacedVector2<Gaze::Space::GodotCameraEuler>(impl->display.bio_data.scale_pitch, impl->display.bio_data.scale_yaw)
        );
        calibrated_dir = Vector3(calib_v.x, calib_v.y, calib_v.z);
    }

    Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm> pos_mm;
    Gaze::GodotCameraVector3 origin_godot(p_origin_cam.x, p_origin_cam.y, p_origin_cam.z);
    Gaze::GodotCameraVector3 dir_godot(calibrated_dir.x, calibrated_dir.y, calibrated_dir.z);
    if (Gaze::project_ray_to_screen_mm(
            origin_godot,
            dir_godot,
            Gaze::GodotCameraVector3(effective_offset.x, effective_offset.y, effective_offset.z),
            effective_tilt,
            Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(physical_sz.x, physical_sz.y),
            pos_mm
        )) {
        double scale_x = (double)logical_sz.x / physical_sz.x;
        double scale_y = (double)logical_sz.y / physical_sz.y;
        Vector2 px(pos_mm.x * scale_x, pos_mm.y * scale_y);

        px = px - win_pos;

        impl->eye.latest_projected_gaze = px;
        Vector2 pos_mm_center(pos_mm.x - physical_sz.x * 0.5, pos_mm.y - physical_sz.y * 0.5);
        impl->eye.latest_projected_gaze_mm = pos_mm_center;
        
        if (impl->eye.screen_smoother.is_valid()) {
            auto now = std::chrono::steady_clock::now();
            double tstamp = std::chrono::duration<double>(now.time_since_epoch()).count();
            impl->eye.latest_filtered_gaze = impl->eye.screen_smoother->_smoother_next(impl->eye.smoother_state, tstamp, px);
            impl->eye.latest_filtered_gaze_mm = impl->eye.latest_filtered_gaze;
        } else {
            impl->eye.latest_filtered_gaze = px;
            impl->eye.latest_filtered_gaze_mm = pos_mm_center;
        }
    }
}

Vector2 GazeServer::get_gaze_screen_px(bool p_smoothed) const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return p_smoothed ? impl->eye.latest_filtered_gaze : impl->eye.latest_projected_gaze;
}

Vector2 GazeServer::get_gaze_screen_mm(bool p_smoothed) const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return p_smoothed ? impl->eye.latest_filtered_gaze_mm : impl->eye.latest_projected_gaze_mm;
}

Vector2 GazeServer::get_gaze_screen(bool p_smoothed) const {
    return get_gaze_screen_px(p_smoothed);
}

Vector2 GazeServer::get_projected_gaze(bool p_smoothed) const {
    return get_gaze_screen_px(p_smoothed);
}

Vector2 GazeServer::get_projected_gaze_mm(bool p_smoothed) const {
    return get_gaze_screen_mm(p_smoothed);
}

float GazeServer::get_left_eye_openness() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->eye.left_eye_openness;
}

float GazeServer::get_right_eye_openness() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->eye.right_eye_openness;
}

void GazeServer::set_eye_openness(float p_left, float p_right) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->eye.left_eye_openness = p_left;
    impl->eye.right_eye_openness = p_right;
}

void GazeServer::set_smoother(const Ref<Smoother>& p_smoother) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->eye.screen_smoother = p_smoother;
    impl->eye.smoother_state.clear();
}

Ref<Smoother> GazeServer::get_smoother() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->eye.screen_smoother;
}

void GazeServer::set_crop_requested(bool p_requested) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->eye.crop_requested = p_requested;
}

bool GazeServer::is_crop_requested() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return impl->eye.crop_requested;
}

Array GazeServer::get_eye_crops() {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    Array crops;
    crops.push_back(impl->eye.left_eye_crop);
    crops.push_back(impl->eye.right_eye_crop);
    return crops;
}

void GazeServer::set_eye_crops(const Ref<Image>& p_left_crop, const Ref<Image>& p_right_crop) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    impl->eye.left_eye_crop = p_left_crop;
    impl->eye.right_eye_crop = p_right_crop;
}

Ref<Texture2D> GazeServer::get_camera_texture() {
    VisionServer* vs = VisionServer::get_singleton();
    if (!vs || !impl->camera.vision_camera_rid.is_valid()) return Ref<Texture2D>();
    return vs->get_camera_current_texture(impl->camera.vision_camera_rid);
}

void GazeServer::set_camera_preview_requested(bool p_requested) {
    VisionServer* vs = VisionServer::get_singleton();
    if (vs && impl->camera.vision_camera_rid.is_valid()) {
        vs->camera_set_preview_requested(impl->camera.vision_camera_rid, p_requested);
    }
    set_crop_requested(p_requested);
}

bool GazeServer::is_camera_preview_requested() const {
    VisionServer* vs = VisionServer::get_singleton();
    if (!vs || !impl->camera.vision_camera_rid.is_valid()) return false;
    return vs->camera_is_preview_requested(impl->camera.vision_camera_rid);
}

void GazeServer::emit_camera_frame_ready(RID p_vision_camera) {
    emit_signal("gaze_data_ready", p_vision_camera);
}

// --- Ray Projection Math ---

Vector2 GazeServer::project_ray_to_viewport(const Vector3 &p_origin_cam, const Vector3 &p_direction_cam, bool p_apply_bio_calibration) const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    Ref<DeviceCalibration> dev_cal = impl->display.device_calibration;
    Vector2 physical_sz = dev_cal.is_valid() ? dev_cal->get_physical_size_mm() : Vector2(345.0, 215.0);
    Vector2i logical_sz = dev_cal.is_valid() ? dev_cal->get_logical_size_px() : Vector2i(1920, 1080);
    Vector3 effective_offset = dev_cal.is_valid() ? dev_cal->get_camera_offset() : impl->camera.offset;
    double effective_tilt = dev_cal.is_valid() ? dev_cal->get_camera_tilt() : impl->camera.tilt;
    Vector2 win_pos = dev_cal.is_valid() ? dev_cal->get_window_position_lpix() : Vector2(0.0, 0.0);

    Vector3 calibrated_dir = p_direction_cam;
    if (p_apply_bio_calibration && impl->display.bio_data.is_valid) {
        Gaze::GodotCameraVector3 raw_dir(p_direction_cam.x, p_direction_cam.y, p_direction_cam.z);
        Gaze::GodotCameraVector3 calib_v = Gaze::apply_3d_bias_vector(
            raw_dir,
            Gaze::SpacedVector2<Gaze::Space::GodotCameraEuler>(impl->display.bio_data.bias_pitch, impl->display.bio_data.bias_yaw),
            Gaze::SpacedVector2<Gaze::Space::GodotCameraEuler>(impl->display.bio_data.scale_pitch, impl->display.bio_data.scale_yaw)
        );
        calibrated_dir = Vector3(calib_v.x, calib_v.y, calib_v.z);
    }

    Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm> pos_mm;
    Gaze::GodotCameraVector3 origin_godot(p_origin_cam.x, p_origin_cam.y, p_origin_cam.z);
    Gaze::GodotCameraVector3 dir_godot(calibrated_dir.x, calibrated_dir.y, calibrated_dir.z);
    if (Gaze::project_ray_to_screen_mm(
            origin_godot,
            dir_godot,
            Gaze::GodotCameraVector3(effective_offset.x, effective_offset.y, effective_offset.z),
            effective_tilt,
            Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(physical_sz.x, physical_sz.y),
            pos_mm
        )) {
        double scale_x = (double)logical_sz.x / physical_sz.x;
        double scale_y = (double)logical_sz.y / physical_sz.y;
        Vector2 px(pos_mm.x * scale_x, pos_mm.y * scale_y);
        return px - win_pos;
    }
    return Vector2(INFINITY, INFINITY);
}

Vector2 GazeServer::project_ray_to_screen_mm(const Vector3 &p_origin_cam, const Vector3 &p_direction_cam) const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    Ref<DeviceCalibration> dev_cal = impl->display.device_calibration;
    Vector2 physical_sz = dev_cal.is_valid() ? dev_cal->get_physical_size_mm() : Vector2(345.0, 215.0);
    Vector3 effective_offset = dev_cal.is_valid() ? dev_cal->get_camera_offset() : impl->camera.offset;
    double effective_tilt = dev_cal.is_valid() ? dev_cal->get_camera_tilt() : impl->camera.tilt;

    Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm> pos_mm;
    Gaze::GodotCameraVector3 origin_godot(p_origin_cam.x, p_origin_cam.y, p_origin_cam.z);
    Gaze::GodotCameraVector3 dir_godot(p_direction_cam.x, p_direction_cam.y, p_direction_cam.z);
    if (Gaze::project_ray_to_screen_mm(
            origin_godot,
            dir_godot,
            Gaze::GodotCameraVector3(effective_offset.x, effective_offset.y, effective_offset.z),
            effective_tilt,
            Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(physical_sz.x, physical_sz.y),
            pos_mm
        )) {
        return Vector2(pos_mm.x, pos_mm.y);
    }
    return Vector2(INFINITY, INFINITY);
}

// --- Event Factory ---

void GazeServer::set_event_factory(const Ref<GazeEventFactory>& p_factory) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    if (p_factory.is_valid()) {
        event_factory = p_factory;
    } else {
        Ref<GazeServerEventFactory> def_factory;
        def_factory.instantiate();
        event_factory = def_factory;
    }
}

void GazeServer::initialize_scene_resources() {
    _ensure_event_factory_loaded();
}

void GazeServer::_ensure_event_factory_loaded() {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    if (event_factory.is_valid() && Object::cast_to<GazeServerEventFactory>(event_factory.ptr()) == nullptr) {
        return;
    }
    ProjectSettings *ps = ProjectSettings::get_singleton();
    if (ps && ps->has_setting("gaze/events/event_factory_path")) {
        String factory_path = ps->get_setting("gaze/events/event_factory_path");
        if (!factory_path.is_empty()) {
            ResourceLoader *rl = ResourceLoader::get_singleton();
            if (rl) {
                Ref<Resource> res = rl->load(factory_path);
                if (res.is_valid()) {
                    GazeEventFactory *fact = Object::cast_to<GazeEventFactory>(res.ptr());
                    if (fact) {
                        event_factory = fact;
                        return;
                    }
                }
            }
        }
    }
    if (event_factory.is_null()) {
        Ref<GazeServerEventFactory> def_factory;
        def_factory.instantiate();
        event_factory = def_factory;
    }
}

Ref<GazeEventFactory> GazeServer::get_event_factory() {
    _ensure_event_factory_loaded();
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    return event_factory;
}

Ref<InputEventGaze> GazeServer::create_default_event() {
    _ensure_event_factory_loaded();
    Ref<InputEventGaze> event;
    event.instantiate();
    event->set_frame_id(++current_frame_id);
    event->set_timestamp_usec(Time::get_singleton()->get_ticks_usec());

    if (active_read_data) {
        event->set_left_eye_openness(active_read_data->left_eye_openness);
        event->set_right_eye_openness(active_read_data->right_eye_openness);
    } else {
        event->set_left_eye_openness(1.0f);
        event->set_right_eye_openness(1.0f);
    }

    Vector2 local_pos = get_gaze_screen_px(true);
    Vector2 win_pos = Vector2(0, 0);
    Ref<DeviceCalibration> dev_cal = impl->display.device_calibration;
    if (dev_cal.is_valid()) {
        win_pos = dev_cal->get_window_position_lpix();
    }
    Vector2 screen_pos = local_pos + win_pos;

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

    Transform3D head_xform = get_head_transform();
    if (!head_xform.basis.is_rotation()) {
        head_xform.basis = Basis();
    }
    event->set_head_transform(head_xform);

    Vector3 gaze_d = Vector3(0, 0, -1);
    Vector3 gaze_o = Vector3(0, 0, 0);
    if (active_read_data && active_read_data->gaze_success) {
        gaze_d = Vector3(active_read_data->gaze_direction.x, active_read_data->gaze_direction.y, active_read_data->gaze_direction.z);
        gaze_o = Vector3(active_read_data->gaze_origin.x, active_read_data->gaze_origin.y, active_read_data->gaze_origin.z);
    }
    Vector3 norm_gaze_dir = gaze_d.is_normalized() ? gaze_d : gaze_d.normalized();
    if (norm_gaze_dir.length_squared() < 1e-4) {
        norm_gaze_dir = Vector3(0, 0, -1);
    }
    Basis gaze_basis = Basis::looking_at(norm_gaze_dir, Vector3(0, 1, 0));
    if (!gaze_basis.is_rotation()) {
        gaze_basis = Basis();
    }
    event->set_gaze_transform(Transform3D(gaze_basis, gaze_o));

    return event;
}

Ref<InputEventGazeMissing> GazeServer::create_default_missing_event(int p_reason) {
    Ref<InputEventGazeMissing> event;
    event.instantiate();
    event->set_frame_id(++current_frame_id);
    event->set_timestamp_usec(Time::get_singleton()->get_ticks_usec());
    event->set_reason(static_cast<InputEventGazeMissing::MissingReason>(p_reason));
    if (active_read_data) {
        event->set_left_eye_openness(active_read_data->left_eye_openness);
        event->set_right_eye_openness(active_read_data->right_eye_openness);
    }
    return event;
}

// --- Emulation Settings ---

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

void GazeServer::set_mouse_emulation_dwell_sec(float p_sec) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    mouse_emulation.set_dwell_time_sec(p_sec);
}

float GazeServer::get_mouse_emulation_dwell_sec() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return mouse_emulation.get_dwell_time_sec();
}

void GazeServer::set_mouse_emulation_transition_sec(float p_sec) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    mouse_emulation.set_transition_duration_sec(p_sec);
}

float GazeServer::get_mouse_emulation_transition_sec() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(state_mutex));
    return mouse_emulation.get_transition_duration_sec();
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

            Vector3 head_t(completed_data->head_translation.x, completed_data->head_translation.y, completed_data->head_translation.z);
            Vector3 head_r(completed_data->head_rotation.x, completed_data->head_rotation.y, completed_data->head_rotation.z);
            gaze_frame->set_head_translation(head_t);
            gaze_frame->set_head_rotation(head_r);

            Vector3 gaze_o(completed_data->gaze_origin.x, completed_data->gaze_origin.y, completed_data->gaze_origin.z);
            Vector3 gaze_d(completed_data->gaze_direction.x, completed_data->gaze_direction.y, completed_data->gaze_direction.z);
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

            if (completed_data->face_detected) {
                const auto &gb = completed_data->head_transform.basis;
                Basis godot_basis(
                    Vector3(gb.x.x, gb.x.y, gb.x.z),
                    Vector3(gb.y.x, gb.y.y, gb.y.z),
                    Vector3(gb.z.x, gb.z.y, gb.z.z)
                );
                if (!godot_basis.is_orthogonal() || !godot_basis.is_rotation()) {
                    godot_basis.orthonormalize();
                }
                if (!godot_basis.is_rotation()) {
                    godot_basis = Basis();
                }
                Transform3D godot_xform(godot_basis, head_t);
                set_face_transform(godot_xform, head_r, true);
                set_face_landmarks_2d(lm_array);
                set_eye_openness(completed_data->left_eye_openness, completed_data->right_eye_openness);
                set_eye_crops(gaze_frame->get_left_eye_crop(), gaze_frame->get_right_eye_crop());

                if (completed_data->gaze_success) {
                    set_gaze(gaze_o, gaze_d);
                }
            } else {
                set_face_transform(Transform3D(), Vector3(), false);
                set_face_landmarks_2d(PackedVector2Array());
                set_eye_openness(0.0f, 0.0f);
                set_eye_crops(Ref<Image>(), Ref<Image>());
                impl->eye.gaze_origin_cam = Vector3();
                impl->eye.gaze_direction_cam = Vector3();
                impl->eye.latest_projected_gaze = Vector2();
                impl->eye.latest_filtered_gaze = Vector2();
                impl->eye.latest_projected_gaze_mm = Vector2();
                impl->eye.latest_filtered_gaze_mm = Vector2();
            }

            emit_signal("gaze_frame_began", gaze_frame);

            if (active_read_data) {
                pipeline->frame_pool.release(active_read_data);
            }
            active_read_data = completed_data;

            if (completed_data->face_detected) {
                face_detected_this_frame = true;
                last_camera_face_detected_usec = Time::get_singleton()->get_ticks_usec();
                Ref<InputEventGazeBase> event;
                if (event_factory.is_valid()) {
                    if (event_factory->get_script().get_type() != Variant::NIL) {
                        event = event_factory->call("create_gaze_event");
                    } else {
                        event = event_factory->create_gaze_event();
                    }
                }
                if (event.is_null()) {
                    event = create_default_event();
                }

                most_recent_event = event;
                mouse_emulation.notify_camera_event(event);
                if (input && event.is_valid()) {
                    input->parse_input_event(event);
                }

                if (emulate_mouse_from_gaze && input) {
                    Vector2 local_pos = get_gaze_screen_px(true);
                    Vector2 rel = local_pos - last_gaze_pos;
                    uint64_t now_usec = Time::get_singleton()->get_ticks_usec();
                    float dt = (last_event_time_usec > 0 && now_usec > last_event_time_usec) ? (float)(now_usec - last_event_time_usec) / 1000000.0f : 0.016667f;
                    if (dt < 0.0001f) dt = 0.0001f;
                    Vector2 vel = rel / dt;

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
                Ref<InputEventGazeBase> missing;
                if (event_factory.is_valid()) {
                    if (event_factory->get_script().get_type() != Variant::NIL) {
                        missing = event_factory->call("create_missing_event", InputEventGazeMissing::REASON_NO_FACE_DETECTED);
                    } else {
                        missing = event_factory->create_missing_event(InputEventGazeMissing::REASON_NO_FACE_DETECTED);
                    }
                }
                if (missing.is_null()) {
                    missing = create_default_missing_event(InputEventGazeMissing::REASON_NO_FACE_DETECTED);
                }
                most_recent_event = missing;
                if (input && missing.is_valid()) {
                    input->parse_input_event(missing);
                }
            }

            emit_signal("gaze_frame_ready", gaze_frame);
        }
    }

    // Grab current frames from VisionServer and push to GazeTrackingPipeline
    if (pipeline && !pipeline->is_busy() && impl->camera.vision_camera_rid.is_valid()) {
        Gaze::Frame current_frame;
        bool has_frame = VisionServer::get_singleton()->get_camera_current_frame(impl->camera.vision_camera_rid, current_frame);
        if (has_frame) {
            Gaze::GazeFrameData* write_data = pipeline->frame_pool.take();
            if (write_data) {
                size_t frame_bytes = current_frame.width * current_frame.height * 3;
                write_data->camera_raw_bgr.resize(frame_bytes);
                std::memcpy(write_data->camera_raw_bgr.data(), current_frame.data, frame_bytes);
                write_data->camera_width = current_frame.width;
                write_data->camera_height = current_frame.height;
                write_data->timestamp = current_frame.timestamp;
                write_data->camera_focal_length_px = VisionServer::get_singleton()->camera_get_focal_length(impl->camera.vision_camera_rid);
                write_data->camera_fov_degrees = VisionServer::get_singleton()->camera_get_fov(impl->camera.vision_camera_rid);

                write_data->auto_roll_enabled = impl->face.auto_roll_enabled;
                write_data->roll_hint_rad = impl->face.roll_hint_rad;

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
                emit_camera_frame_ready(impl->camera.vision_camera_rid);
            }
        }
    }
#endif

    // Mouse-to-Gaze Emulation & IN_OUT Transition Strategy
    DisplayServer* ds = nullptr;
    if (Engine::get_singleton()->get_singleton_list().has("DisplayServer")) {
        ds = DisplayServer::get_singleton();
    }

    uint64_t now_usec = Time::get_singleton()->get_ticks_usec();
    float dt = (last_event_time_usec > 0 && now_usec > last_event_time_usec) ? (float)(now_usec - last_event_time_usec) / 1000000.0f : 0.016667f;
    if (dt < 0.0001f) dt = 0.0001f;

    bool cam_tracking_active = (active_trackers > 0);
    bool face_is_detected = is_face_detected();

    mouse_emulation.update(dt, cam_tracking_active, face_is_detected, emulate_gaze_from_mouse, ds);

    if (mouse_emulation.is_emulation_active() && input) {
        Ref<DeviceCalibration> dev_cal = get_device_calibration();
        Ref<InputEventGazeBase> syn_event = mouse_emulation.synthesize_event(
            ds, dev_cal, event_factory,
            current_frame_id, last_event_time_usec, last_gaze_pos, last_screen_pos
        );
        if (syn_event.is_valid()) {
            most_recent_event = syn_event;
            input->parse_input_event(syn_event);
        }
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
