/**
 * @file pipeline_config.hpp
 * @brief Pipeline Configurations configuration struct (Layer 2)
 *
 * Defines parameters for facial landmarking and crop normalization, including
 * yaw/pitch translation compensation gains, custom nose landmarks, target IPD,
 * and downscaled detection image dimensions.
 */
#pragma once
#include "face_model_geometry.hpp"

namespace Gaze {

struct PipelineConfig {
    double pitch_t_gain = 0.0;
    double yaw_t_gain = 0.0;
    
    double nose_y = FaceModelGeometry::DEFAULT_NOSE_Y;
    double nose_z = FaceModelGeometry::DEFAULT_NOSE_Z; 
    
    // Default IPD in millimeters
    double ipd_mm = 63.0;

    // Resizing dimensions for face detection
    int face_detect_width = 160;
    int face_detect_height = 128;

    // Desired camera capture resolution (default 640x480)
    int desired_camera_width = 640;
    int desired_camera_height = 480;
};

} // namespace Gaze
