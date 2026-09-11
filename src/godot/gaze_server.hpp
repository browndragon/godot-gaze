#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/templates/safe_refcount.hpp>
#include <godot_cpp/classes/time.hpp>
#include "../core/projection_engine.hpp"
#include "../core/face_model_geometry.hpp"
#include "gaze_device_profile.hpp"
#include "smoother.hpp"
#include "one_euro_smoother.hpp"
#include "mouse_gaze_emulation.hpp"
#include "gaze_pipeline_config.hpp"
#include "gaze_frame.hpp"
#include "input_event_gaze.hpp"
#include "gaze_event_factory.hpp"
#include "../core/gaze_frame_data.hpp"

#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace Gaze {
class GazeTrackingPipeline;
}

namespace godot {

class GazeServer : public Object {
    GDCLASS(GazeServer, Object);

private:
    static GazeServer *singleton;
    std::unique_ptr<struct GazeServerImpl> impl;
    mutable std::recursive_mutex state_mutex;

    Gaze::GazeFrameData* active_read_data = nullptr;
    std::shared_ptr<Gaze::GazeTrackingPipeline> pipeline;
    Gaze::PipelineConfig active_config;

    MouseGazeEmulation mouse_emulation;
    uint64_t current_frame_id = 0;
    uint64_t last_event_time_usec = 0;
    Vector2 last_gaze_pos = Vector2(0, 0);
    Vector2 last_screen_pos = Vector2(0, 0);

    Ref<InputEventGazeBase> most_recent_event;
    Ref<GazeEventFactory> event_factory;

    int active_trackers = 0;
    bool emulate_gaze_from_mouse = true;
    bool emulate_mouse_from_gaze = false;
    uint64_t last_camera_face_detected_usec = 0;
    bool was_both_closed = false;

protected:
    static void _bind_methods();

public:
    static GazeServer *get_singleton();

    GazeServer();
    virtual ~GazeServer();

    // --- High-Level Lifecycle & Processing ---
    bool start_tracking();
    void stop_tracking(bool p_immediate = false);
    void _deferred_stop_check();
    bool is_tracking_active() const;
    Ref<InputEventGazeBase> get_most_recent_event() const;

    void ensure_process_connected();
    void trigger_process();
    void start_processing();
    void stop_processing();
    void reset();
    int get_active_tracker_count() const;

    // --- Hardware Profile & Configuration ---
    void set_device_profile(const Ref<GazeDeviceProfile>& p_profile);
    Ref<GazeDeviceProfile> get_device_profile() const;

    void set_camera_offsets(Vector3 p_offset, double p_tilt);
    Vector3 get_camera_offset() const;
    double get_camera_tilt() const;

    void set_camera_vision_rid(RID p_vision_camera);
    RID get_camera_vision_rid() const;

    void set_pipeline_config(const Ref<GazePipelineConfig>& p_config);
    Ref<GazePipelineConfig> get_pipeline_config() const;

    // --- Face & Head Pose Tracking ---
    bool is_face_detected() const;
    Transform3D get_head_transform() const;
    Vector3 get_head_position() const;
    Vector3 get_head_rotation() const;
    Vector3 get_head_pose_origin_mm() const;
    Vector3 get_head_pose_euler_deg() const;

    void set_face_pose(Vector3 p_translation, Vector3 p_rotation, bool p_detected);
    void set_face_transform(const Transform3D &p_transform, const Vector3 &p_rotation, bool p_detected);

    PackedVector2Array get_face_landmarks_2d() const;
    PackedVector2Array get_face_landmarks() const;
    PackedVector2Array get_debug_landmarks() const;
    void set_face_landmarks_2d(const PackedVector2Array &p_landmarks);

    PackedVector3Array get_face_model_points() const;

    void set_roll_hint(float p_roll_hint_rad);
    float get_roll_hint() const;
    void set_auto_roll_enabled(bool p_enabled);
    bool is_auto_roll_enabled() const;

    // --- Eye & Gaze Tracking ---
    bool is_gaze_detected() const;
    Vector3 get_gaze_origin() const;
    Vector3 get_gaze_direction() const;
    void set_gaze(Vector3 p_origin_cam, Vector3 p_direction_cam);

    Vector2 get_gaze_screen_px(bool p_smoothed = true) const;
    Vector2 get_gaze_screen_mm(bool p_smoothed = true) const;
    Vector2 get_gaze_screen(bool p_smoothed = true) const;
    Vector2 get_projected_gaze(bool p_smoothed = false) const;
    Vector2 get_projected_gaze_mm(bool p_smoothed = false) const;

    float get_left_eye_openness() const;
    float get_right_eye_openness() const;
    void set_eye_openness(float p_left, float p_right);

    void set_smoother(const Ref<Smoother>& p_smoother);
    Ref<Smoother> get_smoother() const;

    void set_crop_requested(bool p_requested);
    bool is_crop_requested() const;
    Array get_eye_crops();
    void set_eye_crops(const Ref<Image>& p_left_crop, const Ref<Image>& p_right_crop);

    Ref<Texture2D> get_camera_texture();
    void set_camera_preview_requested(bool p_requested);
    bool is_camera_preview_requested() const;
    void emit_camera_frame_ready(RID p_vision_camera);

    // --- Telemetry & Diagnostics ---
    Dictionary get_pipeline_stage_timings() const;

    // --- Ray Projection Math ---
    Vector3 project_ray_to_camera_plane(const Vector3 &p_origin_cam, const Vector3 &p_direction_cam) const;
    Vector2 project_ray_to_viewport(const Vector3 &p_origin_cam, const Vector3 &p_direction_cam) const;

    // --- Event Factory ---
    void set_event_factory(const Ref<GazeEventFactory>& p_factory);
    Ref<GazeEventFactory> get_event_factory();
    void initialize_scene_resources();
    void _ensure_event_factory_loaded();
    Ref<InputEventGaze> create_default_event();
    Ref<InputEventGazeMissing> create_default_missing_event(int p_reason);

    // --- Emulation Settings ---
    void set_emulate_gaze_from_mouse(bool p_enable);
    bool get_emulate_gaze_from_mouse() const;
    void set_emulate_mouse_from_gaze(bool p_enable);
    bool get_emulate_mouse_from_gaze() const;
    void set_mouse_emulation_dwell_sec(float p_sec);
    float get_mouse_emulation_dwell_sec() const;
    void set_mouse_emulation_transition_sec(float p_sec);
    float get_mouse_emulation_transition_sec() const;

#ifdef WEB_ENABLED
    void feed_gaze_web_raw(const Array& args);
#endif

    void set_verbosity(int level);
    int get_verbosity() const;

    static String get_build_info();
    static String get_build_timestamp();
};

} // namespace godot
