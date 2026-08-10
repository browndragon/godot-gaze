#include "ort_mediapipe_face_mesh.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace Gaze {

static void log_info(const std::string& msg) {
    std::cout << "[INFO] " << msg << std::endl;
}

static void log_error(const std::string& msg) {
    std::cerr << "[ERROR] " << msg << std::endl;
}

MediaPipeFaceMeshPipeline::MediaPipeFaceMeshPipeline(const std::string& model_path)
    : model_path_(model_path),
      env(ORT_LOGGING_LEVEL_WARNING, "MediaPipeFaceMeshPipeline"),
      memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
}

MediaPipeFaceMeshPipeline::MediaPipeFaceMeshPipeline(const std::vector<uint8_t>& model_buffer)
    : model_buffer_(model_buffer),
      env(ORT_LOGGING_LEVEL_WARNING, "MediaPipeFaceMeshPipeline"),
      memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
}

MediaPipeFaceMeshPipeline::~MediaPipeFaceMeshPipeline() {}

bool MediaPipeFaceMeshPipeline::initialize() {
    try {
        if (!model_buffer_.empty()) {
            log_info("MediaPipeFaceMeshPipeline_InitBuffer size=" + std::to_string(model_buffer_.size()));
            session = std::make_unique<Ort::Session>(
                env,
                model_buffer_.data(),
                model_buffer_.size(),
                session_options
            );
        } else if (!model_path_.empty()) {
            log_info("MediaPipeFaceMeshPipeline_InitPath path=" + model_path_);
            session = std::make_unique<Ort::Session>(
                env,
                model_path_.c_str(),
                session_options
            );
        } else {
            log_error("MediaPipeFaceMeshPipeline_InitFailedNoSource");
            return false;
        }
        is_initialized = true;
        log_info("MediaPipeFaceMeshPipeline_InitSuccess");
        return true;
    } catch (const std::exception& e) {
        log_error(std::string("MediaPipeFaceMeshPipeline_InitFailed: ") + e.what());
        return false;
    }
}

bool MediaPipeFaceMeshPipeline::process_frame(const Frame& input_frame, MediaPipeFaceMeshResult& out_result) {
    if (!is_initialized || !session || !input_frame.data) {
        out_result.face_detected = false;
        return false;
    }

    const int target_w = 256;
    const int target_h = 256;
    std::vector<float> input_tensor_values(1 * target_h * target_w * 3);

    // Frame data: BGR 24-bit per pixel
    for (int y = 0; y < target_h; y++) {
        int src_y = y * input_frame.height / target_h;
        for (int x = 0; x < target_w; x++) {
            int src_x = x * input_frame.width / target_w;
            int src_idx = (src_y * input_frame.width + src_x) * 3;

            float b = static_cast<float>(input_frame.data[src_idx + 0]);
            float g = static_cast<float>(input_frame.data[src_idx + 1]);
            float r = static_cast<float>(input_frame.data[src_idx + 2]);

            int dst_spatial = (y * target_w + x) * 3;
            input_tensor_values[dst_spatial + 0] = r;
            input_tensor_values[dst_spatial + 1] = g;
            input_tensor_values[dst_spatial + 2] = b;
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

        if (output_tensors.empty()) {
            out_result.face_detected = false;
            return false;
        }

        const float* lm_raw = output_tensors[0].GetTensorData<float>();
        float presence = (output_tensors.size() > 1) ? output_tensors[1].GetTensorData<float>()[0] : 1.0f;

        if (presence < 0.0f && presence != 0.0f) {
            out_result.face_detected = false;
            return false;
        }

        process_landmarks(lm_raw, input_frame, out_result);
        out_result.face_detected = true;
        return true;
    } catch (const std::exception& e) {
        log_error(std::string("MediaPipeFaceMeshPipeline_RunError: ") + e.what());
        out_result.face_detected = false;
        return false;
    }
}

void MediaPipeFaceMeshPipeline::process_landmarks(const float* lm_raw, const Frame& input_frame, MediaPipeFaceMeshResult& out_result) {
    out_result.landmarks_3d.assign(lm_raw, lm_raw + 478 * 3);

    // Eyelid landmark positions for EAR calculation
    // Left eye: 159 (top), 145 (bot), 33 (left corner), 133 (right corner)
    // Right eye: 386 (top), 374 (bot), 362 (left corner), 263 (right corner)
    auto dist_3d = [](const float* p1, const float* p2) {
        float dx = p1[0] - p2[0];
        float dy = p1[1] - p2[1];
        float dz = p1[2] - p2[2];
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    };

    float l_v = dist_3d(&lm_raw[159 * 3], &lm_raw[145 * 3]);
    float l_h = dist_3d(&lm_raw[33 * 3], &lm_raw[133 * 3]) + 1e-6f;
    float l_ear = l_v / l_h;

    float r_v = dist_3d(&lm_raw[386 * 3], &lm_raw[374 * 3]);
    float r_h = dist_3d(&lm_raw[362 * 3], &lm_raw[263 * 3]) + 1e-6f;
    float r_ear = r_v / r_h;

    out_result.left_eye_openness = std::max(0.0f, std::min(1.0f, (l_ear - 0.15f) / 0.15f));
    out_result.right_eye_openness = std::max(0.0f, std::min(1.0f, (r_ear - 0.15f) / 0.15f));

    float yaw = std::atan2(lm_raw[1 * 3 + 0], -lm_raw[1 * 3 + 2]);
    float pitch = std::atan2(lm_raw[1 * 3 + 1], -lm_raw[1 * 3 + 2]);
    float roll = std::atan2(lm_raw[263 * 3 + 1] - lm_raw[33 * 3 + 1], lm_raw[263 * 3 + 0] - lm_raw[33 * 3 + 0]);

    out_result.head_pose.pitch_rad = pitch;
    out_result.head_pose.yaw_rad = yaw;
    out_result.head_pose.roll_rad = roll;
    out_result.head_pose.trans_x_mm = lm_raw[1 * 3 + 0];
    out_result.head_pose.trans_y_mm = lm_raw[1 * 3 + 1];
    out_result.head_pose.trans_z_mm = std::abs(lm_raw[1 * 3 + 2]) > 1.0f ? std::abs(lm_raw[1 * 3 + 2]) : 700.0f;

    auto crop_eye = [&](int center_idx, uint8_t* out_crop) {
        float cx_norm = (lm_raw[center_idx * 3 + 0] + 128.0f) / 256.0f;
        float cy_norm = (lm_raw[center_idx * 3 + 1] + 128.0f) / 256.0f;

        int cx = static_cast<int>(cx_norm * input_frame.width);
        int cy = static_cast<int>(cy_norm * input_frame.height);

        for (int y = 0; y < 60; y++) {
            int src_y = std::max(0, std::min(input_frame.height - 1, cy - 30 + y));
            for (int x = 0; x < 60; x++) {
                int src_x = std::max(0, std::min(input_frame.width - 1, cx - 30 + x));
                int src_idx = (src_y * input_frame.width + src_x) * 3;
                int dst_idx = (y * 60 + x) * 3;

                out_crop[dst_idx + 0] = input_frame.data[src_idx + 2];
                out_crop[dst_idx + 1] = input_frame.data[src_idx + 1];
                out_crop[dst_idx + 2] = input_frame.data[src_idx + 0];
            }
        }
    };

    crop_eye(33, out_result.left_eye_crop);
    crop_eye(263, out_result.right_eye_crop);
}

} // namespace Gaze
