/**
 * @file gaze_calibration.hpp
 * @brief Gaze Calibration parameters (Layer 4)
 */
#pragma once

#include "math_defs.hpp"

namespace Gaze
{

    /**
     * @struct GazeCalibration
     * @brief Calibration parameters for physical monitors and user biological gaze biases.
     */
    struct GazeCalibration
    {
        // Physical monitor pixel size in millimeters (width and height).
        GazeVector2 pixel_size_mm = GazeVector2(0.25, 0.25);

        // Camera offset relative to monitor center in millimeters (+X right, +Y up, +Z out of screen).
        GazeVector3 camera_offset = GazeVector3(0.0, 148.0, 0.0);

        // Tilt angle of the camera in degrees (downwards tilt is positive).
        double camera_tilt = 0.0;

        // 3D angular pitch and yaw bias corrections in radians.
        // Applied directly to the gaze vector before projection.
        double bias_pitch = 0.0;
        double bias_yaw = 0.0;

        // Scale multipliers to dampen/gain the angular pitch/yaw rotation signal.
        double scale_yaw = 1.0;
        double scale_pitch = 1.0;

        GazeCalibration() = default;
        GazeCalibration(double pitch, double yaw)
            : bias_pitch(pitch), bias_yaw(yaw) {}
    };

} // namespace Gaze
