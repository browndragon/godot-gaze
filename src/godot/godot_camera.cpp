#include "godot_camera.hpp"
#include "../core/log.hpp"
#include <chrono>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/project_settings.hpp>

namespace Gaze {

GodotCamera::GodotCamera(int device) : device_id(device), start_time(0.0), horizontal_flip(false) {}

GodotCamera::~GodotCamera() {
    log_info(2, "GodotCamera_Destructor_Began");
    release();
    log_info(2, "GodotCamera_Destructor_Finished");
}

bool GodotCamera::initialize() {
    log_info("GodotCamera_InitAttempt", "device_id", device_id);
    if (device_id < 0) {
        log_error("GodotCamera_NoFeedsAvailable");
        return false;
    }

    godot::CameraServer* cs = godot::CameraServer::get_singleton();
    if (!cs) {
        log_error("GodotCamera_ServerNotFound");
        return false;
    }
    cs->set_monitoring_feeds(true);
    godot::TypedArray<godot::Ref<godot::CameraFeed>> feeds = cs->feeds();
    
    // Select the camera feed matching device_id or position
    for (int i = 0; i < feeds.size(); ++i) {
        godot::Ref<godot::CameraFeed> f = feeds[i];
        if (f.is_valid()) {
            if (device_id == i || f->get_position() == godot::CameraFeed::FEED_FRONT) {
                feed = f;
                break;
            }
        }
    }

    if (feed.is_null()) {
        if (feeds.size() > 0) {
            feed = feeds[0];
        } else {
            log_error("GodotCamera_NoFeedsAvailable");
            return false;
        }
    }

    // Activate the feed
    feed->set_active(true);
    int datatype = feed->get_datatype();
    log_info("GodotCamera_FeedActive", "feed_id", feed->get_id(), "datatype", datatype);

    if (datatype == 2) {
        is_ycbcr = true;
        y_texture.instantiate();
        y_texture->set_camera_feed_id(feed->get_id());
        y_texture->set_camera_active(true);
        y_texture->set_which_feed(godot::CameraServer::FEED_Y_IMAGE);

        cbcr_texture.instantiate();
        cbcr_texture->set_camera_feed_id(feed->get_id());
        cbcr_texture->set_camera_active(true);
        cbcr_texture->set_which_feed(godot::CameraServer::FEED_CBCR_IMAGE);
    } else {
        is_ycbcr = false;
        camera_texture.instantiate();
        camera_texture->set_camera_feed_id(feed->get_id());
        camera_texture->set_camera_active(true);
        camera_texture->set_which_feed(godot::CameraServer::FEED_RGBA_IMAGE);
    }

    auto now = std::chrono::steady_clock::now();
    start_time = std::chrono::duration<double>(now.time_since_epoch()).count();

    log_info("GodotCamera_InitSuccess", "feed_id", feed->get_id(), "feed_name", feed->get_name().utf8().get_data());
    return true;
}

void GodotCamera::set_resolution(int w, int h) {
    target_width = w;
    target_height = h;
}

bool GodotCamera::grab_frame(Frame& out_frame) {
    if (feed.is_null()) {
        return false;
    }

    if (is_ycbcr) {
        if (y_texture.is_null() || cbcr_texture.is_null()) {
            return false;
        }

        godot::Ref<godot::Image> y_img = y_texture->get_image();
        godot::Ref<godot::Image> cbcr_img = cbcr_texture->get_image();

        if (y_img.is_null() || y_img->is_empty() || cbcr_img.is_null() || cbcr_img->is_empty()) {
            // Check if we forced YCbCr, but textures are not populated, fallback to RGB
            int datatype = feed->get_datatype();
            if (datatype == 1) {
                log_info("GodotCamera_YCbCrFallback", "reason", "textures_empty_falling_back_to_rgb");
                is_ycbcr = false;
                camera_texture.instantiate();
                camera_texture->set_camera_feed_id(feed->get_id());
                camera_texture->set_camera_active(true);
                camera_texture->set_which_feed(godot::CameraServer::FEED_RGBA_IMAGE);
            } else {
                return false;
            }
        }
    }

    if (!is_ycbcr) {
        if (camera_texture.is_null()) {
            return false;
        }

        godot::Ref<godot::Image> img = camera_texture->get_image();
        if (img.is_null()) {
            static int null_count = 0;
            if (null_count++ % 100 == 0) {
                log_warning("GodotCamera_ImageIsNull");
            }
            return false;
        }
        if (img->is_empty()) {
            static int empty_count = 0;
            if (empty_count++ % 100 == 0) {
                log_warning("GodotCamera_ImageIsEmpty", "w", img->get_width(), "h", img->get_height(), "data_size", (int)img->get_data().size());
            }
            return false;
        }

        godot::Image::Format format = img->get_format();
        int width = img->get_width();
        int height = img->get_height();
        int size = width * height * 3;

        static bool logged_first_frame = false;
        if (!logged_first_frame) {
            log_info("GodotCamera_FirstFrameGrabbed", "w", width, "h", height, "format", (int)format, "data_size", (int)img->get_data().size());
            logged_first_frame = true;
        }
        log_info(4, "GodotCamera_FrameGrabbed", "w", width, "h", height, "format", (int)format);

        if (format == godot::Image::FORMAT_R8) {
            godot::PackedByteArray data = img->get_data();
            if (data.size() < width * height) {
                log_warning("GodotCamera_R8SizeMismatch", "data_size", (int)data.size(), "expected", width * height);
                return false;
            }
            frame_buffer.resize(size);
            const uint8_t* src = data.ptr();
            uint8_t* dst = frame_buffer.data();
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int src_x = horizontal_flip ? (width - 1 - x) : x;
                    uint8_t val = src[y * width + src_x];
                    int dst_idx = (y * width + x) * 3;
                    dst[dst_idx + 0] = val; // B
                    dst[dst_idx + 1] = val; // G
                    dst[dst_idx + 2] = val; // R
                }
            }
        } else {
            if (format != godot::Image::FORMAT_RGB8) {
                img = img->duplicate(); // Duplicate before modifying to avoid locking issues on some feeds
                img->convert(godot::Image::FORMAT_RGB8);
            }
            godot::PackedByteArray data = img->get_data();
            if (data.size() < size) {
                log_warning("GodotCamera_RGB8SizeMismatch", "data_size", (int)data.size(), "expected", size);
                return false;
            }
            frame_buffer.resize(size);
            const uint8_t* src = data.ptr();
            uint8_t* dst = frame_buffer.data();
            // Convert RGB to BGR and optionally flip horizontally to undo webcam mirroring
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int src_x = horizontal_flip ? (width - 1 - x) : x;
                    int src_idx = (y * width + src_x) * 3;
                    int dst_idx = (y * width + x) * 3;
                    dst[dst_idx + 0] = src[src_idx + 2]; // B
                    dst[dst_idx + 1] = src[src_idx + 1]; // G
                    dst[dst_idx + 2] = src[src_idx + 0]; // R
                }
            }
        }

        auto now = std::chrono::steady_clock::now();
        double current_time = std::chrono::duration<double>(now.time_since_epoch()).count();

        out_frame.width = width;
        out_frame.height = height;
        out_frame.data = frame_buffer.data();
        out_frame.timestamp = current_time - start_time;

        return true;
    }

    godot::Ref<godot::Image> y_img = y_texture->get_image();
    godot::Ref<godot::Image> cbcr_img = cbcr_texture->get_image();

    if (y_img.is_null() || y_img->is_empty() || cbcr_img.is_null() || cbcr_img->is_empty()) {
        return false;
    }

    int width = y_img->get_width();
    int height = y_img->get_height();
    int size = width * height * 3;

    godot::PackedByteArray y_data = y_img->get_data();
    godot::PackedByteArray cbcr_data = cbcr_img->get_data();

    if (y_data.size() < width * height || cbcr_data.size() < (width / 2) * (height / 2) * 2) {
        return false;
    }

    frame_buffer.resize(size);
    const uint8_t* y_ptr = y_data.ptr();
    const uint8_t* cbcr_ptr = cbcr_data.ptr();
    uint8_t* dst = frame_buffer.data();

    int cbcr_w = cbcr_img->get_width();

    // Perform YCbCr (NV12) to BGR conversion
    for (int y = 0; y < height; ++y) {
        int cy = y / 2;
        for (int x = 0; x < width; ++x) {
            int src_x = horizontal_flip ? (width - 1 - x) : x;
            int src_cx = src_x / 2;

            uint8_t y_val = y_ptr[y * width + src_x];
            int cbcr_idx = (cy * cbcr_w + src_cx) * 2;
            uint8_t cb_val = cbcr_ptr[cbcr_idx];
            uint8_t cr_val = cbcr_ptr[cbcr_idx + 1];

            float Y = static_cast<float>(y_val);
            float Cb = static_cast<float>(cb_val) - 128.0f;
            float Cr = static_cast<float>(cr_val) - 128.0f;

            // YCbCr -> RGB (BT.601)
            float r = Y + 1.402f * Cr;
            float g = Y - 0.344136f * Cb - 0.714136f * Cr;
            float b = Y + 1.772f * Cb;

            int dst_idx = (y * width + x) * 3;
            dst[dst_idx + 0] = static_cast<uint8_t>(std::max(0.0f, std::min(b, 255.0f))); // B
            dst[dst_idx + 1] = static_cast<uint8_t>(std::max(0.0f, std::min(g, 255.0f))); // G
            dst[dst_idx + 2] = static_cast<uint8_t>(std::max(0.0f, std::min(r, 255.0f))); // R
        }
    }

    auto now = std::chrono::steady_clock::now();
    double current_time = std::chrono::duration<double>(now.time_since_epoch()).count();

    out_frame.width = width;
    out_frame.height = height;
    out_frame.data = frame_buffer.data();
    out_frame.timestamp = current_time - start_time;

    return true;
}

