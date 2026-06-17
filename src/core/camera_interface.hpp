// src/core/camera_interface.hpp
#pragma once

namespace Gaze {

struct Frame {
    int width = 0;
    int height = 0;
    const unsigned char* data = nullptr; // Raw pixel buffer (e.g. RGB or gray)
    double timestamp = 0.0;             // High precision timestamp in seconds
};

class CameraInterface {
public:
    virtual ~CameraInterface() = default;

    // Initializes the capture source (camera device or mock file)
    virtual bool initialize() = 0;

    // Grabs the next frame. Returns true if successful.
    virtual bool grab_frame(Frame& out_frame) = 0;

    // Releases the capture source resources
    virtual void release() = 0;
};

} // namespace Gaze
