#include "yunet_pipeline.hpp"
#include "log.hpp"
#include "face_model_geometry.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>

namespace Gaze {

YuNetPipeline::YuNetPipeline(const std::string& yunet_model_path, float score_thresh, float nms_thresh, int k)
    : model_path(yunet_model_path), score_threshold(score_thresh), nms_threshold(nms_thresh), top_k(k) {}

bool YuNetPipeline::initialize() {
    log_info("YuNetPipelineInitAttempt", "model_path", model_path);
    try {
        // YuNet expects size configuration input during creation; we use a standard 320x320 initial estimate
        detector = cv::FaceDetectorYN::create(
            model_path, 
            "", 
            cv::Size(320, 320), 
            score_threshold, 
            nms_threshold, 
            top_k
        );
        if (detector.empty()) {
            log_error("YuNetPipelineInitFailed", "reason", "detector creation returned null");
            return false;
        }
    } catch (const std::exception& e) {
        log_error("YuNetPipelineInitException", "what", e.what());
        return false;
    }
    log_info("YuNetPipelineInitSuccess");
    return true;
}

bool YuNetPipeline::process_frame(const Frame& frame, EyeCrops& out_crops) {
    if (detector.empty() || frame.data == nullptr) {
        return false;
    }

    // Wrap the raw frame buffer into an OpenCV Mat (BGR CV_8UC3)
    cv::Mat bgr(frame.height, frame.width, CV_8UC3, const_cast<unsigned char*>(frame.data));

    // Update detector input size if frame dimensions changed
    detector->setInputSize(bgr.size());

    cv::Mat faces;
    detector->detect(bgr, faces);

    if (faces.empty() || faces.rows == 0) {
        out_crops.face_detected = false;
        return false;
    }

    out_crops.face_detected = true;

    // Use the first detected face (highest confidence score)
    // Row layout: [x, y, w, h, right_eye_x, right_eye_y, left_eye_x, left_eye_y, ...]
    // Note: OpenCV YuNet labels left/right from face perspective.
    float x = faces.at<float>(0, 0);
    float y = faces.at<float>(0, 1);
    float w = faces.at<float>(0, 2);
    float h = faces.at<float>(0, 3);

    // Extract facial landmarks (5 points)
    cv::Point2f landmarks[5];
    for (int i = 0; i < 5; ++i) {
        landmarks[i].x = faces.at<float>(0, 4 + 2 * i);
        landmarks[i].y = faces.at<float>(0, 5 + 2 * i);
    }

    // In YuNet output:
    // landmarks[0] = right eye center (image left side)
    // landmarks[1] = left eye center (image right side)
    cv::Point2f right_eye_img = landmarks[0];
    cv::Point2f left_eye_img = landmarks[1];

    // Convert BGR frame to grayscale for eye crops extraction
    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);

    // Crop and warp eyes (60x36 px)
    crop_eye(gray, landmarks, true, out_crops.left_eye_data);  // Left Eye crop
    crop_eye(gray, landmarks, false, out_crops.right_eye_data); // Right Eye crop

    // Define a standard 3D facial model for PnP (eyes, nose, mouth corners)
    std::vector<cv::Point3f> model_points = {
        cv::Point3f(static_cast<float>(-FaceModelGeometry::EYE_X), static_cast<float>(FaceModelGeometry::EYE_Y), static_cast<float>(FaceModelGeometry::EYE_Z)), // Right eye
        cv::Point3f(static_cast<float>(FaceModelGeometry::EYE_X), static_cast<float>(FaceModelGeometry::EYE_Y), static_cast<float>(FaceModelGeometry::EYE_Z)),  // Left eye
        cv::Point3f(0.0f, static_cast<float>(config.nose_y), static_cast<float>(config.nose_z)),   // Nose tip
        cv::Point3f(static_cast<float>(-FaceModelGeometry::MOUTH_X), static_cast<float>(FaceModelGeometry::MOUTH_Y), static_cast<float>(FaceModelGeometry::MOUTH_Z)), // Right mouth corner
        cv::Point3f(static_cast<float>(FaceModelGeometry::MOUTH_X), static_cast<float>(FaceModelGeometry::MOUTH_Y), static_cast<float>(FaceModelGeometry::MOUTH_Z))  // Left mouth corner
    };

    std::vector<cv::Point2f> image_points = {
        right_eye_img,
        left_eye_img,
        landmarks[2], // Nose tip
        landmarks[3], // Mouth right corner
        landmarks[4]  // Mouth left corner
    };

    // Camera matrix approximation
    double cx = frame.width / 2.0;
    double cy = frame.height / 2.0;
    double fx = (camera_focal_length_px > 0.0) ? camera_focal_length_px : (frame.width * 1.5625);
    cv::Mat camera_matrix = (cv::Mat_<double>(3, 3) << 
        fx,  0.0, cx,
        0.0, fx,  cy,
        0.0, 0.0, 1.0
    );
    cv::Mat dist_coeffs = cv::Mat::zeros(4, 1, CV_64F);

