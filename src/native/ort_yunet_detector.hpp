/**
 * @file ort_yunet_detector.hpp
 * @brief Native ONNX Runtime YuNet 5-Keypoint Face Detector
 */
#pragma once

#include "../core/camera_interface.hpp"
#include "../core/pipeline_config.hpp"
#include "../core/math_defs.hpp"
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <vector>
#include <string>

namespace Gaze
{
    struct YuNetResult
    {
        bool face_detected = false;
        float score = 0.0f;

        float roi_x = 0.0f;
        float roi_y = 0.0f;
        float roi_w = 0.0f;
        float roi_h = 0.0f;

        GazeVector2 right_eye_px;
        GazeVector2 left_eye_px;
        GazeVector2 nose_tip_px;
        GazeVector2 mouth_right_px;
        GazeVector2 mouth_left_px;

        HeadPose head_pose;
    };

    class ORTYuNetDetector
    {
    public:
        struct Anchor
        {
            float cx;
            float cy;
            float stride_x;
            float stride_y;
        };

        int input_width = 640;
        int input_height = 640;
        bool is_nhwc = false;

    private:
        std::string model_path;
        std::vector<uint8_t> model_buffer;
        bool load_from_buffer = false;
        float score_threshold = 0.25f;
        float nms_threshold = 0.3f;

        std::unique_ptr<Ort::Session> session;
        Ort::MemoryInfo memory_info;

        std::vector<const char *> input_names = {"input"};
        std::vector<const char *> output_names = {
            "cls_8", "cls_16", "cls_32",
            "obj_8", "obj_16", "obj_32",
            "bbox_8", "bbox_16", "bbox_32",
            "kps_8", "kps_16", "kps_32"
        };

        PipelineConfig config;

        std::vector<Anchor> generate_anchors(int width, int height);

    public:
        ORTYuNetDetector(const std::string &p_model_path, float score_thresh = 0.25f, float nms_thresh = 0.3f);
        ORTYuNetDetector(const std::vector<uint8_t> &p_model_buffer, float score_thresh = 0.25f, float nms_thresh = 0.3f);
        ~ORTYuNetDetector() = default;

        bool initialize();
        void set_config(const PipelineConfig &p_config);

        bool process_frame_single_pass(const Frame &frame, YuNetResult &out_result, float roll_deg);
        bool process_frame(const Frame &frame, YuNetResult &out_result, float roll_hint_rad = 0.0f);

        std::vector<GazeVector3> get_canonical_godot_model_points() const;
    };

} // namespace Gaze
