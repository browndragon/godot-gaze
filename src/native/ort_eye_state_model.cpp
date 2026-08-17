/**
 * @file ort_eye_state_model.cpp
 * @brief ONNX Runtime Eye Openness / Blink Classifier Implementation
 */
#include "ort_eye_state_model.hpp"
#include "../core/log.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace Gaze
{
    ORTEyeStateModel::ORTEyeStateModel(const std::string &p_model_path)
        : model_path(p_model_path), load_from_buffer(false), memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    {
    }

    ORTEyeStateModel::ORTEyeStateModel(const std::vector<uint8_t> &buffer)
        : model_buffer(buffer), load_from_buffer(true), memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    {
    }

    bool ORTEyeStateModel::initialize()
    {
        try
        {
            session_options.SetIntraOpNumThreads(1);
            session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

            if (load_from_buffer)
            {
                if (model_buffer.empty())
                {
                    log_error("ORTEyeStateModelInitFailed", "reason", "Buffer is empty");
                    return false;
                }
                session = std::make_unique<Ort::Session>(env, model_buffer.data(), model_buffer.size(), session_options);
            }
            else
            {
                if (model_path.empty())
                {
                    log_error("ORTEyeStateModelInitFailed", "reason", "Model path is empty");
                    return false;
                }
                session = std::make_unique<Ort::Session>(env, model_path.c_str(), session_options);
            }

            log_info("ORTEyeStateModelInitSuccess");
            return true;
        }
        catch (const std::exception &e)
        {
            log_error("ORTEyeStateModelInitException", "what", e.what());
            return false;
        }
    }

    void ORTEyeStateModel::preprocess_eye_crop(const uint8_t *raw_crop_bgr, float *out_buffer)
    {
        // 60x60 BGR -> RGB normalized float [0.0, 1.0] in NCHW format [1, 3, 60, 60]
        constexpr int width = 60;
        constexpr int height = 60;
        constexpr int plane_size = width * height;

        float *r_plane = out_buffer;
        float *g_plane = out_buffer + plane_size;
        float *b_plane = out_buffer + 2 * plane_size;

        // ImageNet normalization: mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]
        constexpr float mean_r = 0.485f, mean_g = 0.456f, mean_b = 0.406f;
        constexpr float std_r = 0.229f, std_g = 0.224f, std_b = 0.225f;

        for (int i = 0; i < plane_size; ++i)
        {
            float b = raw_crop_bgr[i * 3 + 0] / 255.0f;
            float g = raw_crop_bgr[i * 3 + 1] / 255.0f;
            float r = raw_crop_bgr[i * 3 + 2] / 255.0f;

            r_plane[i] = (r - mean_r) / std_r;
            g_plane[i] = (g - mean_g) / std_g;
            b_plane[i] = (b - mean_b) / std_b;
        }
    }

    bool ORTEyeStateModel::estimate_openness(const uint8_t *raw_crop_bgr, float &out_openness)
    {
        out_openness = 1.0f;
        if (!session || !raw_crop_bgr)
        {
            return false;
        }

        std::vector<float> input_tensor_data(3 * 60 * 60);
        preprocess_eye_crop(raw_crop_bgr, input_tensor_data.data());

        std::vector<int64_t> input_shape = {1, 3, 60, 60};
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, input_tensor_data.data(), input_tensor_data.size(),
            input_shape.data(), input_shape.size());

        try
        {
            auto output_tensors = session->Run(
                Ort::RunOptions{nullptr},
                input_names.data(),
                &input_tensor,
                1,
                output_names.data(),
                output_names.size());

            if (output_tensors.empty())
            {
                return false;
            }

            float *out_data = output_tensors[0].GetTensorMutableData<float>();
            float blink_prob = out_data[0];
            out_openness = std::clamp(1.0f - blink_prob, 0.0f, 1.0f);
            return true;
        }
        catch (const std::exception &e)
        {
            log_error("ORTEyeStateModelInferenceException", "what", e.what());
            return false;
        }
    }

} // namespace Gaze
