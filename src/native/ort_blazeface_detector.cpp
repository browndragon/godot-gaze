#include "ort_blazeface_detector.hpp"
#include "platform_ort.hpp"
#include "log.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace Gaze {

BlazeFaceDetector::BlazeFaceDetector(const std::string& model_path)
    : model_path_(model_path), load_from_buffer(false), env(ORT_LOGGING_LEVEL_WARNING, "BlazeFaceDetector"), memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {}

BlazeFaceDetector::BlazeFaceDetector(const std::vector<uint8_t>& model_buffer)
    : model_buffer_(model_buffer), load_from_buffer(true), env(ORT_LOGGING_LEVEL_WARNING, "BlazeFaceDetector"), memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {}

BlazeFaceDetector::~BlazeFaceDetector() {}

void BlazeFaceDetector::generate_anchors() {
    anchors.clear();
    anchors.reserve(896);

    // Feature map 0: 16x16 grid, 2 anchors per cell
    for (int y = 0; y < 16; y++) {
        float cy = (y + 0.5f) / 16.0f;
        for (int x = 0; x < 16; x++) {
            float cx = (x + 0.5f) / 16.0f;
            anchors.push_back({cx, cy, 1.0f, 1.0f});
            anchors.push_back({cx, cy, 1.0f, 1.0f});
        }
    }

    // Feature map 1: 8x8 grid, 6 anchors per cell
    for (int y = 0; y < 8; y++) {
        float cy = (y + 0.5f) / 8.0f;
        for (int x = 0; x < 8; x++) {
            float cx = (x + 0.5f) / 8.0f;
            for (int k = 0; k < 6; k++) {
                anchors.push_back({cx, cy, 1.0f, 1.0f});
            }
        }
    }
}

bool BlazeFaceDetector::initialize() {
    try {
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

        if (load_from_buffer) {
            log_info("BlazeFaceDetector_InitBuffer", "size", (int)model_buffer_.size());
            session = platform_create_ort_session(env, model_buffer_, &session_options);
        } else {
            log_info("BlazeFaceDetector_InitPath", "path", model_path_);
            session = platform_create_ort_session(env, model_path_, &session_options);
        }

        if (!session) {
            log_error("BlazeFaceDetector_SessionNull");
            return false;
        }

        generate_anchors();
        is_initialized = true;
        log_info("BlazeFaceDetector_InitSuccess");
        return true;
    } catch (const std::exception& e) {
        log_error("BlazeFaceDetector_InitFailed", "what", e.what());
        return false;
    }
}

