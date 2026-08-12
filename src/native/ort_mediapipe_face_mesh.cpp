#include "ort_mediapipe_face_mesh.hpp"
#include "../core/log.hpp"
#include "../core/pnp_solver.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>

namespace Gaze {

static bool file_exists_native(const std::string& path) {
    std::ifstream f(path.c_str());
    return f.good();
}

MediaPipeFaceMeshPipeline::MediaPipeFaceMeshPipeline(const std::string& model_path, const std::string& detector_path)
    : model_path_(model_path),
      detector_path_(detector_path),
      env(ORT_LOGGING_LEVEL_WARNING, "MediaPipeFaceMeshPipeline"),
      memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
}

MediaPipeFaceMeshPipeline::MediaPipeFaceMeshPipeline(const std::vector<uint8_t>& model_buffer, const std::vector<uint8_t>& detector_buffer)
    : model_buffer_(model_buffer),
      detector_buffer_(detector_buffer),
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

        if (!detector_buffer_.empty()) {
            auto det = std::make_unique<BlazeFaceDetector>(detector_buffer_);
            if (det->initialize()) {
                detector = std::move(det);
            }
        }

        if (!detector) {
            std::vector<std::string> candidate_paths = {
                "addons/godot-gaze/models/mediapipe_face_detector.ort",
                "project/addons/godot-gaze/models/mediapipe_face_detector.ort",
                "../project/addons/godot-gaze/models/mediapipe_face_detector.ort"
            };
            if (!detector_path_.empty()) {
                candidate_paths.insert(candidate_paths.begin(), detector_path_);
            }
            if (!model_path_.empty()) {
                size_t last_slash = model_path_.find_last_of("/\\");
                if (last_slash != std::string::npos) {
                    candidate_paths.insert(candidate_paths.begin(), model_path_.substr(0, last_slash + 1) + "mediapipe_face_detector.ort");
                }
            }

            for (const auto& p : candidate_paths) {
                if (!file_exists_native(p)) {
                    continue;
                }
                auto det = std::make_unique<BlazeFaceDetector>(p);
                if (det->initialize()) {
                    detector = std::move(det);
                    break;
                }
            }
        }
        if (!detector) {
            log_warning("MediaPipeFaceMeshPipeline_DetectorInitFallback: no valid detector found in candidates");
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
    if (!is_initialized || !session || !input_frame.data || input_frame.width <= 0 || input_frame.height <= 0) {
        out_result.face_detected = false;
        return false;
    }

    if (!has_tracking_roi || roi_w <= 0.0f || roi_h <= 0.0f) {
        if (detector) {
            BlazeFaceDetection det;
            if (detector->detect(input_frame, det) && det.face_detected) {
                float pad_w = det.box_w * 0.25f;
                float pad_h = det.box_h * 0.25f;
                roi_x = std::max(0.0f, det.box_x - pad_w);
                roi_y = std::max(0.0f, det.box_y - pad_h);
                roi_w = std::min(input_frame.width - roi_x, det.box_w + 2.0f * pad_w);
                roi_h = std::min(input_frame.height - roi_y, det.box_h + 2.0f * pad_h);
                has_tracking_roi = true;
            } else {
                out_result.face_detected = false;
                has_tracking_roi = false;
                return true;
            }
        } else {
            roi_x = 0.0f;
            roi_y = 0.0f;
            roi_w = static_cast<float>(input_frame.width);
            roi_h = static_cast<float>(input_frame.height);
        }
    }

    const int target_w = 256;
    const int target_h = 256;
    std::vector<float> input_tensor_values(1 * target_h * target_w * 3);

    // Frame data: Crop face ROI from input_frame -> RGB float 256x256
    for (int y = 0; y < target_h; y++) {
        float norm_y = static_cast<float>(y) / target_h;
        int src_y = std::max(0, std::min(input_frame.height - 1, static_cast<int>(roi_y + norm_y * roi_h)));
        for (int x = 0; x < target_w; x++) {
            float norm_x = static_cast<float>(x) / target_w;
            int src_x = std::max(0, std::min(input_frame.width - 1, static_cast<int>(roi_x + norm_x * roi_w)));
            int src_idx = (src_y * input_frame.width + src_x) * 3;

            float b = static_cast<float>(input_frame.data[src_idx + 0]);
            float g = static_cast<float>(input_frame.data[src_idx + 1]);
            float r = static_cast<float>(input_frame.data[src_idx + 2]);

            int dst_spatial = (y * target_w + x) * 3;
            input_tensor_values[dst_spatial + 0] = r / 255.0f;
            input_tensor_values[dst_spatial + 1] = g / 255.0f;
            input_tensor_values[dst_spatial + 2] = b / 255.0f;
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
            has_tracking_roi = false;
            return false;
        }

        if (output_tensors.size() > 1) {
            float presence_score = output_tensors[1].GetTensorData<float>()[0];
            if (presence_score < 0.0f) {
                out_result.face_detected = false;
                has_tracking_roi = false;
                return true;
            }
        }

        const float* lm_raw = output_tensors[0].GetTensorData<float>();
        process_landmarks(lm_raw, input_frame, out_result);
        out_result.roi_x = roi_x;
        out_result.roi_y = roi_y;
        out_result.roi_w = roi_w;
        out_result.roi_h = roi_h;
        out_result.face_detected = true;
        return true;
    } catch (const std::exception& e) {
        log_error(std::string("MediaPipeFaceMeshPipeline_RunError: ") + e.what());
        out_result.face_detected = false;
        has_tracking_roi = false;
        return false;
    }
}

