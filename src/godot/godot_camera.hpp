/**
 * @file godot_camera.hpp
 * @brief Platform-native camera ingestion using Godot's CameraServer and CameraFeed
 */
#pragma once

#include "../core/camera_interface.hpp"
#include <godot_cpp/classes/camera_server.hpp>
#include <godot_cpp/classes/camera_feed.hpp>
#include <godot_cpp/classes/camera_texture.hpp>
#include <godot_cpp/classes/image.hpp>
#include <vector>

namespace Gaze {

class GodotCamera : public CameraInterface {
private:
    int device_id;
    godot::Ref<godot::CameraFeed> feed;
    godot::Ref<godot::CameraTexture> camera_texture;
    godot::Ref<godot::CameraTexture> y_texture;
    godot::Ref<godot::CameraTexture> cbcr_texture;
    bool is_ycbcr = false;
    bool horizontal_flip = false;
    std::vector<unsigned char> frame_buffer;
    double start_time;
    int target_width = 640;
    int target_height = 480;

public:
    GodotCamera(int device = 0);
    virtual ~GodotCamera();

    virtual bool initialize() override;
    virtual void set_resolution(int w, int h) override;
    virtual bool grab_frame(Frame& out_frame) override;
    virtual void release() override;
};

} // namespace Gaze
