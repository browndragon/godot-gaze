#include "vision_server.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>

#ifdef WEB_ENABLED
#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/java_script_object.hpp>
#endif

#include "../core/log.hpp"

namespace godot {

VisionServer *VisionServer::singleton = nullptr;

void VisionServer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("camera_create"), &VisionServer::camera_create);
    ClassDB::bind_method(D_METHOD("camera_set_device_id", "camera_rid", "device_id"), &VisionServer::camera_set_device_id);
    ClassDB::bind_method(D_METHOD("camera_set_resolution", "camera_rid", "width", "height"), &VisionServer::camera_set_resolution);
    ClassDB::bind_method(D_METHOD("camera_set_focal_length", "camera_rid", "focal_length"), &VisionServer::camera_set_focal_length);
    ClassDB::bind_method(D_METHOD("camera_get_focal_length", "camera_rid"), &VisionServer::camera_get_focal_length);
    ClassDB::bind_method(D_METHOD("camera_set_fov", "camera_rid", "fov"), &VisionServer::camera_set_fov);
    ClassDB::bind_method(D_METHOD("camera_get_fov", "camera_rid"), &VisionServer::camera_get_fov);
    ClassDB::bind_method(D_METHOD("camera_start", "camera_rid"), &VisionServer::camera_start);
    ClassDB::bind_method(D_METHOD("camera_stop", "camera_rid"), &VisionServer::camera_stop);
    ClassDB::bind_method(D_METHOD("get_camera_current_texture", "camera_rid"), &VisionServer::get_camera_current_texture);
    ClassDB::bind_method(D_METHOD("camera_get_current_image", "camera_rid"), &VisionServer::camera_get_current_image);
    ClassDB::bind_method(D_METHOD("camera_free", "camera_rid"), &VisionServer::camera_free);
    ClassDB::bind_method(D_METHOD("camera_set_preview_requested", "camera_rid", "requested"), &VisionServer::camera_set_preview_requested);
    ClassDB::bind_method(D_METHOD("camera_is_preview_requested", "camera_rid"), &VisionServer::camera_is_preview_requested);
}

VisionServer::VisionServer() {
    if (singleton == nullptr) {
        singleton = this;
    }
}

VisionServer::~VisionServer() {
    Gaze::log_info(2, "VisionServer_Destructor_Began");
    for (RID rid : allocated_cameras) {
        CameraData *data = camera_owner.get_or_null(rid);
        if (data) {
            if (data->camera) {
                Gaze::log_info(2, "VisionServer_Destructor_CameraRelease_Began");
                data->camera->release();
                Gaze::log_info(2, "VisionServer_Destructor_CameraRelease_Finished");
                delete data->camera;
                data->camera = nullptr;
            }
            memdelete(data);
        }
    }
    allocated_cameras.clear();

    if (singleton == this) {
        singleton = nullptr;
    }
    Gaze::log_info(2, "VisionServer_Destructor_Finished");
}

RID VisionServer::camera_create() {
    CameraData *data = memnew(CameraData);
    data->current_texture.instantiate();
    RID rid = camera_owner.make_rid(data);
    allocated_cameras.push_back(rid);
    return rid;
}

void VisionServer::camera_set_device_id(RID p_camera, int p_device_id) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL(data);
    data->device_id = p_device_id;
}

void VisionServer::camera_set_resolution(RID p_camera, int p_width, int p_height) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL(data);
    data->width = p_width;
    data->height = p_height;
}

void VisionServer::camera_set_focal_length(RID p_camera, double p_focal_length) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL(data);
    data->focal_length = p_focal_length;
}

double VisionServer::camera_get_focal_length(RID p_camera) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL_V(data, 1000.0);
    return data->focal_length;
}

void VisionServer::camera_set_fov(RID p_camera, double p_fov) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL(data);
    data->camera_fov = p_fov;
}

double VisionServer::camera_get_fov(RID p_camera) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL_V(data, Gaze::DEFAULT_CAMERA_FOV_DEGREES);
    return data->camera_fov;
}

