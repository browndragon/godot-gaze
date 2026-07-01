#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/templates/rid_owner.hpp>
#include "camera_interface.hpp"
#include "../core/math_defs.hpp"
#include <vector>

namespace godot {

/**
 * @class VisionServer
 * @brief Singleton server coordinating all platform camera configurations and image capture operations.
 *
 * VisionServer manages raw camera feeds (OpenCV or platform-specific drivers) using an RID-based resource model.
 * It provides double-buffered frame texture cache queries to GDScript client nodes and supports virtual camera injection.
 *
 * NOTE: The implementation of VisionServer is split across three platform layers:
 * - src/godot/vision_server.cpp : Handles generic ClassDB binding, lifecycle, RID registration, and virtual cameras.
 * - src/native/vision_server.cpp : Implements native OpenCV camera capture and frame grabbing.
 * - src/web/vision_server.cpp   : Implements Web HTML5 JS/Emscripten canvas capture callbacks.
 */
class VisionServer : public Object {
    GDCLASS(VisionServer, Object);

private:
    static VisionServer *singleton;

protected:
    static void _bind_methods();

    struct CameraData {
        int device_id = 0;
        int width = 640;
        int height = 480;
        double focal_length = 1000.0;
        double camera_fov = Gaze::DEFAULT_CAMERA_FOV_DEGREES;
        Gaze::CameraInterface *camera = nullptr;
        Ref<ImageTexture> current_texture;
        Ref<Image> current_image;
        bool is_active = false;
        bool preview_requested = false;
        
        Gaze::Frame last_frame;
        std::vector<unsigned char> last_frame_data; // Thread-safe back buffer copy
    };

    RID_PtrOwner<CameraData, true> camera_owner;
    std::vector<RID> allocated_cameras;

public:
    VisionServer();
    virtual ~VisionServer();

    /**
     * @brief Get the VisionServer global singleton instance.
     */
    static VisionServer *get_singleton() { return singleton; }

    /**
     * @brief Create a new camera resource and return its unique RID.
     */
    virtual RID camera_create();

    /**
     * @brief Set the hardware device ID index to capture from.
     */
    virtual void camera_set_device_id(RID p_camera, int p_device_id);

    /**
     * @brief Set target camera capture resolution.
     */
    virtual void camera_set_resolution(RID p_camera, int p_width, int p_height);

    /**
     * @brief Set camera lens focal length parameter.
     */
    virtual void camera_set_focal_length(RID p_camera, double p_focal_length);

    /**
     * @brief Get the camera lens focal length parameter.
     */
    virtual double camera_get_focal_length(RID p_camera);

    /**
     * @brief Set camera lens fov parameter.
     */
    virtual void camera_set_fov(RID p_camera, double p_fov);

    /**
     * @brief Get the camera lens fov parameter.
     */
    virtual double camera_get_fov(RID p_camera);

    /**
     * @brief Start frame capture capture on the camera.
     * @return True if started successfully, false otherwise.
     */
    virtual bool camera_start(RID p_camera);

    /**
     * @brief Stop frame capture and release hardware resources.
     */
    virtual void camera_stop(RID p_camera);

    /**
     * @brief Retrieve the latest captured frame texture for Godot rendering.
     */
    virtual Ref<Texture2D> get_camera_current_texture(RID p_camera);

    /**
     * @brief Retrieve the latest captured CPU image resource.
     */
    virtual Ref<Image> camera_get_current_image(RID p_camera);

    virtual void camera_set_preview_requested(RID p_camera, bool p_requested);
    virtual bool camera_is_preview_requested(RID p_camera);

    /**
     * @brief Free the camera resource.
     */
    virtual void camera_free(RID p_camera);

    /**
     * @brief Retrieve the raw frame data for GazeServer or background pipeline threads.
     * @param p_camera The camera RID.
     * @param r_frame Frame structure to populate.
     * @return True if a new frame was grabbed successfully, false otherwise.
     */
    virtual bool get_camera_current_frame(RID p_camera, Gaze::Frame &r_frame);
};

/**
 * @class MockVisionServer
 * @brief Subclass of VisionServer enabling manual frame injection for unit testing and virtualization.
 *
 * MockVisionServer overrides camera capture logic to return manually injected textures rather than reading from a physical webcam.
 */
class MockVisionServer : public VisionServer {
    GDCLASS(MockVisionServer, VisionServer);

protected:
    static void _bind_methods();

public:
    MockVisionServer() = default;
    virtual ~MockVisionServer() = default;

    virtual RID camera_create() override;
    virtual bool camera_start(RID p_camera) override;
    virtual void camera_stop(RID p_camera) override;
    virtual Ref<Texture2D> get_camera_current_texture(RID p_camera) override;
    virtual Ref<Image> camera_get_current_image(RID p_camera) override;

    /**
     * @brief Manually inject a frame texture into the mock camera.
     * @param p_camera The mock camera RID.
     * @param p_texture The frame texture to inject.
     */
    void inject_texture(RID p_camera, const Ref<Texture2D> &p_texture);
};

} // namespace godot
