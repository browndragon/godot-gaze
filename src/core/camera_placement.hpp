/**
 * @file camera_placement.hpp
 * @brief Camera Placement configuration (Layer 4)
 *
 * Defines the physical position and orientation offset of the hardware camera
 * relative to the center of the display screen. Used by the projection engine
 * to map camera-space coordinates back to the physical screen coordinate system.
 */
#pragma once

#include "opencv_space_conversions.hpp"

namespace Gaze {

struct CameraPlacement {
    // Camera position offset relative to top-bezel center in GodotCamera space (in mm)
    // +X is camera right / display left, +Y is camera up, -Z is forward towards user
    GodotCameraVector3 offset = GodotCameraVector3(0.0, 0.0, 0.0);

    // Camera tilt angle in degrees (rotated about its local X-axis).
    // Positive values represent a downward tilt toward the user.
    double tilt_degrees = 0.0;

    CameraPlacement() = default;
    CameraPlacement(const GodotCameraVector3& off, double tilt) : offset(off), tilt_degrees(tilt) {}
};

} // namespace Gaze
