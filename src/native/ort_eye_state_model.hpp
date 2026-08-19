/**
 * @file ort_eye_state_model.hpp
 * @brief ONNX Runtime Eye Openness / Blink Classifier Implementation
 */
#pragma once

#include "gaze_model.hpp"
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <vector>
#include <string>

namespace Gaze
{
    class ORTEyeStateModel
    {
    private:
        std::string model_path;
        std::vector<uint8_t> model_buffer;
        bool load_from_buffer = false;

        Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "GodotGazeEyeState"};
        Ort::SessionOptions session_options;
        std::unique_ptr<Ort::Session> session;
        Ort::MemoryInfo memory_info{nullptr};

        std::string input_name;
        std::string output_name;

        void preprocess_eye_crop_32(const uint8_t *raw_crop_60_bgr, float *out_buffer);

    public:
        ORTEyeStateModel(const std::string &model_path);
        ORTEyeStateModel(const std::vector<uint8_t> &buffer);
        ~ORTEyeStateModel() = default;

        bool initialize();

        /**
         * @brief Estimates eye openness probability [0.0, 1.0] for a 60x60 BGR eye crop.
         * @param raw_crop_60_bgr Pointer to 60x60x3 BGR pixel data.
         * @param out_openness Output float score [0.0 = closed, 1.0 = fully open].
         * @return True if inference succeeded.
         */
        bool estimate_openness(const uint8_t *raw_crop_60_bgr, float &out_openness);
    };

} // namespace Gaze
