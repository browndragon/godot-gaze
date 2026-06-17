#include "opencv_gaze_model.hpp"
#include "log.hpp"

namespace Gaze {

OpenCVGazeModel::OpenCVGazeModel(const std::string& gaze_onnx_path) : model_path(gaze_onnx_path) {}

bool OpenCVGazeModel::initialize() {
    log_info("OpenCVGazeModelInitAttempt", "model_path", model_path);
    try {
        if (model_path.rfind(".xml") != std::string::npos) {
            std::string bin_path = model_path;
            size_t ext_pos = bin_path.rfind(".xml");
            if (ext_pos != std::string::npos) {
                bin_path.replace(ext_pos, 4, ".bin");
            }
            log_info("OpenCVGazeModelLoadOpenVINO", "xml", model_path, "bin", bin_path);
            net = cv::dnn::readNet(model_path, bin_path);
        } else {
            net = cv::dnn::readNetFromONNX(model_path);
        }
        
        if (net.empty()) {
            log_error("OpenCVGazeModelInitFailed", "reason", "net reading returned empty");
            return false;
        }
        
        // Optimizations for inference speed
        try {
            net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU); // Can target CUDA/OpenCL depending on platform
        } catch (const std::exception& e) {
            log_info("OpenCVGazeModelOptimizationFallback", "reason", e.what());
        }

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

    // 1. Convert eye crop arrays to OpenCV Mats (60x60 px, 3-channel BGR)
    cv::Mat left_eye_mat(60, 60, CV_8UC3, const_cast<unsigned char*>(crops.left_eye_data));
    cv::Mat right_eye_mat(60, 60, CV_8UC3, const_cast<unsigned char*>(crops.right_eye_data));

    // 2. Format images into DNN input blobs (raw pixel values in [0, 255])
    cv::Mat left_blob = cv::dnn::blobFromImage(left_eye_mat, 1.0, cv::Size(60, 60), cv::Scalar(0), false);
    cv::Mat right_blob = cv::dnn::blobFromImage(right_eye_mat, 1.0, cv::Size(60, 60), cv::Scalar(0), false);

    // 3. Format head pose features: Yaw, Pitch, Roll in degrees
    float head_pose_data[3] = {
        static_cast<float>(crops.head_pose_rotation.y), // Yaw
        static_cast<float>(crops.head_pose_rotation.x), // Pitch
        static_cast<float>(crops.head_pose_rotation.z)  // Roll
    };
    cv::Mat head_pose_blob(1, 3, CV_32F, head_pose_data);

    // 4. Set inputs into their corresponding ONNX tensor nodes (Intel ADAS names)
    net.setInput(left_blob, "left_eye_image");
    net.setInput(right_blob, "right_eye_image");
    net.setInput(head_pose_blob, "head_pose_angles");

    // 5. Run forward pass
    cv::Mat output = net.forward("gaze_vector");

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
    } else if (output.cols == 3) {
        // Keep Intel ADAS coordinate system output directly
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
