#include "ort_eye_state_model.hpp"
#include "platform_ort.hpp"
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
                session = platform_create_ort_session(env, model_buffer, &session_options);
            }
            else
            {
                if (model_path.empty())
                {
                    log_error("ORTEyeStateModelInitFailed", "reason", "Model path is empty");
                    return false;
                }
                session = platform_create_ort_session(env, model_path, &session_options);
            }

            if (!session)
            {
                log_error("ORTEyeStateModelSessionNull");
                return false;
            }

            Ort::AllocatorWithDefaultOptions allocator;
            auto in_name_alloc = session->GetInputNameAllocated(0, allocator);
            input_name = in_name_alloc.get();

            auto out_name_alloc = session->GetOutputNameAllocated(0, allocator);
            output_name = out_name_alloc.get();

            log_info("ORTEyeStateModelInitSuccess", "input", input_name.c_str(), "output", output_name.c_str());
            return true;
        }
        catch (const std::exception &e)
        {
            log_error("ORTEyeStateModelInitException", "what", e.what());
            return false;
        }
    }

    void ORTEyeStateModel::preprocess_eye_crop_32(const uint8_t *raw_crop_60_bgr, float *out_buffer)
    {
        // 32x32 BGR [1, 3, 32, 32] normalized as (pixel - 127.0f) / 255.0f
        constexpr int dst_w = 32;
        constexpr int dst_h = 32;
        constexpr int src_w = 60;
        constexpr int src_h = 60;
        constexpr int plane_size = dst_w * dst_h;

        float *b_plane = out_buffer;
        float *g_plane = out_buffer + plane_size;
        float *r_plane = out_buffer + 2 * plane_size;

        float scale_x = static_cast<float>(src_w) / dst_w;
        float scale_y = static_cast<float>(src_h) / dst_h;

        for (int y = 0; y < dst_h; ++y)
        {
            float src_y = (y + 0.5f) * scale_y - 0.5f;
            src_y = std::max(0.0f, std::min(src_y, static_cast<float>(src_h - 1)));
            int y0 = static_cast<int>(std::floor(src_y));
            int y1 = std::min(y0 + 1, src_h - 1);
            float dy = src_y - y0;

            for (int x = 0; x < dst_w; ++x)
            {
                float src_x = (x + 0.5f) * scale_x - 0.5f;
                src_x = std::max(0.0f, std::min(src_x, static_cast<float>(src_w - 1)));
                int x0 = static_cast<int>(std::floor(src_x));
                int x1 = std::min(x0 + 1, src_w - 1);
                float dx = src_x - x0;

                int dst_idx = y * dst_w + x;
                for (int c = 0; c < 3; ++c)
                {
                    float p00 = raw_crop_60_bgr[(y0 * src_w + x0) * 3 + c];
                    float p10 = raw_crop_60_bgr[(y0 * src_w + x1) * 3 + c];
                    float p01 = raw_crop_60_bgr[(y1 * src_w + x0) * 3 + c];
                    float p11 = raw_crop_60_bgr[(y1 * src_w + x1) * 3 + c];

                    float val = (1.0f - dx) * (1.0f - dy) * p00 +
                                dx * (1.0f - dy) * p10 +
                                (1.0f - dx) * dy * p01 +
                                dx * dy * p11;

                    float norm_val = (val - 127.0f) / 255.0f;
                    if (c == 0) b_plane[dst_idx] = norm_val;
                    else if (c == 1) g_plane[dst_idx] = norm_val;
                    else if (c == 2) r_plane[dst_idx] = norm_val;
                }
            }
        }
    }

    bool ORTEyeStateModel::estimate_openness(const uint8_t *raw_crop_60_bgr, float &out_openness)
    {
        out_openness = 1.0f;
        if (!session || !raw_crop_60_bgr)
        {
            return false;
        }

        std::vector<float> input_tensor_data(3 * 32 * 32);
        preprocess_eye_crop_32(raw_crop_60_bgr, input_tensor_data.data());

        std::vector<int64_t> input_shape = {1, 3, 32, 32};
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, input_tensor_data.data(), input_tensor_data.size(),
            input_shape.data(), input_shape.size());

        try
        {
            const char *input_names_ptr[] = {input_name.c_str()};
            const char *output_names_ptr[] = {output_name.c_str()};

            auto output_tensors = session->Run(
                Ort::RunOptions{nullptr},
                input_names_ptr,
                &input_tensor,
                1,
                output_names_ptr,
                1);

            if (output_tensors.empty())
            {
                return false;
            }

            const float *out_data = output_tensors[0].GetTensorData<float>();
            // Output shape is [1, 2, 1, 1] or [1, 2]: Class 0 = Closed, Class 1 = Open
            float open_score = out_data[1];
            out_openness = std::clamp(open_score, 0.0f, 1.0f);
            return true;
        }
        catch (const std::exception &e)
        {
            log_error("ORTEyeStateModelInferenceException", "what", e.what());
            return false;
        }
    }

} // namespace Gaze
