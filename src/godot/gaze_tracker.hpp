/**
 * @file gaze_tracker.hpp
 * @brief GazeTracker Godot Node GDExtension binding
 *
 * The primary entry point for the godot-gaze plugin. Exposes properties, methods,
 * and signals to GDScript. Coordinates platform-specific execution targets:
 * - Native: captures hardware camera feed, executes YuNet face landmarking, and ONNX gaze inference.
 * - Web/WASM: interface stub model linked to the browser's JavaScript sidecar via Emscripten.
 * Integrates 1 Euro filters for coordinate smoothing and runs calibration procedures.
 */
#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/classes/ref.hpp>
#include "gaze_calibration_resource.hpp"
#include "gaze_pipeline_config.hpp"

// Core interfaces (included in both Web and Native)
#include "camera_interface.hpp"
#include "face_pipeline.hpp"
#include "gaze_model.hpp"
#include "projection_engine.hpp"

#include <godot_cpp/variant/array.hpp>
#include <vector>

namespace godot {

class CameraSensor;
class FaceEstimator;
class EyeEstimator;
class Smoother;
class DisplayProfile;
class OneEuroFilter;

struct PlatformGeometry {
    // Window position relative to the primary screen top-left corner, in LOGICAL screen pixels
    Vector2 window_position_px = Vector2(0.0, 0.0);
};

/**
 * @class GazeTracker
 * @brief Godot Node coordinating camera tracking, model inference, and gaze coordinate projection.
 *
 * GazeTracker handles camera acquisition (via CameraSensor), pipelines inputs through YuNet face detection
 * and GazeNet estimation (via Face/Eye Estimators), smoothes projected points, and maps coordinates
 * from camera space (millimeters) to viewport space (logical screen pixels).
 */
class GazeTracker : public Node3D {
    GDCLASS(GazeTracker, Node3D);

public:
    static constexpr double MM_PER_INCH = 25.4;
    static constexpr double CSS_PIXELS_PER_INCH = 96.0;
    static constexpr Gaze::GazeVector2 DEFAULT_SCREEN_SIZE_MM = Gaze::GazeVector2(345.0, 215.0);
    static constexpr Gaze::GazeVector2i DEFAULT_SCREEN_SIZE_PIXELS = Gaze::GazeVector2i(1920, 1080);

    enum GazeLifecycle {
        LIFECYCLE_UNKNOWN = 0,
        LIFECYCLE_PERM_REQ = 1,
        LIFECYCLE_INITIALIZING = 2,
        LIFECYCLE_RUNNING = 3,
        LIFECYCLE_ERROR = 4
    };

private:
    CameraSensor* camera_sensor = nullptr;
    FaceEstimator* face_estimator = nullptr;
    EyeEstimator* eye_estimator = nullptr;

    Ref<Smoother> screen_smooth;
    Array smoother_state;

    // Mathematical projection engine
    Gaze::ProjectionEngine projection_engine;

    // Configurable Properties
    Ref<GazeCalibration> calibration_resource;
    Ref<GazePipelineConfig> pipeline_config;
    Ref<DisplayProfile> display_profile;
    int camera_device_id = 0;
    int debug_logging_frames = 0;
    int debug_log_frame_counter = 0;
    bool log_this_frame = false;
    Vector2 window_position_override = Vector2(-1.0, -1.0);

    // Runtime state (latest estimations)
    Vector2 latest_projected_gaze_px;
    Vector2 latest_filtered_gaze_px;
    bool tracker_initialized = false;
    GazeLifecycle lifecycle_state = LIFECYCLE_UNKNOWN;
    bool autostart = true;
    void set_lifecycle_state(GazeLifecycle p_state);
    bool is_face_tracked = false;
    uint64_t last_frame_time = 0;
    Gaze::GazeVector3 latest_gaze_origin = Gaze::GazeVector3(0.0, 0.0, 500.0);
    Gaze::GazeVector3 latest_gaze_dir = Gaze::GazeVector3(0.0, 0.0, -1.0);
    Gaze::EyeCrops latest_crops;

    Vector2 web_canvas_pos = Vector2(0.0, 0.0);
    void* opaque = nullptr;

    void update_projection_parameters();
    void update_filter_parameters();
    void update_pipeline_config();
    String copy_model_to_user_dir(const String &res_path);
    void copy_individual_file(const String &src, const String &dest);
    std::vector<uint8_t> load_file_buffer(const String &path);
    Transform2D get_adjusted_viewport_transform() const;

    // Platform-specific helper methods returning coordinates in physical spaces
    void platform_initialize();
    void platform_terminate();
    void platform_process(double delta);
    void platform_on_permission_result(bool granted);
    void platform_trigger_permission_request();
    


protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    GazeTracker();
    virtual ~GazeTracker();


    virtual void _ready() override;
    virtual void _process(double delta) override;

