#include "gaze_pipeline_config.hpp"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void GazePipelineConfig::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_pitch_t_gain", "val"), &GazePipelineConfig::set_pitch_t_gain);
    ClassDB::bind_method(D_METHOD("get_pitch_t_gain"), &GazePipelineConfig::get_pitch_t_gain);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pitch_t_gain"), "set_pitch_t_gain", "get_pitch_t_gain");

    ClassDB::bind_method(D_METHOD("set_yaw_t_gain", "val"), &GazePipelineConfig::set_yaw_t_gain);
    ClassDB::bind_method(D_METHOD("get_yaw_t_gain"), &GazePipelineConfig::get_yaw_t_gain);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "yaw_t_gain"), "set_yaw_t_gain", "get_yaw_t_gain");

    ClassDB::bind_method(D_METHOD("set_nose_z", "val"), &GazePipelineConfig::set_nose_z);
    ClassDB::bind_method(D_METHOD("get_nose_z"), &GazePipelineConfig::get_nose_z);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "nose_z"), "set_nose_z", "get_nose_z");

    ClassDB::bind_method(D_METHOD("set_ipd_mm", "val"), &GazePipelineConfig::set_ipd_mm);
    ClassDB::bind_method(D_METHOD("get_ipd_mm"), &GazePipelineConfig::get_ipd_mm);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ipd_mm"), "set_ipd_mm", "get_ipd_mm");

    ClassDB::bind_method(D_METHOD("set_face_detect_width", "val"), &GazePipelineConfig::set_face_detect_width);
    ClassDB::bind_method(D_METHOD("get_face_detect_width"), &GazePipelineConfig::get_face_detect_width);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "face_detect_width"), "set_face_detect_width", "get_face_detect_width");

    ClassDB::bind_method(D_METHOD("set_face_detect_height", "val"), &GazePipelineConfig::set_face_detect_height);
    ClassDB::bind_method(D_METHOD("get_face_detect_height"), &GazePipelineConfig::get_face_detect_height);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "face_detect_height"), "set_face_detect_height", "get_face_detect_height");

    ClassDB::bind_method(D_METHOD("set_desired_camera_size", "size"), &GazePipelineConfig::set_desired_camera_size);
    ClassDB::bind_method(D_METHOD("get_desired_camera_size"), &GazePipelineConfig::get_desired_camera_size);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "desired_camera_size"), "set_desired_camera_size", "get_desired_camera_size");
}

void GazePipelineConfig::set_pitch_t_gain(double val) { config.pitch_t_gain = val; }
double GazePipelineConfig::get_pitch_t_gain() const { return config.pitch_t_gain; }

void GazePipelineConfig::set_yaw_t_gain(double val) { config.yaw_t_gain = val; }
double GazePipelineConfig::get_yaw_t_gain() const { return config.yaw_t_gain; }

void GazePipelineConfig::set_nose_z(double val) { config.nose_z = val; }
double GazePipelineConfig::get_nose_z() const { return config.nose_z; }

void GazePipelineConfig::set_ipd_mm(double val) { config.ipd_mm = val; }
double GazePipelineConfig::get_ipd_mm() const { return config.ipd_mm; }

void GazePipelineConfig::set_face_detect_width(int val) { config.face_detect_width = val; }
int GazePipelineConfig::get_face_detect_width() const { return config.face_detect_width; }

void GazePipelineConfig::set_face_detect_height(int val) { config.face_detect_height = val; }
int GazePipelineConfig::get_face_detect_height() const { return config.face_detect_height; }

void GazePipelineConfig::set_desired_camera_size(Vector2i size) {
    config.desired_camera_width = size.x;
    config.desired_camera_height = size.y;
}

Vector2i GazePipelineConfig::get_desired_camera_size() const {
    return Vector2i(config.desired_camera_width, config.desired_camera_height);
}

} // namespace godot
