// 3D ray-plane intersection, focal depth calculation, and 3D/2D analytical calibration (Layer 4).
#pragma once

#include "camera_placement.hpp"
#include "gaze_calibration.hpp"
#include "math_defs.hpp"

namespace Gaze {

class ProjectionEngine {
private:
    CameraPlacement placement;
    GazeCalibration calibration;
    GazeVector2 screen_size_pixels = GazeVector2(1920.0, 1080.0);
    GazeVector2 screen_size_mm = GazeVector2(527.0, 296.0); // e.g. Typical 24-inch 16:9 monitor
    double camera_focal_length_px = 1000.0;                 // For Z depth estimation if needed

public:
    ProjectionEngine() = default;

    // Setters / Getters
    void set_camera_placement(const CameraPlacement& p) { placement = p; }
    CameraPlacement get_camera_placement() const { return placement; }

    void set_calibration(const GazeCalibration& c) { calibration = c; }
    GazeCalibration get_calibration() const { return calibration; }

    void set_screen_size_pixels(const GazeVector2& size) { screen_size_pixels = size; }
    GazeVector2 get_screen_size_pixels() const { return screen_size_pixels; }

    void set_screen_size_mm(const GazeVector2& size) { screen_size_mm = size; }
    GazeVector2 get_screen_size_mm() const { return screen_size_mm; }

    void set_camera_focal_length_px(double f) { camera_focal_length_px = f; }
    double get_camera_focal_length_px() const { return camera_focal_length_px; }

    // Estimate depth Z (mm) from 2D eye center pixel distance
    double estimate_depth_z(double eye_distance_px, double ipd_mm = 63.0) const;

    // Convert screen pixel coordinates to screen millimeter coordinates (relative to center, Y-down +)
    GazeVector2 pixel_to_millimeter(const GazeVector2& pixel) const;

    // Map screen millimeter coordinates (relative to center, Y-down +) to 3D position in Camera Space
    GazeVector3 screen_mm_to_camera_space(const GazeVector2& screen_mm) const;

    // Apply pitch/yaw angular bias to raw 3D gaze vector
    GazeVector3 apply_3d_bias(const GazeVector3& raw_gaze_dir) const;

    // Projects the raw gaze vector and returns pixel coordinates (applying all biases)
    bool project_gaze(const GazeVector3& gaze_origin_cam, 
                      const GazeVector3& raw_gaze_dir_cam, 
                      GazeVector2& out_pixel) const;

    // Helper to calculate the 3D angular bias (pitch/yaw) that maps the current raw gaze ray
    // to a target screen pixel coordinate. Updates the calibration struct's 3D parameters.
    bool calibrate_3d_bias(const GazeVector3& gaze_origin_cam,
                           const GazeVector3& raw_gaze_dir_cam,
                           const GazeVector2& target_pixel,
                           GazeCalibration& out_calib) const;

    // Helper to calculate the 2D screen-space pixel bias that shifts the projected coordinate
    // to a target screen pixel coordinate. Updates the calibration struct's 2D parameters.
    bool calibrate_2d_bias(const GazeVector3& gaze_origin_cam,
                           const GazeVector3& raw_gaze_dir_cam,
                           const GazeVector2& target_pixel,
                           GazeCalibration& out_calib) const;

    // Map OpenCV Face-to-Camera translation and rotation to standard Camera Space GazeTransform3D
    GazeTransform3D get_head_transform_in_camera_space(const GazeVector3& opencv_translation,
                                                       const GazeVector3& opencv_rotation_deg) const;
};

} // namespace Gaze
