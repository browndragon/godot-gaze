#include "camera_sensor.hpp"
#include "log.hpp"
#ifndef WEB_ENABLED
#include "opencv_camera.hpp"
#endif
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>

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

    ADD_PROPERTY(PropertyInfo(Variant::INT, "camera_device_id"), "set_camera_device_id", "get_camera_device_id");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "focal_length"), "set_focal_length", "get_focal_length");

    ADD_SIGNAL(MethodInfo("frame_ready", PropertyInfo(Variant::OBJECT, "image", PROPERTY_HINT_RESOURCE_TYPE, "Image")));
}

CameraSensor::CameraSensor() {}

CameraSensor::~CameraSensor() {
    stop_sensor();
}

bool CameraSensor::initialize_sensor() {
#ifndef WEB_ENABLED
    if (!camera) {
        camera = new Gaze::OpenCVCamera(camera_device_id);
    }
    camera->set_resolution(desired_width, desired_height);
    return camera->initialize();
#else
    return true;
#endif
}

void CameraSensor::stop_sensor() {
    if (camera) {
        camera->release();
        delete camera;
        camera = nullptr;
    }
    last_frame.unref();
}

bool CameraSensor::grab_frame(Gaze::Frame& out_frame) {
    if (!camera) return false;
    bool success = camera->grab_frame(out_frame);
    if (success) {
        // Convert captured BGR frame to RGB Image for Godot
        cv::Mat bgr(out_frame.height, out_frame.width, CV_8UC3, const_cast<unsigned char*>(out_frame.data));
        cv::Mat rgb;
        cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);

        PackedByteArray img_data;
        img_data.resize(rgb.total() * rgb.elemSize());
        std::memcpy(img_data.ptrw(), rgb.data, img_data.size());

        last_frame = Image::create_from_data(rgb.cols, rgb.rows, false, Image::FORMAT_RGB8, img_data);
        emit_signal("frame_ready", last_frame);
    }
    return success;
}

Ref<Image> CameraSensor::get_last_frame() const {
    return last_frame;
}

void CameraSensor::set_camera_device_id(int id) {
    camera_device_id = id;
}

int CameraSensor::get_camera_device_id() const {
    return camera_device_id;
}

void CameraSensor::set_resolution(int w, int h) {
    desired_width = w;
    desired_height = h;
    if (camera) {
        camera->set_resolution(w, h);
    }
}

void CameraSensor::set_focal_length(double focal) {
    focal_length = focal;
}

double CameraSensor::get_focal_length() const {
    return focal_length;
}

} // namespace godot
