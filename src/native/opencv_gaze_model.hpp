/**
 * @file opencv_gaze_model.hpp
 * @brief ONNX Gaze Estimation Model using OpenCV DNN (Layer 3 - Native)
 *
 * Implements GazeModel to execute forward pass inference on gaze estimation ONNX/OpenVINO
 * networks (e.g. gaze-estimation-adas-0002). Reconciles input crops, head pose sign
 * conventions, and translates output raw direction vectors to GodotGaze space.
 */
#pragma once

#include "gaze_model.hpp"
#include <opencv2/dnn.hpp>
#include <string>

namespace Gaze {

class OpenCVGazeModel : public GazeModel {
private:
    std::string model_path;
    cv::dnn::Net net;
    PipelineConfig config;

public:
    OpenCVGazeModel(const std::string& gaze_onnx_path);
    virtual ~OpenCVGazeModel() = default;

    virtual bool initialize() override;
    virtual bool estimate_raw_gaze(const EyeCrops& crops, GazeVector3& out_gaze_dir_cv) override;
    virtual void set_config(const PipelineConfig& cfg) override { config = cfg; }
};

} // namespace Gaze