bool BlazeFaceDetector::detect(const Frame& input_frame, BlazeFaceDetection& out_detection) {
    if (!is_initialized || !session || !input_frame.data || input_frame.width <= 0 || input_frame.height <= 0) {
        out_detection.face_detected = false;
        return false;
    }

    const int target_w = 128;
    const int target_h = 128;
    std::vector<float> input_tensor_values(1 * target_h * target_w * 3);

    for (int y = 0; y < target_h; y++) {
        int src_y = y * input_frame.height / target_h;
        for (int x = 0; x < target_w; x++) {
            int src_x = x * input_frame.width / target_w;
            int src_idx = (src_y * input_frame.width + src_x) * 3;

            float b = static_cast<float>(input_frame.data[src_idx + 0]);
            float g = static_cast<float>(input_frame.data[src_idx + 1]);
            float r = static_cast<float>(input_frame.data[src_idx + 2]);

            int dst_spatial = (y * target_w + x) * 3;
            input_tensor_values[dst_spatial + 0] = (r - 127.5f) / 127.5f;
            input_tensor_values[dst_spatial + 1] = (g - 127.5f) / 127.5f;
            input_tensor_values[dst_spatial + 2] = (b - 127.5f) / 127.5f;
        }
    }

    std::vector<int64_t> input_shape = {1, target_h, target_w, 3};
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info,
        input_tensor_values.data(),
        input_tensor_values.size(),
        input_shape.data(),
        input_shape.size()
    );

    Ort::AllocatorWithDefaultOptions default_allocator;
    Ort::AllocatedStringPtr input_name_ptr = session->GetInputNameAllocated(0, default_allocator);
    const char* input_names[] = {input_name_ptr.get()};

    size_t num_outputs = session->GetOutputCount();
    std::vector<Ort::AllocatedStringPtr> output_name_ptrs;
    std::vector<const char*> output_names;
    for (size_t i = 0; i < num_outputs; i++) {
        output_name_ptrs.push_back(session->GetOutputNameAllocated(i, default_allocator));
        output_names.push_back(output_name_ptrs.back().get());
    }

    try {
        auto output_tensors = session->Run(
            Ort::RunOptions{nullptr},
            input_names,
            &input_tensor,
            1,
            output_names.data(),
            output_names.size()
        );

        if (output_tensors.size() < 2) {
            out_detection.face_detected = false;
            return false;
        }

        const float* raw_boxes = output_tensors[0].GetTensorData<float>();
        const float* raw_scores = output_tensors[1].GetTensorData<float>();

        float max_score = -1e9f;
        int best_idx = -1;

        auto sigmoid = [](float x) { return 1.0f / (1.0f + std::exp(-x)); };

        for (int i = 0; i < 896; i++) {
            float score = sigmoid(raw_scores[i]);
            if (score > max_score) {
                max_score = score;
                best_idx = i;
            }
        }

        if (best_idx < 0 || max_score < 0.50f) {
            out_detection.face_detected = false;
            return false;
        }

        const Anchor& anchor = anchors[best_idx];
        const float* box_data = &raw_boxes[best_idx * 16];

        float cx = (box_data[0] / 128.0f) + anchor.x_center;
        float cy = (box_data[1] / 128.0f) + anchor.y_center;
        float w = box_data[2] / 128.0f;
        float h = box_data[3] / 128.0f;

        out_detection.face_detected = true;
        out_detection.score = max_score;
        out_detection.box_x = (cx - w * 0.5f) * input_frame.width;
        out_detection.box_y = (cy - h * 0.5f) * input_frame.height;
        out_detection.box_w = w * input_frame.width;
        out_detection.box_h = h * input_frame.height;

        out_detection.r_eye_x = ((box_data[4] / 128.0f) + anchor.x_center) * input_frame.width;
        out_detection.r_eye_y = ((box_data[5] / 128.0f) + anchor.y_center) * input_frame.height;
        out_detection.l_eye_x = ((box_data[6] / 128.0f) + anchor.x_center) * input_frame.width;
        out_detection.l_eye_y = ((box_data[7] / 128.0f) + anchor.y_center) * input_frame.height;
        out_detection.nose_x = ((box_data[8] / 128.0f) + anchor.x_center) * input_frame.width;
        out_detection.nose_y = ((box_data[9] / 128.0f) + anchor.y_center) * input_frame.height;
        out_detection.mouth_x = ((box_data[10] / 128.0f) + anchor.x_center) * input_frame.width;
        out_detection.mouth_y = ((box_data[11] / 128.0f) + anchor.y_center) * input_frame.height;
        out_detection.r_ear_x = ((box_data[12] / 128.0f) + anchor.x_center) * input_frame.width;
        out_detection.r_ear_y = ((box_data[13] / 128.0f) + anchor.y_center) * input_frame.height;
        out_detection.l_ear_x = ((box_data[14] / 128.0f) + anchor.x_center) * input_frame.width;
        out_detection.l_ear_y = ((box_data[15] / 128.0f) + anchor.y_center) * input_frame.height;

        return true;
    } catch (const std::exception& e) {
        log_error(std::string("BlazeFaceDetector_RunError: ") + e.what());
        out_detection.face_detected = false;
        return false;
    }
}

} // namespace Gaze
