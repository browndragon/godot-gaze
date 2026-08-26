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
#include "gaze_calibration_resource.hpp"
#include "smoother.hpp"
#include "one_euro_smoother.hpp"
#include "gaze_pipeline_config.hpp"
#include "gaze_frame.hpp"
#include "display_profile.hpp"
#include "input_event_gaze.hpp"
#include "../core/gaze_frame_data.hpp"

#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#ifndef WEB_ENABLED
#include "../native/gaze_tracking_pipeline.hpp"
#endif

namespace godot {

class GazeServer;

/**
 * @class GazeServer
 * @brief Central engine singleton managing camera acquisition, ML pipeline, calibrations,
 *        lifecycle, and dispatching InputEventGaze events into Godot's input tree.
 */
struct GazeServerImpl;

class GazeServer : public Object {
    GDCLASS(GazeServer, Object);

private:
    static GazeServer *singleton;
    std::unique_ptr<GazeServerImpl> impl;
#ifndef WEB_ENABLED
    std::unique_ptr<Gaze::GazeTrackingPipeline> pipeline;
#endif

    RID default_display_rid;
    RID default_camera_rid;
    RID default_face_rid;
    RID default_eye_rid;

    Ref<DisplayProfile> default_display_profile;
    Ref<DeviceCalibration> default_device_calibration;
    Ref<BioCalibration> default_bio_calibration;

    Ref<InputEventGazeBase> most_recent_event;
    uint64_t current_frame_id = 0;
    Vector2 last_gaze_pos;
    Vector2 last_screen_pos;
    uint64_t last_event_time_usec = 0;

    bool emulate_gaze_from_mouse = true;
    bool emulate_mouse_from_gaze = false;
    bool was_both_closed = false;

    Ref<ImageTexture> camera_debug_texture;

protected:
    static void _bind_methods();

    Gaze::PipelineConfig active_config;
    mutable std::recursive_mutex state_mutex;
    int active_trackers = 0;

public:
    Gaze::GazeFrameData* active_read_data = nullptr;

    GazeServer();
    virtual ~GazeServer();

    /**
     * @brief Get the GazeServer global singleton instance.
     */
    static GazeServer *get_singleton() { return singleton; }

    // --- High-Level Lifecycle Management ---

    /**
     * @brief Request tracking start (increments refcount).
     * @return true if this call was the 0 -> 1 transition (initiating pipeline startup), false otherwise.
     */
    bool start_tracking();

    /**
     * @brief Request tracking stop (decrements refcount).
     * @param p_immediate If true, stops camera/pipeline immediately. If false, schedules deferred check.
     */
    void stop_tracking(bool p_immediate = false);

    /**
     * @brief Deferred check to halt tracking if refcount remains 0 at end of frame.
     */
    void _deferred_stop_check();

    /**
     * @brief Check whether gaze tracking or pipeline processing is actively running.
     */
    bool is_tracking_active() const;

    /**
     * @brief Retrieve the most recent gaze input event (InputEventGaze or InputEventGazeMissing).
     */
    Ref<InputEventGazeBase> get_most_recent_event() const;

    // --- Calibrations & Display Geometry ---

    void set_display_profile(const Ref<DisplayProfile>& p_profile);
    Ref<DisplayProfile> get_display_profile() const;

    void set_device_calibration(const Ref<DeviceCalibration>& p_calibration);
    Ref<DeviceCalibration> get_device_calibration() const;

    void set_bio_calibration(const Ref<BioCalibration>& p_calibration);
    Ref<BioCalibration> get_bio_calibration() const;

    // --- Emulation Settings ---

    void set_emulate_gaze_from_mouse(bool p_enable);
    bool get_emulate_gaze_from_mouse() const;

    void set_emulate_mouse_from_gaze(bool p_enable);
    bool get_emulate_mouse_from_gaze() const;

    // --- Debug Texture & Landmarks Access ---

    Ref<Texture2D> get_camera_texture();
    PackedVector2Array get_debug_landmarks() const;

    // --- Server Low-Level Resource Management (RIDs) ---

    RID display_create();
    void display_set_geometry(RID p_display, Vector2 p_logical_size, Vector2 p_physical_size);
    void display_set_device_calibration(RID p_display, const Ref<DeviceCalibration>& p_calibration);
    void display_set_bio_calibration(RID p_display, const Ref<BioCalibration>& p_calibration);
    void display_set_window_parameters(RID p_display, Vector2 p_window_pos, Transform2D p_viewport_transform);
    void display_free(RID p_display);

    RID camera_create(RID p_display);
    void camera_set_offsets(RID p_camera, Vector3 p_offset, double p_tilt);
    void camera_set_vision_rid(RID p_camera, RID p_vision_camera);
    void camera_free(RID p_camera);

    RID face_tracker_create(RID p_camera);
    PackedVector3Array get_face_model_points() const;
    void face_tracker_set_pose(RID p_face, Vector3 p_translation, Vector3 p_rotation, bool p_detected);
    void face_tracker_free(RID p_face);

    Vector3 get_head_rotation_from_face_tracker(RID p_face) const;
    Vector3 get_head_translation_from_face_tracker(RID p_face) const;
    Vector3 get_head_pose_origin_mm(RID p_face) const;
    Vector3 get_head_pose_euler_deg(RID p_face) const;
    void face_tracker_set_landmarks_2d(RID p_face, const PackedVector2Array &p_landmarks);
    PackedVector2Array get_face_landmarks_2d(RID p_face) const;

    RID eye_tracker_create(RID p_face);
    void eye_tracker_set_gaze(RID p_eye, Vector3 p_origin_cam, Vector3 p_direction_cam);
    void eye_tracker_set_openness(RID p_eye, float p_left, float p_right);
    float get_left_eye_openness(RID p_eye) const;
    float get_right_eye_openness(RID p_eye) const;
    void eye_tracker_set_smoother(RID p_eye, const Ref<Smoother>& p_smoother);
    void eye_tracker_set_crop_requested(RID p_eye, bool p_requested);
    bool eye_tracker_is_crop_requested(RID p_eye);
    Array tracker_get_eye_crops(RID p_eye);
    void eye_tracker_free(RID p_eye);

    Vector3 get_gaze_origin_from_eye_tracker(RID p_eye) const;
    Vector3 get_gaze_direction_from_eye_tracker(RID p_eye) const;
    Vector2 get_projected_gaze_from_eye_tracker(RID p_eye, bool p_smoothed = false) const;
    Vector2 get_projected_gaze_mm_from_eye_tracker(RID p_eye, bool p_smoothed = false) const;
    void set_crops_on_eye_tracker(RID p_eye, const Ref<Image>& p_left_crop, const Ref<Image>& p_right_crop);
    void reset_eye_tracker(RID p_eye);

    void emit_camera_frame_ready(RID p_vision_camera);
    Transform3D get_relative_transform(RID p_entity);
    Vector2 get_gaze_screen(RID p_display, bool p_smoothed = true);
    bool is_face_detected(RID p_face);

    // --- Pipeline Processing ---

    void set_pipeline_config(const Ref<GazePipelineConfig>& p_config);
    void trigger_process();
    void start_processing();
    void stop_processing();

    void ref_tracker();
    void unref_tracker();
    int get_active_tracker_count() const;

#ifdef WEB_ENABLED
    void feed_gaze_web_raw(const Array& args);
#endif

    void set_verbosity(int level);
    int get_verbosity() const;

    static String get_build_info();
    static String get_build_timestamp();
};

} // namespace godot
