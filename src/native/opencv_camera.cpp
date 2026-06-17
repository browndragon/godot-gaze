// src/native/opencv_camera.cpp
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
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS, 30);

    auto now = std::chrono::steady_clock::now();
    start_time = std::chrono::duration<double>(now.time_since_epoch()).count();
    
    log_info("CameraInitSuccess", "width", cap.get(cv::CAP_PROP_FRAME_WIDTH), "height", cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    return true;
}

bool OpenCVCamera::grab_frame(Frame& out_frame) {
    if (!cap.isOpened()) {
        return false;
    }

    cv::Mat mat;
    if (!cap.read(mat) || mat.empty()) {
        return false;
    }

    // Convert frame to standard grayscale to simplify face detection and pipeline processing
    cv::Mat gray;
    if (mat.channels() == 3) {
        cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = mat;
    }

    // Copy to persistent frame buffer to avoid lifetime issues
    frame_buffer.resize(gray.total() * gray.elemSize());
    std::memcpy(frame_buffer.data(), gray.data, frame_buffer.size());

    auto now = std::chrono::steady_clock::now();
    double current_time = std::chrono::duration<double>(now.time_since_epoch()).count();

    out_frame.width = gray.cols;
    out_frame.height = gray.rows;
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
