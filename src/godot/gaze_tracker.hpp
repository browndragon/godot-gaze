// Exposes tracker node properties, handles platform execution branches (native vs Web), applies filtering, and emits signals.
#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/transform3d.hpp>
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

namespace godot {

class GazeTracker : public Node {
    GDCLASS(GazeTracker, Node);

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
    Ref<GazeCalibrationResource> calibration_resource;
    Ref<GazePipelineConfig> pipeline_config;
    Vector3 camera_offset = Vector3(0.0, 148.0, 0.0); // mm relative to screen center (flush with bezel)
    double camera_tilt = 0.0;                           // degrees
    Vector2i screen_size_pixels = Vector2i(-1, -1);
    Vector2 screen_size_mm = Vector2(-1.0, -1.0);
    double camera_focal_length_px = 1000.0;
    int camera_device_id = 0;

    double filter_min_cutoff = 1.0;
    double filter_beta = 0.01;
    double filter_d_cutoff = 1.0;

    String yunet_model_path;
    String gaze_onnx_path;
    bool expression_tracking_enabled = false;

    // Runtime state (latest estimations)
    Vector2 latest_projected_gaze_px;
    Vector2 latest_filtered_gaze_px;
    bool tracker_initialized = false;
    bool is_face_tracked = false;
    Gaze::GazeVector3 latest_gaze_origin = Gaze::GazeVector3(0.0, 0.0, 500.0);
    Gaze::GazeVector3 latest_gaze_dir = Gaze::GazeVector3(0.0, 0.0, -1.0);
    Gaze::EyeCrops latest_crops;

    void update_projection_parameters();
    void update_filter_parameters();
    void update_pipeline_config();

protected:
    static void _bind_methods();

public:
    GazeTracker();
    virtual ~GazeTracker();

    virtual void _ready() override;
    virtual void _process(double delta) override;

    // Tracker control
    bool initialize_tracker();
    void stop_tracker();

    // Calibration routines
    void calibrate_3d(Vector2 target_pixel);
    void calibrate_2d(Vector2 target_pixel);
    void clear_calibration();

    // Web/WASM sidecar feed APIs
    void feed_gaze_web(Vector3 origin, Vector3 direction);
    void feed_expression_web(String name, double value);

    // Getters / Setters for properties
    void set_pipeline_config(const Ref<GazePipelineConfig>& res);
    Ref<GazePipelineConfig> get_pipeline_config() const;

    void set_calibration_resource(const Ref<GazeCalibrationResource>& res);
    Ref<GazeCalibrationResource> get_calibration_resource() const;

    void set_camera_offset(Vector3 offset);
    Vector3 get_camera_offset() const;

    void set_camera_tilt(double tilt);
    double get_camera_tilt() const;

    void set_screen_size_pixels(Vector2i size);
    Vector2i get_screen_size_pixels() const;

    void set_screen_size_mm(Vector2 size);
    Vector2 get_screen_size_mm() const;

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

    Vector3 get_raw_head_rotation() const;
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
    Vector3 get_raw_left_eye_center() const;
    Vector3 get_raw_right_eye_center() const;
    Vector3 get_raw_gaze_direction() const;

    const Gaze::ProjectionEngine& get_projection_engine() const { return projection_engine; }
};

} // namespace godot
