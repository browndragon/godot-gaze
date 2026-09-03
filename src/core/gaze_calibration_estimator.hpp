/**
 * @file gaze_calibration_estimator.hpp
 * @brief Zero-dependency C++ optimization estimator for screen geometry (Layer 4)
 */
#pragma once

#include "opencv_space_conversions.hpp"
#include <vector>

namespace Gaze
{

    struct CalibrationSample
    {
        GodotCameraVector3 gaze_origin;                           // Eye center in GodotCamera space, mm
        GodotCameraVector3 gaze_direction;                        // Raw gaze direction unit vector in GodotCamera space (+Z towards screen)
        SpacedVector2<Space::GodotDisplayMm> target_pos_mm;       // Target position in display millimeters from top-left (0..W_mm, 0..H_mm, +X right, +Y down)
    };

    struct CalibrationWeights
    {
        // Prior weights (regularization factors)
        // Locked to 1e9 to prevent physical parameters from shifting during calibration
        double offset_x = 1e9;
        double offset_y = 1e9;
        double offset_z = 1e9;
        double tilt = 1e9;
        double bias = 10.0;

        // Solver parameter boundaries
        double max_camera_offset_x = 300.0;
        double min_camera_offset_y = -150.0;
        double max_camera_offset_y = 400.0;
        double min_camera_offset_z = -100.0;
        double max_camera_offset_z = 300.0;
        double max_camera_tilt = 75.0;
        double max_bias = 0.6;

        // Initial simplex step sizes
        double step_camera_offset = 10.0;
        double step_camera_tilt = 3.0;
        double step_bias = 0.03;

        // Solver execution parameters
        int max_iterations = 3000;
        double convergence_threshold = 1e-5;
        double penalty_multiplier = 1e6;
    };

    class CalibrationEstimator
    {
    public:
        static bool estimate(
            const std::vector<CalibrationSample> &samples,
            const SpacedVector2<Space::GodotDisplayMm> &screen_size_mm,
            const GodotCameraVector3 &initial_camera_offset,
            double initial_camera_tilt_deg,
            bool freeze_camera_params,
            GodotCameraVector3 &out_camera_offset,
            double &out_camera_tilt_deg,
            double &out_bias_pitch,
            double &out_bias_yaw,
            const CalibrationWeights &weights = CalibrationWeights());
    };

} // namespace Gaze
