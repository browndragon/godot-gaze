#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/templates/safe_refcount.hpp>
#include <godot_cpp/classes/time.hpp>
#include "projection_engine.hpp"
#include "gaze_calibration_resource.hpp"
#include "smoother.hpp"
#include "gaze_pipeline_config.hpp"
#include "gaze_frame.hpp"
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
 * @brief Singleton coordinating display profiles, camera models, and thread-safe pipeline execution.
 *
 * GazeServer is the core architectural component of godot-gaze. It manages spatial entities
 * (Displays, Cameras, Faces, Eye Trackers) using RIDs, executes processor pipelines on a background
 * thread, applies calibration, projects gaze rays onto displays, and defer-emits signals to Godot clients.
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

    // --- Server Resource Lifecycle ---

    /**
     * @brief Create a display resource representation and return its unique RID.
     */
    RID display_create();

    /**
     * @brief Set display size geometry parameter tokens.
     * @param p_display The display RID.
     * @param p_logical_size Size of the screen in logical pixels.
     * @param p_physical_size Physical width and height of the display in millimeters.
     */
    void display_set_geometry(RID p_display, Vector2 p_logical_size, Vector2 p_physical_size);

    /**
     * @brief Set the active device calibration resource.
     */
    void display_set_device_calibration(RID p_display, const Ref<DeviceCalibration>& p_calibration);

    /**
     * @brief Set the active biological calibration resource.
     */
    void display_set_bio_calibration(RID p_display, const Ref<BioCalibration>& p_calibration);

    /**
     * @brief Set window location and viewport transformation tokens.
     * @param p_display The display RID.
     * @param p_window_pos Position of the window relative to primary screen top-left.
     * @param p_viewport_transform Viewport scale and shift parameters.
     */
    void display_set_window_parameters(RID p_display, Vector2 p_window_pos, Transform2D p_viewport_transform);

    /**
     * @brief Free the display resource.
     */
    void display_free(RID p_display);

    /**
     * @brief Create a spatial camera model resource relative to a display.
     */
    RID camera_create(RID p_display);

    /**
     * @brief Set camera offset translation and tilt angle relative to display center-top.
     * @param p_camera The camera RID.
     * @param p_offset Offset position in millimeters.
     * @param p_tilt Tilt angle in degrees (positive tilts down).
     */
    void camera_set_offsets(RID p_camera, Vector3 p_offset, double p_tilt);

    /**
     * @brief Associate the camera model with a raw VisionServer camera capture stream.
     */
    void camera_set_vision_rid(RID p_camera, RID p_vision_camera);

    /**
     * @brief Free the camera model resource.
     */
    void camera_free(RID p_camera);

    /**
     * @brief Create a face tracker resource relative to a camera.
     */
    RID face_tracker_create(RID p_camera);

    /**
     * @brief Set the current estimated head pose and detection status.
     * @param p_face The face RID.
     * @param p_translation Head translation vector in Camera Space (mm).
     * @param p_rotation Head rodrigues rotation vector in Camera Space.
     * @param p_detected True if face landmarking succeeded.
     */
    void face_tracker_set_pose(RID p_face, Vector3 p_translation, Vector3 p_rotation, bool p_detected);

    /**
     * @brief Free the face tracker resource.
     */
    void face_tracker_free(RID p_face);

    /**
     * @brief Retrieve head pose parameters in standard camera space.
     */
    Vector3 get_head_rotation_from_face_tracker(RID p_face) const;
    Vector3 get_head_translation_from_face_tracker(RID p_face) const;

    /**
     * @brief Create an eye tracker resource relative to a face.
     */
    RID eye_tracker_create(RID p_face);

    /**
     * @brief Feed raw estimated gaze parameters in Camera Space and calculate screen intersections.
     * @param p_eye The eye RID.
     * @param p_origin_cam Gaze origin 3D vector in Camera Space (mm).
     * @param p_direction_cam Gaze direction 3D unit vector in Camera Space.
     */
    void eye_tracker_set_gaze(RID p_eye, Vector3 p_origin_cam, Vector3 p_direction_cam);

    /**
     * @brief Set a coordinate smoothing filter resource.
     */
    void eye_tracker_set_smoother(RID p_eye, const Ref<Smoother>& p_smoother);

    void eye_tracker_set_crop_requested(RID p_eye, bool p_requested);
    bool eye_tracker_is_crop_requested(RID p_eye);

    /**
     * @brief Retrieve eye crop images (left, right) for native UI display previews.
     */
    Array tracker_get_eye_crops(RID p_eye);

    /**
     * @brief Free the eye tracker resource.
     */
    void eye_tracker_free(RID p_eye);

    /**
     * @brief Retrieve gaze origin and direction from eye tracker.
     */
    Vector3 get_gaze_origin_from_eye_tracker(RID p_eye) const;
    Vector3 get_gaze_direction_from_eye_tracker(RID p_eye) const;

    /**
     * @brief Set eye crop preview images.
     */
    void set_crops_on_eye_tracker(RID p_eye, const Ref<Image>& p_left_crop, const Ref<Image>& p_right_crop);

    /**
     * @brief Reset eye tracker coordinates and signal.
     */
    void reset_eye_tracker(RID p_eye);

    /**
     * @brief Emit gaze_data_ready for the VisionServer Camera RID.
     */
    void emit_camera_frame_ready(RID p_vision_camera);

    // --- Core transform and gaze retrieval APIs ---

    /**
     * @brief Retrieve the relative spatial Transform3D for display, camera, face, or eye.
     */
    Transform3D get_relative_transform(RID p_entity);

    /**
     * @brief Get the latest calculated gaze coordinate on the display in logical pixels.
     * @param p_display The display RID.
     * @param p_smoothed True to apply 1 Euro smoothing, false to return raw projection.
     */
    Vector2 get_gaze_screen(RID p_display, bool p_smoothed = true);

    /**
     * @brief Query whether a face is actively detected on the tracker.
     */
    bool is_face_detected(RID p_face);

    // --- Pipeline Configuration & Processing ---

    /**
     * @brief Update the server's pipeline configuration.
     */
    void set_pipeline_config(const Ref<GazePipelineConfig>& p_config);

    /**
     * @brief Trigger processing of any pending frames on the main thread (poll results).
     */
    void trigger_process();

    /**
     * @brief Start the background execution pipeline.
     */
    void start_processing();

    /**
     * @brief Stop the background execution pipeline.
     */
    void stop_processing();

#ifdef WEB_ENABLED
    void feed_gaze_web_raw(const Array& args);
#endif

    void set_verbosity(int level);
    int get_verbosity() const;
};

} // namespace godot
