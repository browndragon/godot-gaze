#pragma once

namespace Gaze {

struct PipelineConfig {
    double pitch_t_gain = 0.0;
    double yaw_t_gain = 0.0;
    
    double nose_y = -3.5;
    double nose_z = -13.0; 
    
    // Default IPD in millimeters
    double ipd_mm = 63.0;
};

} // namespace Gaze