    cv::Mat rvec = (cv::Mat_<double>(3, 1) << 0.0, 0.0, 0.0);
    cv::Mat tvec = (cv::Mat_<double>(3, 1) << 0.0, 0.0, 700.0);
    bool pnp_success = false;
    try {
        pnp_success = cv::solvePnP(model_points, image_points, camera_matrix, dist_coeffs, rvec, tvec, true, cv::SOLVEPNP_ITERATIVE);
    } catch (const std::exception& e) {
        log_error("YuNetPipelinePnPException", "what", e.what());
    }

    if (pnp_success) {
        // Use tvec + model points to find eye centers in camera coordinates (mm)
        cv::Mat R;
        cv::Rodrigues(rvec, R);

        // Output head pose rotation as the raw Rodrigues rotation vector
        out_crops.head_pose_rotation = GazeVector3(rvec.at<double>(0), rvec.at<double>(1), rvec.at<double>(2));
        out_crops.head_pose_translation = GazeVector3(tvec.at<double>(0), tvec.at<double>(1), tvec.at<double>(2));
        
        cv::Mat left_eye_cam_mat = R * (cv::Mat_<double>(3, 1) << FaceModelGeometry::EYE_X, FaceModelGeometry::EYE_Y, FaceModelGeometry::EYE_Z) + tvec;
        cv::Mat right_eye_cam_mat = R * (cv::Mat_<double>(3, 1) << -FaceModelGeometry::EYE_X, FaceModelGeometry::EYE_Y, FaceModelGeometry::EYE_Z) + tvec;

        out_crops.left_eye_center_cam = GazeVector3(left_eye_cam_mat.at<double>(0), left_eye_cam_mat.at<double>(1), left_eye_cam_mat.at<double>(2));
        out_crops.right_eye_center_cam = GazeVector3(right_eye_cam_mat.at<double>(0), right_eye_cam_mat.at<double>(1), right_eye_cam_mat.at<double>(2));
    } else {
        // Fallback: estimate camera coordinate positions via basic depth triangulation
        double dx = left_eye_img.x - right_eye_img.x;
        double dy = left_eye_img.y - right_eye_img.y;
        double dist_px = std::sqrt(dx * dx + dy * dy);
        double depth_z = (config.ipd_mm * fx) / dist_px;

        // Midpoint of eyes in pixels
        double mid_x = (left_eye_img.x + right_eye_img.x) / 2.0;
        double mid_y = (left_eye_img.y + right_eye_img.y) / 2.0;

        double mid_cam_x = (mid_x - cx) * depth_z / fx;
        double mid_cam_y = (mid_y - cy) * depth_z / fx;

        GazeVector3 mid_cam(mid_cam_x, mid_cam_y, depth_z);
        double half_ipd = config.ipd_mm * 0.5;
        out_crops.left_eye_center_cam = mid_cam + GazeVector3(half_ipd, 0.0, 0.0);
        out_crops.right_eye_center_cam = mid_cam - GazeVector3(half_ipd, 0.0, 0.0);
        out_crops.head_pose_rotation = GazeVector3(0.0, 0.0, 0.0);
        out_crops.head_pose_translation = mid_cam;
    }

    return true;
}

bool YuNetPipeline::crop_eye(const cv::Mat& gray, const cv::Point2f landmarks[5], bool is_left, unsigned char out_buffer[10800]) {
    // Select primary eye landmark (landmarks[1] is left eye, landmarks[0] is right eye)
    cv::Point2f eye_center = is_left ? landmarks[1] : landmarks[0];

    // Compute face roll angle from the eye-to-eye vector (right eye landmarks[0] to left eye landmarks[1])
    double roll_dx = landmarks[1].x - landmarks[0].x;
    double roll_dy = landmarks[1].y - landmarks[0].y;
    double angle = std::atan2(roll_dy, roll_dx) * (180.0 / 3.141592653589793);

    // Calculate inter-pupillary distance in pixels
    double dist_px = std::sqrt(roll_dx * roll_dx + roll_dy * roll_dy);
    
    // Normalize eye crop scale based on eye distance in pixels to handle different resolutions/distances
    double scale = 70.0 / (dist_px > 1e-6 ? dist_px : 70.0);

    // Define eye crop target dimensions (Intel ADAS model expects 60x60)
    cv::Size target_size(60, 60);

    // Create rotation matrix around eye center to neutralize roll and normalize scale
    cv::Mat M = cv::getRotationMatrix2D(eye_center, angle, scale);

    // Adjust transformation translation to center the eye crop
    M.at<double>(0, 2) += (target_size.width / 2.0) - eye_center.x;
    M.at<double>(1, 2) += (target_size.height / 2.0) - eye_center.y;

    // Apply affine warp to crop and rotate eye crop
    cv::Mat warped;
    cv::warpAffine(gray, warped, M, target_size, cv::INTER_LINEAR, cv::BORDER_REPLICATE);

    // Convert grayscale eye crop to 3-channel BGR expected by the ONNX model
    cv::Mat warped_bgr;
    cv::cvtColor(warped, warped_bgr, cv::COLOR_GRAY2BGR);

    // Copy to flat output buffer (10800 bytes)
    std::memcpy(out_buffer, warped_bgr.data, 10800);
    return true;
}

void YuNetPipeline::set_camera_focal_length_px(double f) {
    camera_focal_length_px = f;
}

} // namespace Gaze
