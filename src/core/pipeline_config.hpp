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
};

} // namespace Gaze
