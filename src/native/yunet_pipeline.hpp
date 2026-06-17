// Detects faces via YuNet, normalizes roll via affine warps, crops eyes, and estimates camera space 3D eye coordinates.
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

    // Helper to perform perspective warp / crop on eye landmarks
    bool crop_eye(const cv::Mat& gray, const cv::Point2f landmarks[5], bool is_left, unsigned char out_buffer[2160]);

public:
    YuNetPipeline(const std::string& yunet_model_path, float score_thresh = 0.9f, float nms_thresh = 0.3f, int k = 5000);
    virtual ~YuNetPipeline() = default;

    virtual bool initialize() override;
    virtual bool process_frame(const Frame& frame, EyeCrops& out_crops) override;
};

} // namespace Gaze
