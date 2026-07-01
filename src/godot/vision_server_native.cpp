#include "vision_server.hpp"
#include "../core/log.hpp"
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "../core/math_defs.hpp"
#include <cstring>

#ifndef WEB_ENABLED

static_assert(sizeof(Gaze::GazeVector3) == sizeof(godot::Vector3), "Size of GazeVector3 must match godot::Vector3");
static_assert(alignof(Gaze::GazeVector3) == alignof(godot::Vector3), "Alignment of GazeVector3 must match godot::Vector3");

#if defined(WINDOWS_ENABLED) || defined(_WIN32)
#include "../windows/wmf_camera.hpp"
using NativeCamera = Gaze::WMFCamera;
#else
#include "godot_camera.hpp"
using NativeCamera = Gaze::GodotCamera;
#endif

namespace godot {

bool VisionServer::camera_start(RID p_camera) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL_V(data, false);

    if (data->is_active) return true;

    // Device ID -1 represents virtual/fake camera (managed by MockVisionServer)
    if (data->device_id == -1) {
        data->is_active = true;
        return true;
    }

    if (!data->camera) {
        data->camera = new NativeCamera(data->device_id);
    }
    data->camera->set_resolution(data->width, data->height);
    
    if (data->camera->initialize()) {
        data->is_active = true;
        Gaze::log_info("VisionServer_CameraStarted", "device_id", data->device_id);
        return true;
    } else {
        Gaze::log_error("VisionServer_CameraStartFailed", "device_id", data->device_id);
        delete data->camera;
        data->camera = nullptr;
        return false;
    }
}

void VisionServer::camera_stop(RID p_camera) {
    Gaze::log_info("VisionServer_CameraStop_Began");
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL(data);

    if (!data->is_active) {
        Gaze::log_info("VisionServer_CameraStop_NotActive");
        return;
    }

    if (data->camera) {
        Gaze::log_info("VisionServer_CameraStop_CameraRelease_Began");
        data->camera->release();
        Gaze::log_info("VisionServer_CameraStop_CameraRelease_Finished");
        delete data->camera;
        data->camera = nullptr;
    }

    data->is_active = false;
    data->last_frame = Gaze::Frame();
    data->last_frame_data.clear();
    Gaze::log_info("VisionServer_CameraStopped", "device_id", data->device_id);
}

bool VisionServer::get_camera_current_frame(RID p_camera, Gaze::Frame &r_frame) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL_V(data, false);

    if (!data->is_active) return false;

    // Handle mock frame injection
    if (data->device_id == -1) {
        if (data->last_frame.data == nullptr) {
            return false;
        }
        r_frame = data->last_frame;
        return true;
    }

    if (!data->camera) return false;

    Gaze::Frame raw_frame;
    bool success = data->camera->grab_frame(raw_frame);
    if (success) {
        if (data->preview_requested) {
            // Convert captured BGR frame to RGB Image for Godot preview
            int width = raw_frame.width;
            int height = raw_frame.height;
            int size = width * height * 3;

            PackedByteArray img_data;
            img_data.resize(size);
            uint8_t* dst = img_data.ptrw();
            const uint8_t* src = raw_frame.data;

            // Convert BGR to RGB
            for (int i = 0; i < width * height; ++i) {
                dst[i * 3 + 0] = src[i * 3 + 2]; // R
                dst[i * 3 + 1] = src[i * 3 + 1]; // G
                dst[i * 3 + 2] = src[i * 3 + 0]; // B
            }

            Ref<Image> img = Image::create_from_data(width, height, false, Image::FORMAT_RGB8, img_data);
            data->current_image = img;
            if (data->current_texture.is_null() || data->current_texture->get_size() != img->get_size()) {
                data->current_texture = ImageTexture::create_from_image(img);
            } else {
                data->current_texture->set_image(img);
            }
        }

        // Copy raw BGR bytes to server thread-safe cache
        size_t frame_bytes = raw_frame.width * raw_frame.height * 3;
        data->last_frame_data.resize(frame_bytes);
        std::memcpy(data->last_frame_data.data(), raw_frame.data, frame_bytes);

        data->last_frame.width = raw_frame.width;
        data->last_frame.height = raw_frame.height;
        data->last_frame.data = data->last_frame_data.data();
        data->last_frame.timestamp = raw_frame.timestamp;

        r_frame = data->last_frame;
    }
    return success;
}

} // namespace godot

#endif // WEB_ENABLED
