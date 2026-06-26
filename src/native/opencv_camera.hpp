/**
 * @file opencv_camera.hpp
 * @brief Hardware Camera Capture class using OpenCV (Layer 1 - Native)
 *
 * Implements CameraInterface using OpenCV's cv::VideoCapture to stream frames
 * from local USB/integrated webcam hardware. Outputs raw pixel frames with
 * millisecond timestamp metrics.
 */
#pragma once

#include "camera_interface.hpp"
#include <opencv2/videoio.hpp>
#include <vector>

namespace Gaze {

class OpenCVCamera : public CameraInterface {
private:
    int device_id;
    cv::VideoCapture cap;
    std::vector<unsigned char> frame_buffer;
    double start_time;

public:
    OpenCVCamera(int device = 0);
    virtual ~OpenCVCamera();

    virtual bool initialize() override;
    virtual bool grab_frame(Frame& out_frame) override;
    virtual void release() override;
};

} // namespace Gaze
