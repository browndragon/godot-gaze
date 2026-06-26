/**
 * @file camera_interface.hpp
 * @brief Frame Ingestion Interface (Layer 1)
 *
 * Defines the abstract interface and data structures for ingesting frames
 * from hardware camera devices, video files, or mock frame injectors.
 * Declares the core Frame struct which propagates raw pixel data and timestamps.
 */
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
