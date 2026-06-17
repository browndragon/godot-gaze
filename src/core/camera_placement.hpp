// src/core/camera_placement.hpp
#pragma once

#include "math_defs.hpp"

namespace Gaze {

struct CameraPlacement {
    // Camera position offset relative to the screen center (in mm)
    // +X is right, +Y is down, +Z is forward (towards user, out of screen)
    GazeVector3 offset = GazeVector3(0.0, 0.0, 0.0);

    // Camera tilt angle in degrees (rotated about its local X-axis).
    // Positive values represent a downward tilt toward the user.
    double tilt_degrees = 0.0;

    CameraPlacement() = default;
    CameraPlacement(const GazeVector3& off, double tilt) : offset(off), tilt_degrees(tilt) {}
};

} // namespace Gaze
