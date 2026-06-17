// src/native/opencv_gaze_model.cpp
#include "opencv_gaze_model.hpp"
#include "log.hpp"

namespace Gaze {

OpenCVGazeModel::OpenCVGazeModel(const std::string& gaze_onnx_path) : model_path(gaze_onnx_path) {}

bool OpenCVGazeModel::initialize() {
    log_info("OpenCVGazeModelInitAttempt", "model_path", model_path);
    try {
        net = cv::dnn::readNetFromONNX(model_path);
        if (net.empty()) {
            log_error("OpenCVGazeModelInitFailed", "reason", "net reading returned empty");
            return false;
        }
        
        // Optimizations for inference speed
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU); // Can target CUDA/OpenCL depending on platform

    } catch (const std::exception& e) {
        log_error("OpenCVGazeModelInitException", "what", e.what());
        return false;
    }
    log_info("OpenCVGazeModelInitSuccess");
    return true;
}

bool OpenCVGazeModel::estimate_raw_gaze(const EyeCrops& crops, GazeVector3& out_raw_gaze_dir) {
    if (net.empty()) {
        return false;
    }

    // 1. Convert eye crop arrays to OpenCV Mats
    cv::Mat left_eye_mat(36, 60, CV_8UC1, const_cast<unsigned char*>(crops.left_eye_data));
    cv::Mat right_eye_mat(36, 60, CV_8UC1, const_cast<unsigned char*>(crops.right_eye_data));

    // 2. Normalize and format images into DNN input blobs
    // Scale pixel values to [0.0, 1.0]
    cv::Mat left_blob = cv::dnn::blobFromImage(left_eye_mat, 1.0 / 255.0, cv::Size(60, 36), cv::Scalar(0), false);
    cv::Mat right_blob = cv::dnn::blobFromImage(right_eye_mat, 1.0 / 255.0, cv::Size(60, 36), cv::Scalar(0), false);

    // 3. Format head pose features (only pitch & yaw are typically required by CNN)
    float head_pose_data[2] = {
        static_cast<float>(crops.head_pose_rotation.x), // Pitch
        static_cast<float>(crops.head_pose_rotation.y)  // Yaw
    };
    cv::Mat head_pose_blob(1, 2, CV_32F, head_pose_data);

    // 4. Set inputs into their corresponding ONNX tensor nodes
    net.setInput(left_blob, "left_eye_input");
    net.setInput(right_blob, "right_eye_input");
    net.setInput(head_pose_blob, "head_pose_input");

    // 5. Run forward pass
    cv::Mat output = net.forward("gaze_output");

    if (output.empty() || output.cols < 2) {
        log_error("OpenCVGazeModelForwardFailed");
        return false;
    }

    // 6. Parse output format:
    // Case A: Model outputs 2D spherical angles (Pitch, Yaw)
    if (output.cols == 2) {
        double pitch = output.at<float>(0, 0);
        double yaw = output.at<float>(0, 1);
        
        double cos_pitch = std::cos(pitch);
        out_raw_gaze_dir = GazeVector3(
            std::sin(yaw) * cos_pitch,
            std::sin(pitch),
            std::cos(yaw) * cos_pitch
        ).normalized();
    }
    // Case B: Model outputs direct 3D gaze vector (X, Y, Z) in camera space
    else if (output.cols == 3) {
        out_raw_gaze_dir = GazeVector3(
            output.at<float>(0, 0),
            output.at<float>(0, 1),
            output.at<float>(0, 2)
        ).normalized();
    } else {
        return false;
    }

    return true;
}

} // namespace Gaze
