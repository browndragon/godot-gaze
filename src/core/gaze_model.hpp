/**
 * @file gaze_model.hpp
 * @brief Gaze Estimation Model interface (Layer 3)
 *
 * Defines the abstract interface for the gaze estimation inference stage.
 * Receives normalized eye crops and head pose parameters, executes the
 * underlying DNN (e.g. OpenVINO), and outputs a raw 3D gaze direction vector
 * in Space::OpenVINOADASGaze (+X right ear, +Y up, +Z forward towards camera).
 */
#pragma once

#include "face_pipeline.hpp"
#include "opencv_space_conversions.hpp"
#include "pipeline_config.hpp"

namespace Gaze {

class GazeModel {
public:
    virtual ~GazeModel() = default;

    // Load model weights (e.g. from ONNX) and initialize inference network
    virtual bool initialize() = 0;

    // Computes raw, uncalibrated 3D gaze direction vector in OpenVINO ADAS Gaze Space
    // (+X user right / display left, +Y up, +Z forward towards camera)
    virtual bool estimate_raw_gaze(const EyeCrops& crops, OpenVINOGazeVector3& out_gaze_dir_openvino) = 0;

    // Configure model settings
    virtual void set_config(const PipelineConfig& config) {}
};

} // namespace Gaze
