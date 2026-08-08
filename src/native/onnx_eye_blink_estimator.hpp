#pragma once
#include "eye_blink_estimator.hpp"
#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <memory>

namespace Gaze {

/**
 * @brief ONNX-based Eye Blink and Openness Estimator running eye_openness.ort.
 */
class ONNXEyeBlinkEstimator : public EyeBlinkEstimator {
private:
    std::string model_path;
    std::vector<uint8_t> model_buffer;
    bool load_from_buffer = false;

    std::unique_ptr<Ort::Session> session;
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "ONNXEyeBlinkEstimator"};
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    float run_single_crop_inference(const uint8_t* crop_rgb_60x60);

public:
    ONNXEyeBlinkEstimator(const std::string& model_file_path);
    ONNXEyeBlinkEstimator(const std::vector<uint8_t>& model_buf);
    ~ONNXEyeBlinkEstimator() override = default;

    bool initialize();

    void estimate_openness(
        const uint8_t* left_crop_rgb,
        const uint8_t* right_crop_rgb,
        float& out_left_openness,
        float& out_right_openness
    ) override;
};

} // namespace Gaze
