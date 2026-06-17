// src/core/face_pipeline.hpp
#pragma once

#include "camera_interface.hpp"
#include "math_defs.hpp"

namespace Gaze {

struct EyeCrops {
    bool face_detected = false;

    // Estimated head pose vectors relative to camera (in mm and radians)
    GazeVector3 head_pose_rotation;    // Rotational angles (pitch, yaw, roll)
    GazeVector3 head_pose_translation; // Position of the head

    // Left and right eye crops (standardized to 60x36 px)
    // grayscale buffer (60 * 36 = 2160 bytes)
    unsigned char left_eye_data[2160] = {0};
    unsigned char right_eye_data[2160] = {0};

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
};

} // namespace Gaze
