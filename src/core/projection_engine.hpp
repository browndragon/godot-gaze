/**
 * @file projection_engine.hpp
 * @brief Ray-Plane Projection Engine and Calibration Solver (Layer 4)
 *
 * Implements the mathematical models for projecting raw 3D gaze rays onto
 * physical screen planes, incorporating offset, resolution, and tilt parameters.
 * Computes analytical calibration offsets (both 3D angular biases and 2D pixel offsets)
 * and estimates focal depth from eye distance.
 */
#pragma once

#include "camera_placement.hpp"
#include "gaze_calibration.hpp"
#include "opencv_space_conversions.hpp"

namespace Gaze
{

    class ProjectionEngine
    {
    private:
        CameraPlacement placement;
        GazeCalibration calibration;
        GodotDisplayVector2 screen_size_pixels = GodotDisplayVector2(1920.0, 1080.0);
        SpacedVector2<Space::GodotDisplayMm> screen_size_mm = SpacedVector2<Space::GodotDisplayMm>(527.0, 296.0); // e.g. Typical 24-inch 16:9 monitor
        double camera_focal_length_px = 1000.0;                 // For Z depth estimation if needed

    public:
        ProjectionEngine() = default;

        // Setters / Getters
        void set_camera_placement(const CameraPlacement &p) { placement = p; }
        CameraPlacement get_camera_placement() const { return placement; }

        void set_calibration(const GazeCalibration &c) { calibration = c; }
        GazeCalibration get_calibration() const { return calibration; }

        void set_screen_size_pixels(const GodotDisplayVector2 &size) { screen_size_pixels = size; }
        GodotDisplayVector2 get_screen_size_pixels() const { return screen_size_pixels; }

        void set_screen_size_mm(const SpacedVector2<Space::GodotDisplayMm> &size) { screen_size_mm = size; }
        SpacedVector2<Space::GodotDisplayMm> get_screen_size_mm() const { return screen_size_mm; }

        void set_camera_focal_length_px(double f) { camera_focal_length_px = f; }
        double get_camera_focal_length_px() const { return camera_focal_length_px; }

        // Estimate depth Z (mm) from 2D eye center pixel distance
        double estimate_depth_z(double eye_distance_px, double ipd_mm = 63.0) const;

        // Convert screen pixel coordinates to screen millimeter coordinates (relative to center, Y-down +)
        SpacedVector2<Space::GodotDisplayMm> pixel_to_millimeter(const GodotDisplayVector2 &pixel) const;

        // Map screen millimeter coordinates (relative to center, Y-down +) to 3D position in Camera Space
        GodotCameraVector3 screen_mm_to_camera_space(const SpacedVector2<Space::GodotDisplayMm> &screen_mm) const;

        // Apply pitch/yaw angular bias to raw 3D gaze vector
        GodotCameraVector3 apply_3d_bias(const GodotCameraVector3 &raw_gaze_dir) const;

        // Projects the raw gaze vector and returns pixel coordinates (applying all biases)
        bool project_gaze(const GodotCameraVector3 &gaze_origin_cam,
                          const GodotCameraVector3 &raw_gaze_dir_cam,
                          GodotDisplayVector2 &out_pixel) const;
    };

} // namespace Gaze