void MediaPipeFaceMeshPipeline::process_landmarks(const float* lm_raw, const Frame& input_frame, MediaPipeFaceMeshResult& out_result) {
    // Map raw 256x256 ROI landmarks to full image pixel space
    std::vector<float> mapped(478 * 3);
    float x_min = 1e9f, x_max = -1e9f, y_min = 1e9f, y_max = -1e9f;

    for (int i = 0; i < 478; i++) {
        float lx = roi_x + (lm_raw[i * 3 + 0] / 256.0f) * roi_w;
        float ly = roi_y + (lm_raw[i * 3 + 1] / 256.0f) * roi_h;
        float lz = lm_raw[i * 3 + 2];

        mapped[i * 3 + 0] = lx;
        mapped[i * 3 + 1] = ly;
        mapped[i * 3 + 2] = lz;

        if (lx < x_min) x_min = lx;
        if (lx > x_max) x_max = lx;
        if (ly < y_min) y_min = ly;
        if (ly > y_max) y_max = ly;
    }

    out_result.landmarks_3d = mapped;

    // Update face ROI for next frame tracking with 25% padding
    float face_w = x_max - x_min;
    float face_h = y_max - y_min;
    if (face_w > 10.0f && face_h > 10.0f) {
        roi_x = std::max(0.0f, x_min - 0.20f * face_w);
        roi_y = std::max(0.0f, y_min - 0.20f * face_h);
        roi_w = std::min(input_frame.width - roi_x, face_w * 1.40f);
        roi_h = std::min(input_frame.height - roi_y, face_h * 1.40f);
        has_tracking_roi = true;
    }

    // MediaPipe Face Mesh indexing:
    // Anatomical Right Eye (image left): 33 (outer), 133 (inner), 159 (top1), 145 (bot1), 158 (top2), 153 (bot2)
    // Anatomical Left Eye (image right): 263 (outer), 362 (inner), 386 (top1), 374 (bot1), 385 (top2), 380 (bot2)
    auto dist_3d = [](const float* p1, const float* p2) {
        float dx = p1[0] - p2[0];
        float dy = p1[1] - p2[1];
        float dz = p1[2] - p2[2];
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    };

    float r_v1 = dist_3d(&lm_raw[159 * 3], &lm_raw[145 * 3]);
    float r_v2 = dist_3d(&lm_raw[158 * 3], &lm_raw[153 * 3]);
    float r_h = dist_3d(&lm_raw[33 * 3], &lm_raw[133 * 3]) + 1e-6f;
    float r_ear = (r_v1 + r_v2) / (2.0f * r_h);

    float l_v1 = dist_3d(&lm_raw[386 * 3], &lm_raw[374 * 3]);
    float l_v2 = dist_3d(&lm_raw[385 * 3], &lm_raw[380 * 3]);
    float l_h = dist_3d(&lm_raw[362 * 3], &lm_raw[263 * 3]) + 1e-6f;
    float l_ear = (l_v1 + l_v2) / (2.0f * l_h);

    // EAR mapped to eye openness [0.0, 1.0] with threshold range [0.20, 0.30]
    out_result.right_eye_openness = std::max(0.0f, std::min(1.0f, (r_ear - 0.20f) / 0.10f));
    out_result.left_eye_openness = std::max(0.0f, std::min(1.0f, (l_ear - 0.20f) / 0.10f));

    // Eye crop sampling from mapped full-image pixel coordinates
    auto crop_eye = [&](int center_idx, uint8_t* out_crop) {
        int cx = static_cast<int>(mapped[center_idx * 3 + 0]);
        int cy = static_cast<int>(mapped[center_idx * 3 + 1]);

        for (int y = 0; y < 60; y++) {
            int src_y = std::max(0, std::min(input_frame.height - 1, cy - 30 + y));
            for (int x = 0; x < 60; x++) {
                int src_x = std::max(0, std::min(input_frame.width - 1, cx - 30 + x));
                int src_idx = (src_y * input_frame.width + src_x) * 3;
                int dst_idx = (y * 60 + x) * 3;

                out_crop[dst_idx + 0] = input_frame.data[src_idx + 0];
                out_crop[dst_idx + 1] = input_frame.data[src_idx + 1];
                out_crop[dst_idx + 2] = input_frame.data[src_idx + 2];
            }
        }
    };

    // Landmark 468 (Anatomical Right Eye / Viewer Left X=113) -> right_eye_crop
    // Landmark 473 (Anatomical Left Eye / Viewer Right X=152) -> left_eye_crop
    crop_eye(468 < 478 ? 468 : 33, out_result.right_eye_crop);
    crop_eye(473 < 478 ? 473 : 362, out_result.left_eye_crop);

    double w = input_frame.width > 0 ? (double)input_frame.width : 640.0;
    double h = input_frame.height > 0 ? (double)input_frame.height : 480.0;
    double focal = w * 1.5;
    double cx = w / 2.0;
    double cy = h / 2.0;

    int r_idx = 473 < 478 ? 473 : 33;
    int l_idx = 468 < 478 ? 468 : 362;
    float r_x = mapped[r_idx * 3 + 0];
    float r_y = mapped[r_idx * 3 + 1];
    float l_x = mapped[l_idx * 3 + 0];
    float l_y = mapped[l_idx * 3 + 1];
    float n_x = mapped[1 * 3 + 0];
    float n_y = mapped[1 * 3 + 1];

    double eye_dist_px = std::sqrt((r_x - l_x)*(r_x - l_x) + (r_y - l_y)*(r_y - l_y)) + 1e-6;
    double z_mm = (focal * 63.0) / eye_dist_px;
    if (z_mm < 300.0) z_mm = 300.0;
    if (z_mm > 1200.0) z_mm = 1200.0;

    double nose_cx = (n_x - cx) / focal * z_mm;
    double nose_cy = (n_y - cy) / focal * z_mm;

    out_result.head_pose.trans_x_mm = static_cast<float>(nose_cx);
    out_result.head_pose.trans_y_mm = static_cast<float>(nose_cy);
    out_result.head_pose.trans_z_mm = static_cast<float>(z_mm);

    float yaw = std::atan2(n_x - cx, focal);
    float pitch = std::atan2(n_y - cy, focal);
    float roll = std::atan2(r_y - l_y, r_x - l_x);

    out_result.head_pose.pitch_rad = pitch;
    out_result.head_pose.yaw_rad = yaw;
    out_result.head_pose.roll_rad = roll;
    out_result.head_pose.trans_x_mm = nose_cx;
    out_result.head_pose.trans_y_mm = nose_cy;
    out_result.head_pose.trans_z_mm = z_mm;
}

} // namespace Gaze
