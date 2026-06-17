// Decoupled calibration data structure.
#pragma once

namespace Gaze {

struct GazeCalibration {
    // 3D angular pitch and yaw bias corrections (in radians)
    // Applied to the gaze vector before projection.
    double bias_pitch = 0.0;
    double bias_yaw = 0.0;

    // 2D screen-space pixel bias corrections
    // Added to the final pixel coordinate after projection.
    double bias_pixel_x = 0.0;
    double bias_pixel_y = 0.0;

    GazeCalibration() = default;
    GazeCalibration(double pitch, double yaw, double px, double py)
        : bias_pitch(pitch), bias_yaw(yaw), bias_pixel_x(px), bias_pixel_y(py) {}
};

} // namespace Gaze
