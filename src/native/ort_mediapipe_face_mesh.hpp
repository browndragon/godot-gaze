#ifndef ORT_MEDIAPIPE_FACE_MESH_HPP
#define ORT_MEDIAPIPE_FACE_MESH_HPP

#include "ort_yunet_detector.hpp"
#include "../core/gaze_frame_data.hpp"
#include "../core/camera_interface.hpp"
#include "../core/pipeline_config.hpp"
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <string>
#include <vector>

namespace Gaze {



struct MediaPipeFaceMeshResult {
    bool face_detected = false;
    float roi_x = 0.0f;
    float roi_y = 0.0f;
    float roi_w = 0.0f;
    float roi_h = 0.0f;
    std::vector<float> landmarks_3d; // 478 * 3 floats
    float left_eye_openness = 1.0f;
    float right_eye_openness = 1.0f;
    HeadPose head_pose;
    uint8_t left_eye_crop[60 * 60 * 3];
    uint8_t right_eye_crop[60 * 60 * 3];
};

class MediaPipeFaceMeshPipeline {
public:
    MediaPipeFaceMeshPipeline(const std::string& model_path, const std::string& detector_path = "");
    MediaPipeFaceMeshPipeline(const std::vector<uint8_t>& model_buffer, const std::vector<uint8_t>& detector_buffer = {});
    ~MediaPipeFaceMeshPipeline();

    bool initialize();
    bool process_frame(const Frame& input_frame, MediaPipeFaceMeshResult& out_result);
    void set_config(const PipelineConfig& config) { active_config = config; }

private:
    std::string model_path_;
    std::string detector_path_;
    std::vector<uint8_t> model_buffer_;
    std::vector<uint8_t> detector_buffer_;
    Ort::Env env;
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;
    Ort::MemoryInfo memory_info;
    PipelineConfig active_config;
    bool is_initialized = false;
    bool has_tracking_roi = false;
    float roi_x = 0.0f;
    float roi_y = 0.0f;
    float roi_w = 0.0f;
    float roi_h = 0.0f;

    std::unique_ptr<ORTYuNetDetector> detector;

    void process_landmarks(const float* landmarks_raw, const Frame& input_frame, MediaPipeFaceMeshResult& out_result);
};

} // namespace Gaze

#endif // ORT_MEDIAPIPE_FACE_MESH_HPP