void VisionServer::camera_set_preview_requested(RID p_camera, bool p_requested) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL(data);
    data->preview_requested = p_requested;
#ifdef WEB_ENABLED
    JavaScriptBridge *js = JavaScriptBridge::get_singleton();
    if (js) {
        Ref<JavaScriptObject> window = js->get_interface("window");
        if (window.is_valid()) {
            Ref<JavaScriptObject> godotGaze = window->get("godotGaze");
            if (godotGaze.is_valid()) {
                godotGaze->set("previewRequested", p_requested);
            }
        }
    }
#endif
}

bool VisionServer::camera_is_preview_requested(RID p_camera) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL_V(data, false);
    return data->preview_requested;
}

void VisionServer::camera_free(RID p_camera) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    if (data) {
        camera_stop(p_camera);
        camera_owner.free(p_camera);
        memdelete(data);
        for (auto it = allocated_cameras.begin(); it != allocated_cameras.end(); ++it) {
            if (*it == p_camera) {
                allocated_cameras.erase(it);
                break;
            }
        }
    }
}

Ref<Texture2D> VisionServer::get_camera_current_texture(RID p_camera) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL_V(data, Ref<Texture2D>());
    return data->current_texture;
}

Ref<Image> VisionServer::camera_get_current_image(RID p_camera) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL_V(data, Ref<Image>());
    return data->current_image;
}


// --- MockVisionServer Implementation ---

void MockVisionServer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("inject_texture", "camera_rid", "texture"), &MockVisionServer::inject_texture);
}

RID MockVisionServer::camera_create() {
    CameraData *data = memnew(CameraData);
    data->current_texture.instantiate();
    // Device ID -1 represents virtual/fake camera
    data->device_id = -1;
    RID rid = camera_owner.make_rid(data);
    allocated_cameras.push_back(rid);
    return rid;
}

bool MockVisionServer::camera_start(RID p_camera) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL_V(data, false);
    data->is_active = true;
    return true;
}

void MockVisionServer::camera_stop(RID p_camera) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL(data);
    data->is_active = false;
}

Ref<Texture2D> MockVisionServer::get_camera_current_texture(RID p_camera) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL_V(data, Ref<Texture2D>());
    return data->current_texture;
}

Ref<Image> MockVisionServer::camera_get_current_image(RID p_camera) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL_V(data, Ref<Image>());
    return data->current_image;
}

void MockVisionServer::inject_texture(RID p_camera, const Ref<Texture2D> &p_texture) {
    CameraData *data = camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL(data);
    if (!data->is_active || p_texture.is_null()) return;

    Ref<Image> image = p_texture->get_image();
    if (image.is_null() || image->is_empty()) return;

    // Convert format to RGBA8 for GPU preprocessing compatibility
    if (image->get_format() != Image::FORMAT_RGBA8) {
        image = image->duplicate();
        image->convert(Image::FORMAT_RGBA8);
    }

    int w = image->get_width();
    int h = image->get_height();
    
    data->current_image = image;
    
    if (data->current_texture.is_null() || data->current_texture->get_size() != image->get_size()) {
        data->current_texture = ImageTexture::create_from_image(image);
    } else {
        data->current_texture->set_image(image);
    }



    // Convert RGBA to BGR raw buffer
    PackedByteArray rgba_data = image->get_data();
    data->last_frame_data.resize(w * h * 3);
    
    unsigned char *dest = data->last_frame_data.data();
    const unsigned char *src = rgba_data.ptr();
    
    for (int i = 0; i < w * h; ++i) {
        dest[i * 3 + 0] = src[i * 4 + 2];     // B
        dest[i * 3 + 1] = src[i * 4 + 1];     // G
        dest[i * 3 + 2] = src[i * 4 + 0];     // R
    }

    data->last_frame.width = w;
    data->last_frame.height = h;
    data->last_frame.data = data->last_frame_data.data();
    data->last_frame.timestamp = (double)UtilityFunctions::snapped(0.001 * UtilityFunctions::randf(), 0.001); // Mock timestamp
}

} // namespace godot
