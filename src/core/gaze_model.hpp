// Raw model inference interface (Layer 3).
#pragma once

#include "face_pipeline.hpp"
#include "math_defs.hpp"

namespace Gaze {

class GazeModel {
public:
    virtual ~GazeModel() = default;

    // Load model weights (e.g. from ONNX) and initialize inference network
    virtual bool initialize() = 0;

    // Computes the raw, uncalibrated 3D gaze direction vector in camera space
    // X is right, Y is down, Z is forward (optical axis)
    virtual bool estimate_raw_gaze(const EyeCrops& crops, GazeVector3& out_raw_gaze_dir) = 0;
};

} // namespace Gaze
