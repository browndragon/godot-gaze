#ifndef ORT_MEDIAPIPE_FACE_MESH_HPP
#define ORT_MEDIAPIPE_FACE_MESH_HPP

#include "../core/gaze_frame_data.hpp"
#include "../core/camera_interface.hpp"
#include "../core/pipeline_config.hpp"
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <string>
#include <vector>

namespace Gaze {

struct HeadPose {
    float pitch_rad = 0.0f;
    float yaw_rad = 0.0f;
    float roll_rad = 0.0f;
    float trans_x_mm = 0.0f;
    float trans_y_mm = 0.0f;
    float trans_z_mm = 600.0f;

    GazeVector3 translation() const { return GazeVector3(trans_x_mm, trans_y_mm, trans_z_mm); }
    GazeVector3 rotation_matrix() const { return GazeVector3(pitch_rad, yaw_rad, roll_rad); }
};

struct MediaPipeFaceMeshResult {
    bool face_detected = false;
    std::vector<float> landmarks_3d; // 478 * 3 floats
    float left_eye_openness = 1.0f;
    float right_eye_openness = 1.0f;
    HeadPose head_pose;
    uint8_t left_eye_crop[60 * 60 * 3];
    uint8_t right_eye_crop[60 * 60 * 3];
};

class MediaPipeFaceMeshPipeline {
public:
    MediaPipeFaceMeshPipeline(const std::string& model_path);
    MediaPipeFaceMeshPipeline(const std::vector<uint8_t>& model_buffer);
    ~MediaPipeFaceMeshPipeline();

    bool initialize();
    bool process_frame(const Frame& input_frame, MediaPipeFaceMeshResult& out_result);
    void set_config(const PipelineConfig& config) { active_config = config; }

private:
    std::string model_path_;
    std::vector<uint8_t> model_buffer_;
    Ort::Env env;
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;
    Ort::MemoryInfo memory_info;
    PipelineConfig active_config;
    bool is_initialized = false;

    void process_landmarks(const float* landmarks_raw, const Frame& input_frame, MediaPipeFaceMeshResult& out_result);
};

} // namespace Gaze

#endif // ORT_MEDIAPIPE_FACE_MESH_HPP
