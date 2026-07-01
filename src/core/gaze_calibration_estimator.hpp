/**
 * @file gaze_calibration_estimator.hpp
 * @brief Zero-dependency C++ optimization estimator for screen geometry (Layer 4)
 */
#pragma once

#include "math_defs.hpp"
#include <vector>

namespace Gaze
{

    struct CalibrationSample
    {
        GazeVector3 gaze_origin;    // Eye center in camera space, mm
        GazeVector3 gaze_direction; // Raw gaze direction unit vector in camera space
        GazeVector2 target_pos_mm;  // Target position in screen millimeters (relative to center: +X right, +Y up)
    };

    // TODO: Is this code still on the critical path/in use in our code?
    // I don't love it: many of these values either seem automatically derivable or otherwise kind of weird.
    // For instance, on macos we could examine the AVCaptureDevice for `position` == `front`, then use the display info to calculate information abt the notch on the screen to know about the camera.
    // We're already writing our own windows equivalent, which should behave similarly. On android & ios, we can just _assume_ a connected/notched webcam.
    // etc etc; the need to "solve for" camera offset xyz and tilt feels VERY bad to me.
    // The one case we definitely do still need it has to do with browsers, where we know very little about the underlying machine.
    // In those cases, we already MUST do display calibration for good results. Is it possible to do some sort of webcam calibration at the same time?
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
            const GazeVector2 &screen_size_mm,
            const GazeVector3 &initial_camera_offset,
            double initial_camera_tilt_deg,
            bool freeze_camera_params,
            GazeVector3 &out_camera_offset,
            double &out_camera_tilt_deg,
            double &out_bias_pitch,
            double &out_bias_yaw,
            const CalibrationWeights &weights = CalibrationWeights());
    };

} // namespace Gaze
