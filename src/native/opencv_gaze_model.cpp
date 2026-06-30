#include "opencv_gaze_model.hpp"
#include "log.hpp"

namespace Gaze {

OpenCVGazeModel::OpenCVGazeModel(const std::string& gaze_onnx_path) : model_path(gaze_onnx_path) {}

OpenCVGazeModel::OpenCVGazeModel(const std::vector<uint8_t>& onnx_buffer)
    : model_buffer(onnx_buffer), load_from_buffer(true), is_openvino(false) {}

OpenCVGazeModel::OpenCVGazeModel(const std::vector<uint8_t>& xml_buffer, const std::vector<uint8_t>& bin_buffer)
    : model_buffer(xml_buffer), bin_buffer(bin_buffer), load_from_buffer(true), is_openvino(true) {}

bool OpenCVGazeModel::initialize() {
    try {
        if (load_from_buffer) {
            log_info("OpenCVGazeModelInitAttemptBuffer", "is_openvino", is_openvino);
            if (is_openvino) {
                net = cv::dnn::readNetFromModelOptimizer(
                    reinterpret_cast<const unsigned char*>(model_buffer.data()), model_buffer.size(),
                    reinterpret_cast<const unsigned char*>(bin_buffer.data()), bin_buffer.size()
                );
            } else {
                net = cv::dnn::readNetFromONNX(
                    reinterpret_cast<const char*>(model_buffer.data()), model_buffer.size()
                );
            }
        } else {
            log_info("OpenCVGazeModelInitAttempt", "model_path", model_path);
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


bool OpenCVGazeModel::estimate_raw_gaze(const EyeCrops& crops, GazeVector3& out_gaze_dir_cv) {
    if (net.empty()) {
        return false;
    }

    // 1. Convert eye crop arrays to OpenCV Mats (60x60 px, 3-channel BGR)
    cv::Mat left_eye_mat(60, 60, CV_8UC3, const_cast<unsigned char*>(crops.left_eye_data));
    cv::Mat right_eye_mat(60, 60, CV_8UC3, const_cast<unsigned char*>(crops.right_eye_data));

    // 2. Format images into DNN input blobs (raw pixel values in [0, 255])
    cv::Mat left_blob = cv::dnn::blobFromImage(left_eye_mat, 1.0, cv::Size(60, 60), cv::Scalar(0), false);
    cv::Mat right_blob = cv::dnn::blobFromImage(right_eye_mat, 1.0, cv::Size(60, 60), cv::Scalar(0), false);

    // Reconstruct head rotation and extract Euler angles (Yaw, Pitch, Roll)
    GazeBasis3D R_basis = rodrigues_to_basis(crops.head_pose_rotation);
    GazeVector3 euler = R_basis.get_euler_gaze_model_deg();

    // Coordinate/Sign Conventions (see docs/gaze_math_physical_model.md Section 7):
    // 1. Yaw (-euler.y): In our Y-down camera space, physical rotation when turning left is negative.
    //    However, the ADAS network expects a positive yaw when turning left. Thus we negate it.
    // 2. Pitch (euler.x): Physical pitch when looking down is positive in both spaces.
    // 3. Roll (-euler.z): Align roll coordinate signs by negating.
    float head_pose_data[3] = {
        static_cast<float>(-euler.y), // Yaw (Model positive yaw turning left)
        static_cast<float>(euler.x),  // Pitch (Model positive pitch looking down)
        static_cast<float>(-euler.z)  // Roll (Model positive roll tilting right)
    };
    cv::Mat head_pose_blob(1, 3, CV_32F, head_pose_data);
 
    // 4. Set inputs into their corresponding ONNX tensor nodes (Intel ADAS names)
    // Note: The model's inputs are named from the viewer's (camera's) perspective:
    // "left_eye_image" receives the image-left eye crop (anatomical right eye, right_blob)
    // and "right_eye_image" receives the image-right eye crop (anatomical left eye, left_blob).
    net.setInput(right_blob, "left_eye_image");
    net.setInput(left_blob, "right_eye_image");
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
        double dx = std::sin(yaw) * cos_pitch;
        double dy = std::sin(pitch);
        double dz = std::cos(yaw) * cos_pitch;
        // Transform Gaze ADAS Space to OpenCV Camera Space: (dx, -dy, dz)
        out_gaze_dir_cv = GazeVector3(dx, -dy, dz).normalized();
    } else if (output.cols == 3) {
        // Transform Gaze ADAS Space to OpenCV Camera Space: (raw_gaze.x, -raw_gaze.y, raw_gaze.z)
        out_gaze_dir_cv = GazeVector3(
            output.at<float>(0, 0),
            -output.at<float>(0, 1),
            output.at<float>(0, 2)
        ).normalized();
    } else {
        return false;
    }

    return true;
}

} // namespace Gaze
