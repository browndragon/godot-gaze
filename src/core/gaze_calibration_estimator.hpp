/**
 * @file gaze_calibration_estimator.hpp
 * @brief Zero-dependency C++ optimization estimator for screen geometry (Layer 4)
 */
#pragma once

#include "math_defs.hpp"
#include <vector>

namespace Gaze {

struct CalibrationSample {
    GazeVector3 gaze_origin;      // Eye center in camera space, mm
    GazeVector3 gaze_direction;   // Raw gaze direction unit vector in camera space
    GazeVector2 target_pixel_ppix; // Target position in physical screen pixels
};

struct CalibrationWeights {
    // Prior weights (regularization factors)
    // Locked to 1e9 to prevent physical parameters from shifting during calibration
    double aspect_prior = 1e9;
    double size_prior = 1e9;
    double offset_x = 1e9;
    double offset_y = 1e9;
    double offset_z = 1e9;
    double tilt = 1e9;
    double bias = 10.0;

    // Solver parameter boundaries
    double min_pixel_size = 0.05;
    double max_pixel_size = 2.0;
    double max_camera_offset_x = 300.0;
    double min_camera_offset_y = -150.0;
    double max_camera_offset_y = 400.0;
    double min_camera_offset_z = -100.0;
    double max_camera_offset_z = 300.0;
    double max_camera_tilt = 75.0;
    double max_bias = 0.6;

    // Initial simplex step sizes
    double step_pixel_size = 0.02;
    double step_camera_offset = 10.0;
    double step_camera_tilt = 3.0;
    double step_bias = 0.03;

    // Solver execution parameters
    int max_iterations = 3000;
    double convergence_threshold = 1e-5;
    double penalty_multiplier = 1e6;
};

class CalibrationEstimator {
public:
    static bool estimate(
        const std::vector<CalibrationSample>& samples,
        const GazeVector2& screen_size_pixels,
        const GazeVector2& initial_pixel_size_mm,
        const GazeVector3& initial_camera_offset,
        double initial_camera_tilt_deg,
        GazeVector2& out_pixel_size_mm,
        GazeVector3& out_camera_offset,
        double& out_camera_tilt_deg,
        double& out_bias_pitch,
        double& out_bias_yaw,
        const CalibrationWeights& weights = CalibrationWeights()
    );
};

} // namespace Gaze
