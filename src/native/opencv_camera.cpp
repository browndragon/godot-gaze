#include "opencv_camera.hpp"
#include "log.hpp"
#include <chrono>
#include <opencv2/imgproc.hpp>

namespace Gaze {

OpenCVCamera::OpenCVCamera(int device) : device_id(device), start_time(0.0) {}

OpenCVCamera::~OpenCVCamera() {
    release();
}

bool OpenCVCamera::initialize() {
    log_info("CameraInitAttempt", "device_id", device_id);
    cap.open(device_id, cv::CAP_ANY);
    if (!cap.isOpened()) {
        log_error("CameraInitFailed", "device_id", device_id);
        return false;
    }

    // Set resolution properties (optional / recommended defaults)
    cap.set(cv::CAP_PROP_FRAME_WIDTH, target_width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, target_height);
    cap.set(cv::CAP_PROP_FPS, 30);

    auto now = std::chrono::steady_clock::now();
    start_time = std::chrono::duration<double>(now.time_since_epoch()).count();
    
    log_info("CameraInitKind", "target_width", target_width, "target_height", target_height);
    log_info("CameraInitSuccess", "width", cap.get(cv::CAP_PROP_FRAME_WIDTH), "height", cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    return true;
}

void OpenCVCamera::set_resolution(int w, int h) {
    target_width = w;
    target_height = h;
}

bool OpenCVCamera::grab_frame(Frame& out_frame) {
    if (!cap.isOpened()) {
        return false;
    }

    cv::Mat mat;
    if (!cap.read(mat) || mat.empty()) {
        return false;
    }

    // Ensure the frame has 3 channels (BGR) to support YuNet face detector requirements
    cv::Mat bgr;
    if (mat.channels() == 1) {
        cv::cvtColor(mat, bgr, cv::COLOR_GRAY2BGR);
    } else {
        bgr = mat;
    }

    // Copy to persistent frame buffer to avoid lifetime issues
    frame_buffer.resize(bgr.total() * bgr.elemSize());
    std::memcpy(frame_buffer.data(), bgr.data, frame_buffer.size());

    auto now = std::chrono::steady_clock::now();
    double current_time = std::chrono::duration<double>(now.time_since_epoch()).count();

    out_frame.width = bgr.cols;
    out_frame.height = bgr.rows;
    out_frame.data = frame_buffer.data();
    out_frame.timestamp = current_time - start_time;

    return true;
}

void OpenCVCamera::release() {
    if (cap.isOpened()) {
        cap.release();
        log_info("CameraReleased", "device_id", device_id);
    }
}

} // namespace Gaze
