/**
 * @file wmf_camera.hpp
 * @brief Windows Media Foundation (WMF) camera capture (Layer 1 - Native)
 */
#pragma once

#include "../core/camera_interface.hpp"
#include <vector>

namespace Gaze
{

    class WMFCamera : public CameraInterface
    {
    private:
        int device_id;
        std::vector<unsigned char> frame_buffer;
        double start_time;
        // TODO: Are these target_width/height *reasonable*? Can they be pulled from some other good const source (constexpr static members of camera_interface.hpp?)
        //       We know our underlying model strongly prefers 640x640, so isn't that our preferred size (?).
        int target_width = 640;
        int target_height = 480;

        void *pReader = nullptr; // IMFSourceReader*
        bool m_initialized = false;

    public:
        WMFCamera(int device = 0);
        virtual ~WMFCamera();

        virtual bool initialize() override;
        virtual void set_resolution(int w, int h) override;
        virtual bool grab_frame(Frame &out_frame) override;
        virtual void release() override;
    };

} // namespace Gaze