    // Tracker control
    /**
     * @brief Initializes the gaze tracker, setting up camera and models.
     * @return True if initialized successfully, false otherwise.
     */
    bool initialize_tracker();
    /**
     * @brief Stops camera acquisition and model inference pipelines.
     * @param p_emit_signal Whether to emit lifecycle status change signals.
     */
    void stop_tracker(bool p_emit_signal = true);
    bool complete_initialization();
    void trigger_permission_request();
    void on_permission_result(bool granted);


    // Calibration routines
    /**
     * @brief Clears active calibration, reverting to uncalibrated geometric projection.
     */
    void clear_calibration();
    /**
     * @brief Maps logical viewport coordinates to primary screen logical coordinates.
     * @param logical_pixel Viewport-relative coordinates.
     * @return Screen-relative logical coordinates.
     */
    Vector2 map_viewport_to_screen(Vector2 logical_pixel) const;
    /**
     * @brief Filters raw gaze coordinates using configured smoothing algorithms.
     * @param raw Raw projected coordinate.
     * @return Filtered smoothed coordinate.
     */
    Vector2 filter_gaze_coordinate(Vector2 raw);

    // Unified gaze feed API (Web/WASM sidecar & custom injectors)
    /**
     * @brief Manually injects raw gaze origin and direction coordinates.
     * @param face_detected True if face landmarking succeeded.
     * @param origin Gaze origin 3D camera-space vector (mm).
     * @param direction Gaze direction unit vector in camera-space.
     */
    void feed_gaze(bool face_detected, Vector3 origin, Vector3 direction);
    void feed_gaze_web_raw(const Array& args);
    void on_sidecar_ready(const Array& args);

    CameraSensor* get_camera_sensor() const;
    FaceEstimator* get_face_estimator() const;
    EyeEstimator* get_eye_estimator() const;

    void set_screen_smooth(const Ref<Smoother>& smoother);
    Ref<Smoother> get_screen_smooth() const;

    // Getters / Setters for properties
    int get_lifecycle_state() const;
    void set_autostart(bool p_autostart);
    bool get_autostart() const;

    void set_pipeline_config(const Ref<GazePipelineConfig>& res);
    Ref<GazePipelineConfig> get_pipeline_config() const;

    void set_calibration_resource(const Ref<GazeCalibration>& res);
    Ref<GazeCalibration> get_calibration_resource() const;

    void set_display_profile(const Ref<DisplayProfile>& profile);
    Ref<DisplayProfile> get_display_profile() const;

    PlatformGeometry platform_get_geometry() const;

    void set_debug_logging_frames(int frames);
    int get_debug_logging_frames() const;

    void set_camera_device_id(int id);
    int get_camera_device_id() const;

    void set_window_position_override(Vector2 pos);
    Vector2 get_window_position_override() const;

    Vector2 get_latest_projected_gaze() const { return latest_projected_gaze_px; }
    Vector2 get_latest_filtered_gaze() const { return latest_filtered_gaze_px; }
    bool is_face_detected() const { return is_face_tracked; }

    Transform3D get_head_transform() const;
    Transform3D get_camera_to_screen_transform() const;
    Vector3 get_derived_camera_offset() const;
    double get_derived_camera_tilt() const;
    Vector2 get_pixel_size_mm() const;
    Vector3 get_gaze_origin() const;
    Vector3 get_gaze_direction(bool apply_calibration = true) const;

    /**
     * @brief Gets the current head rotation in OpenCV Camera Space (Euler angles in degrees).
     */
    Vector3 get_head_rotation_opencv_space() const;

    /**
     * @brief Gets the current head translation in OpenCV Camera Space (in millimeters).
     */
    Vector3 get_head_translation_opencv_space() const;
    Vector3 get_head_position() const;
    Vector3 get_head_forward() const;

    /**
     * Projects a 3D ray in Camera Space (in millimeters) onto the physical screen plane,
     * and maps it to viewport/window-local pixel coordinates by subtracting the window position.
     *
     * @param origin The 3D start point of the ray in Camera Space.
     * @param direction The 3D direction vector of the ray in Camera Space.
     * @return The 2D viewport-local pixel coordinates of the intersection point,
     *         or Vector2(INFINITY, INFINITY) (resolving to Vector2.INF in Godot) if the ray
     *         is parallel to or points away from the screen.
     */
    Vector2 project_gaze_ray_to_viewport(Vector3 origin, Vector3 direction, bool apply_calibration = true) const;

    /**
     * @brief Gets the left eye center position in OpenCV Camera Space (in millimeters).
     */
    Vector3 get_left_eye_center_opencv_space() const;

    /**
     * @brief Gets the right eye center position in OpenCV Camera Space (in millimeters).
     */
    Vector3 get_right_eye_center_opencv_space() const;

    /**
     * @brief Gets the gaze direction unit vector in OpenCV Camera Space.
     */
    Vector3 get_gaze_direction_opencv_space() const;

    const Gaze::ProjectionEngine& get_projection_engine() const { return projection_engine; }
};

} // namespace godot

VARIANT_ENUM_CAST(godot::GazeTracker::GazeLifecycle);

