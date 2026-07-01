#include "camera_sensor.hpp"
#include "vision_server.hpp"
#include "gaze_server.hpp"
#include "log.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/texture2d.hpp>

namespace godot {

void CameraSensor::_bind_methods() {

    ClassDB::bind_method(D_METHOD("initialize_sensor"), &CameraSensor::initialize_sensor);
    ClassDB::bind_method(D_METHOD("stop_sensor"), &CameraSensor::stop_sensor);
    ClassDB::bind_method(D_METHOD("get_last_frame"), &CameraSensor::get_last_frame);
    ClassDB::bind_method(D_METHOD("get_camera_device_id"), &CameraSensor::get_camera_device_id);
    ClassDB::bind_method(D_METHOD("set_camera_device_id", "id"), &CameraSensor::set_camera_device_id);
    ClassDB::bind_method(D_METHOD("set_resolution", "width", "height"), &CameraSensor::set_resolution);
    ClassDB::bind_method(D_METHOD("get_focal_length"), &CameraSensor::get_focal_length);
    ClassDB::bind_method(D_METHOD("set_focal_length", "focal"), &CameraSensor::set_focal_length);
    ClassDB::bind_method(D_METHOD("get_camera_fov"), &CameraSensor::get_camera_fov);
    ClassDB::bind_method(D_METHOD("set_camera_fov", "fov"), &CameraSensor::set_camera_fov);
    ClassDB::bind_method(D_METHOD("get_camera_rid"), &CameraSensor::get_camera_rid);
    ClassDB::bind_method(D_METHOD("_on_gaze_data_ready", "rid"), &CameraSensor::_on_gaze_data_ready);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "camera_device_id"), "set_camera_device_id", "get_camera_device_id");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "focal_length"), "set_focal_length", "get_focal_length");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "camera_fov"), "set_camera_fov", "get_camera_fov");

    ADD_SIGNAL(MethodInfo("frame_ready", PropertyInfo(Variant::OBJECT, "image", PROPERTY_HINT_RESOURCE_TYPE, "Image")));
}

CameraSensor::CameraSensor() {}

CameraSensor::~CameraSensor() {
    Gaze::log_info("CameraSensor_Destructor_Began");
    stop_sensor();
    Gaze::log_info("CameraSensor_Destructor_Finished");
}

bool CameraSensor::initialize_sensor() {
    if (camera_rid.is_valid()) return true;

    VisionServer *vs = VisionServer::get_singleton();
    if (!vs) return false;

    camera_rid = vs->camera_create();
    vs->camera_set_device_id(camera_rid, camera_device_id);
    vs->camera_set_resolution(camera_rid, desired_width, desired_height);
    vs->camera_set_focal_length(camera_rid, focal_length);
    vs->camera_set_fov(camera_rid, camera_fov);

    GazeServer *gs = GazeServer::get_singleton();
    if (gs) {
        gs->connect("gaze_data_ready", Callable(this, "_on_gaze_data_ready"));
    }

    return vs->camera_start(camera_rid);
}

void CameraSensor::stop_sensor() {
    Gaze::log_info("CameraSensor_StopSensor_Began");
    if (camera_rid.is_valid()) {
        GazeServer *gs = GazeServer::get_singleton();
        if (gs && gs->is_connected("gaze_data_ready", Callable(this, "_on_gaze_data_ready"))) {
            gs->disconnect("gaze_data_ready", Callable(this, "_on_gaze_data_ready"));
        }

        VisionServer *vs = VisionServer::get_singleton();
        if (vs) {
            Gaze::log_info("CameraSensor_StopSensor_CameraStop_Began");
            vs->camera_stop(camera_rid);
            Gaze::log_info("CameraSensor_StopSensor_CameraStop_Finished");
            vs->camera_free(camera_rid);
        }
        camera_rid = RID();
    }
    Gaze::log_info("CameraSensor_StopSensor_Finished");
}

Ref<Image> CameraSensor::get_last_frame() const {
    if (!camera_rid.is_valid()) return Ref<Image>();
    
    VisionServer *vs = VisionServer::get_singleton();
    if (vs) {
        return vs->camera_get_current_image(camera_rid);
    }
    return Ref<Image>();
}

void CameraSensor::set_camera_device_id(int id) {
    camera_device_id = id;
    if (camera_rid.is_valid()) {
        VisionServer::get_singleton()->camera_set_device_id(camera_rid, id);
    }
}

int CameraSensor::get_camera_device_id() const {
    return camera_device_id;
}

void CameraSensor::set_resolution(int w, int h) {
    desired_width = w;
    desired_height = h;
    if (camera_rid.is_valid()) {
        VisionServer::get_singleton()->camera_set_resolution(camera_rid, w, h);
    }
}

void CameraSensor::set_focal_length(double focal) {
    focal_length = focal;
    if (camera_rid.is_valid()) {
        VisionServer::get_singleton()->camera_set_focal_length(camera_rid, focal);
    }
}

double CameraSensor::get_focal_length() const {
    return focal_length;
}

void CameraSensor::set_camera_fov(double fov) {
    camera_fov = fov;
    if (camera_rid.is_valid()) {
        VisionServer::get_singleton()->camera_set_fov(camera_rid, fov);
    }
}

double CameraSensor::get_camera_fov() const {
    return camera_fov;
}

void CameraSensor::_on_gaze_data_ready(RID p_rid) {
    UtilityFunctions::print("[C++] CameraSensor::_on_gaze_data_ready: p_rid=", p_rid, " camera_rid=", camera_rid);
    if (p_rid == camera_rid) {
        emit_signal("frame_ready", get_last_frame());
    }
}



} // namespace godot