void GodotCamera::release() {
    log_info(2, "GodotCamera_Release_Began");
    if (feed.is_valid()) {
        log_info(2, "GodotCamera_Release_SetActiveFalse_Began", "feed_id", feed->get_id());
        feed->set_active(false);
        log_info(2, "GodotCamera_Release_SetActiveFalse_Finished");
        log_info(2, "GodotCamera_Released", "feed_id", feed->get_id());
        feed.unref();
    }
    if (camera_texture.is_valid()) {
        log_info(2, "GodotCamera_Release_CameraTextureActiveFalse_Began");
        camera_texture->set_camera_active(false);
        log_info(2, "GodotCamera_Release_CameraTextureActiveFalse_Finished");
        camera_texture.unref();
    }
    if (y_texture.is_valid()) {
        log_info(2, "GodotCamera_Release_YTextureActiveFalse_Began");
        y_texture->set_camera_active(false);
        log_info(2, "GodotCamera_Release_YTextureActiveFalse_Finished");
        y_texture.unref();
    }
    if (cbcr_texture.is_valid()) {
        log_info(2, "GodotCamera_Release_CbCrTextureActiveFalse_Began");
        cbcr_texture->set_camera_active(false);
        log_info(2, "GodotCamera_Release_CbCrTextureActiveFalse_Finished");
        cbcr_texture.unref();
    }
    log_info(2, "GodotCamera_Release_Finished");
}

} // namespace Gaze
