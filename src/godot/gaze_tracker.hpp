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

#include <godot_cpp/classes/node.hpp>
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
#include "one_euro_filter.hpp"

#include <godot_cpp/variant/array.hpp>
#include <vector>

namespace godot {

struct PlatformGeometry {
    // Logical dimensions of the primary screen (in points/CSS pixels)
    Vector2i screen_size_lpix = Vector2i(-1, -1);

    // Physical dimensions of the primary screen (in hardware panel pixels)
    // MUST always represent the raw hardware pixels to align with 3D projection boundaries.
    Vector2i screen_size_ppix = Vector2i(-1, -1);

    // Physical size of the primary screen in millimeters
    Vector2 screen_size_mm = Vector2(-1.0, -1.0);

    // Window position relative to the primary screen top-left corner, in PHYSICAL screen pixels
    Vector2 window_position_ppix = Vector2(0.0, 0.0);

    // Active scale factor of the screen (e.g. 2.0 for Retina/high-DPI)
    double logical_to_physical_pixel_ratio = -1.0;

    // Mapping ratio to convert physical screen coordinates down to Godot window backing store coordinates.
    // Derived as: (Godot Backing Store Scale) / (OS Screen Scale)
    double window_to_screen_scale_ratio = 1.0;

    void merge_overrides(const PlatformGeometry& overrides) {
        if (overrides.screen_size_lpix.x > 0 && overrides.screen_size_lpix.y > 0) {
            screen_size_lpix = overrides.screen_size_lpix;
        }
        if (overrides.screen_size_mm.x > 0.0 && overrides.screen_size_mm.y > 0.0) {
            screen_size_mm = overrides.screen_size_mm;
        }
        if (overrides.logical_to_physical_pixel_ratio > 0.0) {
            logical_to_physical_pixel_ratio = overrides.logical_to_physical_pixel_ratio;
        }
    }
};

class GazeTracker : public Node {
    GDCLASS(GazeTracker, Node);

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
    // Pointers to the active layered pipeline implementations
    Gaze::CameraInterface* camera = nullptr;
    Gaze::FacePipeline* pipeline = nullptr;
    Gaze::GazeModel* model = nullptr;

    // Mathematical projection engine
    Gaze::ProjectionEngine projection_engine;

    // 1 Euro Filters for coordinates (horizontal and vertical)
    OneEuroFilter* filter_x = nullptr;
    OneEuroFilter* filter_y = nullptr;

    // Configurable Properties
    Ref<GazeCalibration> calibration_resource;
    Ref<GazePipelineConfig> pipeline_config;
    Vector3 camera_offset = Vector3(0.0, 148.0, 0.0); // mm relative to screen center (flush with bezel)
    double camera_tilt = 0.0;                           // degrees
    PlatformGeometry overrides;
    double camera_focal_length_px = -1.0;
    int camera_device_id = 0;

    double filter_min_cutoff = 1.0;
    double filter_beta = 0.01;
    double filter_d_cutoff = 1.0;

    String yunet_model_path;
    String gaze_onnx_path;
    bool expression_tracking_enabled = false;
    int debug_logging_frames = 0;
    int debug_log_frame_counter = 0;
    bool log_this_frame = false;

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
    bool initialize_tracker();
    void stop_tracker(bool p_emit_signal = true);
    bool complete_initialization();
    void trigger_permission_request();
    void on_permission_result(bool granted);


    // Calibration routines
    void clear_calibration();
    Vector2 map_logical_to_physical_screen(Vector2 logical_pixel) const;
    Vector2 filter_gaze_coordinate(Vector2 raw);

    // Unified gaze feed API (Web/WASM sidecar & custom injectors)
    void feed_gaze(bool face_detected, Vector3 origin, Vector3 direction);
    void feed_gaze_web_raw(const Array& args);
    void on_sidecar_ready(const Array& args);

    // Getters / Setters for properties
    int get_lifecycle_state() const;
    void set_autostart(bool p_autostart);
    bool get_autostart() const;

    void set_pipeline_config(const Ref<GazePipelineConfig>& res);
    Ref<GazePipelineConfig> get_pipeline_config() const;

    void set_calibration_resource(const Ref<GazeCalibration>& res);
    Ref<GazeCalibration> get_calibration_resource() const;

    void set_camera_offset(Vector3 offset);
    Vector3 get_camera_offset() const;

    void set_camera_tilt(double tilt);
    double get_camera_tilt() const;

    void set_debug_logging_frames(int frames);
    int get_debug_logging_frames() const;

    Vector2 get_derived_pixel_size_mm() const;
    Vector3 get_derived_camera_offset() const;
    double get_derived_camera_tilt() const;
    Vector2 get_pixel_size_mm() const;

    /**
     * @brief Gets the resolved display, screen, and window layout coordinates.
     */
    PlatformGeometry platform_get_geometry() const;

    /**
     * @brief Sets the screen size in logical pixels (CSS pixels on Web).
     *
     * Used to map coordinates across high-DPI displays consistently.
     * By default, this is automatically queried from the platform windowing/display APIs.
     * In most cases, the default is fine and this property does not need to be set manually.
     *
     * @param size The screen resolution in logical pixels.
     */
    void set_screen_size_lpix(Vector2i size);

    /**
     * @brief Gets the screen size in logical pixels.
     * @return The screen size in logical pixels.
     */
    Vector2i get_screen_size_lpix() const;

    /**
     * @brief Sets the scale factor of the window's backing store relative to logical screen space.
     *
     * @param ratio The logical-to-physical pixel ratio.
     */
    void set_logical_to_physical_pixel_ratio(double ratio);

    /**
     * @brief Gets the scale factor of the window's backing store relative to logical screen space.
     * @return The logical-to-physical pixel ratio.
     */
    double get_logical_to_physical_pixel_ratio() const;

    /**
     * @brief Sets the physical screen size in millimeters. If left unset, GazeTracker will query platform EDID.
     */
    void set_screen_size_mm(Vector2 size);
    Vector2 get_screen_size_mm() const;

    /**
     * @brief Sets the camera focal length in pixels.
     */
    void set_camera_focal_length_px(double f);
    double get_camera_focal_length_px() const;

    void set_camera_device_id(int id);
    int get_camera_device_id() const;

    void set_filter_min_cutoff(double val);
    double get_filter_min_cutoff() const;

    void set_filter_beta(double val);
    double get_filter_beta() const;

    void set_filter_d_cutoff(double val);
    double get_filter_d_cutoff() const;

    void set_yunet_model_path(String path);
    String get_yunet_model_path() const;

    void set_gaze_onnx_path(String path);
    String get_gaze_onnx_path() const;

    void set_expression_tracking_enabled(bool enabled);
    bool get_expression_tracking_enabled() const;

    Vector2 get_latest_projected_gaze() const { return latest_projected_gaze_px; }
    Vector2 get_latest_filtered_gaze() const { return latest_filtered_gaze_px; }
    bool is_face_detected() const { return is_face_tracked; }

    Transform3D get_head_transform() const;
    Transform3D get_camera_to_screen_transform() const;
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
    Vector2 project_gaze_ray_to_viewport(Vector3 origin, Vector3 direction) const;

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

