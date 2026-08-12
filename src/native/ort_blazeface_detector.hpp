#ifndef ORT_BLAZEFACE_DETECTOR_HPP
#define ORT_BLAZEFACE_DETECTOR_HPP

#include "../core/camera_interface.hpp"
#include "../core/pipeline_config.hpp"
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <string>
#include <vector>

namespace Gaze {

struct BlazeFaceDetection {
    bool face_detected = false;
    float score = 0.0f;
    float box_x = 0.0f;
    float box_y = 0.0f;
    float box_w = 0.0f;
    float box_h = 0.0f;

    float r_eye_x = 0.0f;
    float r_eye_y = 0.0f;
    float l_eye_x = 0.0f;
    float l_eye_y = 0.0f;
    float nose_x = 0.0f;
    float nose_y = 0.0f;
    float mouth_x = 0.0f;
    float mouth_y = 0.0f;
    float r_ear_x = 0.0f;
    float r_ear_y = 0.0f;
    float l_ear_x = 0.0f;
    float l_ear_y = 0.0f;
};

class BlazeFaceDetector {
public:
    BlazeFaceDetector(const std::string& model_path);
    BlazeFaceDetector(const std::vector<uint8_t>& model_buffer);
    ~BlazeFaceDetector();

    bool initialize();
    bool detect(const Frame& input_frame, BlazeFaceDetection& out_detection);

private:
    std::string model_path_;
    std::vector<uint8_t> model_buffer_;
    bool load_from_buffer = false;

    Ort::Env env;
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;
    Ort::MemoryInfo memory_info;
    bool is_initialized = false;

    struct Anchor {
        float x_center;
        float y_center;
        float w;
        float h;
    };
    std::vector<Anchor> anchors;

    void generate_anchors();
};

} // namespace Gaze

#endif // ORT_BLAZEFACE_DETECTOR_HPP
