// TODO: Document me
#pragma once

#include "face_pipeline.hpp"
#include <onnxruntime_cxx_api.h>
#include "math_defs.hpp"
#include <string>
#include <vector>
#include <memory>

namespace Gaze
{

    class ORTYuNetPipeline : public FacePipeline
    {
    private:
        std::string model_path;
        std::vector<uint8_t> model_buffer;
        bool load_from_buffer = false;

        std::unique_ptr<Ort::Session> session;
        float score_threshold;
        float nms_threshold;
        int top_k;

        double camera_focal_length_px = -1.0;
        double camera_fov_degrees = DEFAULT_CAMERA_FOV_DEGREES;
        PipelineConfig config;

        double last_roll_rad = 0.0;
        bool has_last_roll = false;

        struct Anchor
        {
            float cx, cy;
            float stride_x, stride_y;
        };

        // Inputs/Outputs
        const std::vector<const char *> input_names = {"input"};
        const std::vector<const char *> output_names = {
            "cls_8", "cls_16", "cls_32",
            "obj_8", "obj_16", "obj_32",
            "bbox_8", "bbox_16", "bbox_32",
            "kps_8", "kps_16", "kps_32"}; // YuNet 12 outputs

        std::vector<Anchor> generate_anchors(int width, int height);
        bool crop_eye(const Frame &frame, const GazePoint landmarks[5], bool is_left, uint8_t out_buffer[EyeCrops::EYE_CROP_SIZE]);
        void rotate_image_bgr(const unsigned char *src, int w, int h, unsigned char *dst, double angle_rad);

    public:
        static constexpr int INFERENCE_WIDTH = 640;
        static constexpr int INFERENCE_HEIGHT = 640;

        ORTYuNetPipeline(const std::string &yunet_model_path, float score_thresh = 0.3f, float nms_thresh = 0.3f, int k = 5000);
        ORTYuNetPipeline(const std::vector<uint8_t> &buffer, float score_thresh = 0.3f, float nms_thresh = 0.3f, int k = 5000);
        virtual ~ORTYuNetPipeline() = default;

        virtual bool initialize() override;
        virtual bool process_frame(const Frame &frame, EyeCrops &out_crops) override;
        virtual void set_camera_focal_length_px(double f) override;
        virtual void set_camera_fov_degrees(double fov) override { camera_fov_degrees = fov; }
        virtual void set_config(const PipelineConfig &cfg) override { config = cfg; }

        void set_roll_hint(double angle_rad) { last_roll_rad = angle_rad; has_last_roll = true; }
        void reset_tracking_state() { last_roll_rad = 0.0; has_last_roll = false; }
    };

} // namespace Gaze
