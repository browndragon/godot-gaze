#include "onnx_eye_blink_estimator.hpp"
#include "platform_ort.hpp"
#include "log.hpp"
#include <algorithm>
#include <iostream>

namespace Gaze {

ONNXEyeBlinkEstimator::ONNXEyeBlinkEstimator(const std::string& model_file_path)
    : model_path(model_file_path), load_from_buffer(false) {}

ONNXEyeBlinkEstimator::ONNXEyeBlinkEstimator(const std::vector<uint8_t>& model_buf)
    : model_buffer(model_buf), load_from_buffer(true) {}

bool ONNXEyeBlinkEstimator::initialize() {
    try {
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

        if (load_from_buffer) {
            log_info("ONNXEyeBlinkEstimator_InitBuffer", "size", (int)model_buffer.size());
            session = platform_create_ort_session(env, model_buffer, &session_options);
        } else {
            log_info("ONNXEyeBlinkEstimator_InitPath", "path", model_path);
            session = platform_create_ort_session(env, model_path, &session_options);
        }

        if (!session) {
            log_error("ONNXEyeBlinkEstimator_SessionNull");
            return false;
        }

        log_info("ONNXEyeBlinkEstimator_InitSuccess");
        return true;
    } catch (const std::exception& e) {
        log_error("ONNXEyeBlinkEstimator_InitFailed", "what", e.what());
        return false;
    } catch (...) {
        log_error("ONNXEyeBlinkEstimator_InitFailedUnknown");
        return false;
    }
}

float ONNXEyeBlinkEstimator::run_single_crop_inference(const uint8_t* crop_rgb_60x60) {
    if (!session || !crop_rgb_60x60) return 0.0f;

    // Resize 60x60 RGB to 224x224 RGB with ImageNet normalization
    const int target_w = 224;
    const int target_h = 224;
    std::vector<float> input_tensor_values(1 * 3 * target_h * target_w);

    const float mean[3] = {0.485f, 0.456f, 0.406f};
    const float std[3] = {0.229f, 0.224f, 0.225f};

    for (int y = 0; y < target_h; y++) {
        int src_y = y * 60 / target_h;
        for (int x = 0; x < target_w; x++) {
            int src_x = x * 60 / target_w;
            int src_idx = (src_y * 60 + src_x) * 3;

            float r = (crop_rgb_60x60[src_idx + 0] / 255.0f - mean[0]) / std[0];
            float g = (crop_rgb_60x60[src_idx + 1] / 255.0f - mean[1]) / std[1];
            float b = (crop_rgb_60x60[src_idx + 2] / 255.0f - mean[2]) / std[2];

            int dst_spatial = y * target_w + x;
            input_tensor_values[0 * 50176 + dst_spatial] = r;
            input_tensor_values[1 * 50176 + dst_spatial] = g;
            input_tensor_values[2 * 50176 + dst_spatial] = b;
        }
    }

    std::vector<int64_t> input_shape = {1, 3, target_h, target_w};
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info,
        input_tensor_values.data(),
        input_tensor_values.size(),
        input_shape.data(),
        input_shape.size()
    );

    Ort::AllocatorWithDefaultOptions default_allocator;
    Ort::AllocatedStringPtr input_name_ptr = session->GetInputNameAllocated(0, default_allocator);
    Ort::AllocatedStringPtr output_name_ptr = session->GetOutputNameAllocated(0, default_allocator);
    const char* input_names[] = {input_name_ptr.get()};
    const char* output_names[] = {output_name_ptr.get()};

    try {
        auto output_tensors = session->Run(
            Ort::RunOptions{nullptr},
            input_names,
            &input_tensor,
            1,
            output_names,
            1
        );

        if (output_tensors.empty()) return 0.0f;
        float* logits = output_tensors[0].GetTensorMutableData<float>();
        
        float max_l = std::max(logits[0], logits[1]);
        float exp0 = std::exp(logits[0] - max_l);
        float exp1 = std::exp(logits[1] - max_l);
        float prob_open = exp1 / (exp0 + exp1);
        return prob_open;
    } catch (const std::exception& e) {
        log_error(std::string("ONNXEyeBlinkEstimator_RunError: ") + e.what());
        return 0.0f;
    }
}

void ONNXEyeBlinkEstimator::estimate_openness(
    const uint8_t* left_crop_rgb,
    const uint8_t* right_crop_rgb,
    float& out_left_openness,
    float& out_right_openness
) {
    if (!session) {
        out_left_openness = 1.0f;
        out_right_openness = 1.0f;
        return;
    }

    out_left_openness = run_single_crop_inference(left_crop_rgb);
    out_right_openness = run_single_crop_inference(right_crop_rgb);
}

} // namespace Gaze
