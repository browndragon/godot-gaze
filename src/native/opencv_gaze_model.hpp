// src/native/opencv_gaze_model.hpp
#pragma once

#include "gaze_model.hpp"
#include <opencv2/dnn.hpp>
#include <string>

namespace Gaze {

class OpenCVGazeModel : public GazeModel {
private:
    std::string model_path;
    cv::dnn::Net net;

public:
    OpenCVGazeModel(const std::string& gaze_onnx_path);
    virtual ~OpenCVGazeModel() = default;

    virtual bool initialize() override;
    virtual bool estimate_raw_gaze(const EyeCrops& crops, GazeVector3& out_raw_gaze_dir) override;
};

} // namespace Gaze
