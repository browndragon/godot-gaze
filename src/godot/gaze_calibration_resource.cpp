// src/godot/gaze_calibration_resource.cpp
#include "gaze_calibration_resource.hpp"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void GazeCalibrationResource::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_bias_pitch", "val"), &GazeCalibrationResource::set_bias_pitch);
    ClassDB::bind_method(D_METHOD("get_bias_pitch"), &GazeCalibrationResource::get_bias_pitch);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bias_pitch"), "set_bias_pitch", "get_bias_pitch");

    ClassDB::bind_method(D_METHOD("set_bias_yaw", "val"), &GazeCalibrationResource::set_bias_yaw);
    ClassDB::bind_method(D_METHOD("get_bias_yaw"), &GazeCalibrationResource::get_bias_yaw);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bias_yaw"), "set_bias_yaw", "get_bias_yaw");

    ClassDB::bind_method(D_METHOD("set_bias_pixel_x", "val"), &GazeCalibrationResource::set_bias_pixel_x);
    ClassDB::bind_method(D_METHOD("get_bias_pixel_x"), &GazeCalibrationResource::get_bias_pixel_x);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bias_pixel_x"), "set_bias_pixel_x", "get_bias_pixel_x");

    ClassDB::bind_method(D_METHOD("set_bias_pixel_y", "val"), &GazeCalibrationResource::set_bias_pixel_y);
    ClassDB::bind_method(D_METHOD("get_bias_pixel_y"), &GazeCalibrationResource::get_bias_pixel_y);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bias_pixel_y"), "set_bias_pixel_y", "get_bias_pixel_y");
}

void GazeCalibrationResource::set_bias_pitch(double val) {
    calibration.bias_pitch = val;
    emit_changed();
}

double GazeCalibrationResource::get_bias_pitch() const {
    return calibration.bias_pitch;
}

void GazeCalibrationResource::set_bias_yaw(double val) {
    calibration.bias_yaw = val;
    emit_changed();
}

double GazeCalibrationResource::get_bias_yaw() const {
    return calibration.bias_yaw;
}

void GazeCalibrationResource::set_bias_pixel_x(double val) {
    calibration.bias_pixel_x = val;
    emit_changed();
}

double GazeCalibrationResource::get_bias_pixel_x() const {
    return calibration.bias_pixel_x;
}

void GazeCalibrationResource::set_bias_pixel_y(double val) {
    calibration.bias_pixel_y = val;
    emit_changed();
}

double GazeCalibrationResource::get_bias_pixel_y() const {
    return calibration.bias_pixel_y;
}

Gaze::GazeCalibration GazeCalibrationResource::get_calibration() const {
    return calibration;
}

void GazeCalibrationResource::set_calibration(const Gaze::GazeCalibration& c) {
    calibration = c;
    emit_changed();
}

} // namespace godot
