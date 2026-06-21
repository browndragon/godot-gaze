#pragma once

namespace Gaze {

struct PipelineConfig {
    double pitch_t_gain = 0.0;
    double yaw_t_gain = 0.0;
    
    // Default nose_z set to a realistic average human depth (~4.5 cm)
    double nose_z = -45.0; 
    
    // Default IPD in millimeters
    double ipd_mm = 63.0;
};

} // namespace Gaze
