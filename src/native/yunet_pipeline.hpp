/**
 * @file yunet_pipeline.hpp
 * @brief Face Landmarking and Normalization Pipeline (Layer 2 - Native)
 *
 * Implements FacePipeline using OpenCV's FaceDetectorYN (YuNet) face landmarker.
 * Resolves 5 facial landmarks, applies perspective affine warps to neutralize face
 * roll/scale rotation, extracts eye crops, and estimates 3D camera coordinates via solvePnP.
 */
#pragma once

#include "face_pipeline.hpp"
#include <opencv2/objdetect.hpp>
#include <string>

namespace Gaze {

class YuNetPipeline : public FacePipeline {
private:
    std::string model_path;
    cv::Ptr<cv::FaceDetectorYN> detector;
    float score_threshold;
    float nms_threshold;
    int top_k;

    double camera_focal_length_px = -1.0;
    PipelineConfig config;

    // Helper to perform perspective warp / crop on eye landmarks
    bool crop_eye(const cv::Mat& gray, const cv::Point2f landmarks[5], bool is_left, unsigned char out_buffer[10800]);

public:
    YuNetPipeline(const std::string& yunet_model_path, float score_thresh = 0.6f, float nms_thresh = 0.3f, int k = 5000);
    virtual ~YuNetPipeline() = default;

    virtual bool initialize() override;
    virtual bool process_frame(const Frame& frame, EyeCrops& out_crops) override;
    virtual void set_camera_focal_length_px(double f) override;
    virtual void set_config(const PipelineConfig& cfg) override { config = cfg; }
};

} // namespace Gaze
