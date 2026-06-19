// Face detector and normalizer interface (Layer 2).
#pragma once

#include "camera_interface.hpp"
#include "math_defs.hpp"

namespace Gaze {

struct EyeCrops {
    bool face_detected = false;

    // Estimated head pose vectors relative to camera (in mm and radians)
    GazeVector3 head_pose_rotation;    // Rotational angles (pitch, yaw, roll)
    GazeVector3 head_pose_translation; // Position of the head

    // Left and right eye crops (standardized to 60x60 px, 3-channel BGR)
    // BGR buffer (60 * 60 * 3 = 10800 bytes)
    unsigned char left_eye_data[10800] = {0};
    unsigned char right_eye_data[10800] = {0};

    // 3D coordinates of left and right eye centers in camera space (in mm)
    // Used to calculate Z distance based on IPD (Interpupillary Distance)
    GazeVector3 left_eye_center_cam;
    GazeVector3 right_eye_center_cam;
};

class FacePipeline {
public:
    virtual ~FacePipeline() = default;

    // Load models (e.g. YuNet face detector) and initialize pipeline
    virtual bool initialize() = 0;

    // Process frame, detect faces, estimate head pose, and crop eyes
    virtual bool process_frame(const Frame& frame, EyeCrops& out_crops) = 0;

    // Configure camera focal length
    virtual void set_camera_focal_length_px(double f) {}
};

} // namespace Gaze
