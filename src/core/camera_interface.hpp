/**
 * @file camera_interface.hpp
 * @brief Frame Ingestion Interface (Layer 1)
 *
 * Defines the abstract interface and data structures for ingesting frames
 * from hardware camera devices, video files, or mock frame injectors.
 * Declares the core Frame struct which propagates raw pixel data and timestamps.
 */
#pragma once
#include <cstdint>
#include <type_traits>

namespace Gaze
{

    struct Frame
    {
        int width;
        int height;
        const uint8_t *data; // Raw pixel buffer (expects BGR 24-bit)
        double timestamp;    // High precision timestamp in seconds

        Frame() = default;
    };

    static_assert(std::is_standard_layout<Frame>::value, "Frame must be standard-layout");
    static_assert(std::is_trivial<Frame>::value, "Frame must be trivial");

    class CameraInterface
    {
    public:
        virtual ~CameraInterface() = default;

        // Initializes the capture source (camera device or mock file)
        virtual bool initialize() = 0;

        // Sets the desired camera capture resolution (if supported by the device)
        virtual void set_resolution(int w, int h) {}

        // Grabs the next frame. Returns true if successful.
        virtual bool grab_frame(Frame &out_frame) = 0;

        // Releases the capture source resources
        virtual void release() = 0;
    };

} // namespace Gaze
