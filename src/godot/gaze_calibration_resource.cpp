/**
 * @file gaze_calibration_resource.cpp
 * @brief Implement Godot wrapper classes GazeCalibration and GazeDeviceCalibration
 */
#include "gaze_calibration_resource.hpp"
#include "gaze_tracker.hpp"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

// ==================== GazeCalibration ====================

GazeCalibration::GazeCalibration() {
    impl = std::make_shared<Gaze::FixedCalibration>();
}

GazeCalibration::GazeCalibration(std::shared_ptr<Gaze::Calibration> p_impl) : impl(p_impl) {}

void GazeCalibration::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_pixel_size_mm", "val"), &GazeCalibration::set_pixel_size_mm);
    ClassDB::bind_method(D_METHOD("get_pixel_size_mm", "tracker"), &GazeCalibration::get_pixel_size_mm, DEFVAL(nullptr));
    ClassDB::bind_method(D_METHOD("get_pixel_size_mm_bind"), &GazeCalibration::get_pixel_size_mm_bind);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "pixel_size_mm"), "set_pixel_size_mm", "get_pixel_size_mm_bind");

    ClassDB::bind_method(D_METHOD("set_camera_offset", "val"), &GazeCalibration::set_camera_offset);
    ClassDB::bind_method(D_METHOD("get_camera_offset", "tracker"), &GazeCalibration::get_camera_offset, DEFVAL(nullptr));
    ClassDB::bind_method(D_METHOD("get_camera_offset_bind"), &GazeCalibration::get_camera_offset_bind);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "camera_offset"), "set_camera_offset", "get_camera_offset_bind");

    ClassDB::bind_method(D_METHOD("set_camera_tilt", "val"), &GazeCalibration::set_camera_tilt);
    ClassDB::bind_method(D_METHOD("get_camera_tilt", "tracker"), &GazeCalibration::get_camera_tilt, DEFVAL(nullptr));
    ClassDB::bind_method(D_METHOD("get_camera_tilt_bind"), &GazeCalibration::get_camera_tilt_bind);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "camera_tilt"), "set_camera_tilt", "get_camera_tilt_bind");

    ClassDB::bind_method(D_METHOD("set_bias_pitch", "val"), &GazeCalibration::set_bias_pitch);
    ClassDB::bind_method(D_METHOD("get_bias_pitch"), &GazeCalibration::get_bias_pitch);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bias_pitch"), "set_bias_pitch", "get_bias_pitch");

    ClassDB::bind_method(D_METHOD("set_bias_yaw", "val"), &GazeCalibration::set_bias_yaw);
    ClassDB::bind_method(D_METHOD("get_bias_yaw"), &GazeCalibration::get_bias_yaw);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bias_yaw"), "set_bias_yaw", "get_bias_yaw");

    ClassDB::bind_method(D_METHOD("set_bias_pixel_x", "val"), &GazeCalibration::set_bias_pixel_x);
    ClassDB::bind_method(D_METHOD("get_bias_pixel_x"), &GazeCalibration::get_bias_pixel_x);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bias_pixel_x"), "set_bias_pixel_x", "get_bias_pixel_x");

    ClassDB::bind_method(D_METHOD("set_bias_pixel_y", "val"), &GazeCalibration::set_bias_pixel_y);
    ClassDB::bind_method(D_METHOD("get_bias_pixel_y"), &GazeCalibration::get_bias_pixel_y);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bias_pixel_y"), "set_bias_pixel_y", "get_bias_pixel_y");
}

void GazeCalibration::set_pixel_size_mm(Vector2 val) {
    auto fixed = std::dynamic_pointer_cast<Gaze::FixedCalibration>(impl);
    if (fixed) {
        Gaze::GazeCalibration data = fixed->get_data();
        data.pixel_size_mm = Gaze::GazeVector2(val.x, val.y);
        fixed->set_data(data);
        emit_changed();
    }
}

Vector2 GazeCalibration::get_pixel_size_mm(Object* tracker) const {
    Gaze::GazeVector2 default_val(0.25, 0.25);
    if (tracker) {
        GazeTracker* gt = Object::cast_to<GazeTracker>(tracker);
        if (gt) {
            Vector2 dev_sz = gt->get_pixel_size_mm();
            default_val = Gaze::GazeVector2(dev_sz.x, dev_sz.y);
        }
    }
    Gaze::GazeVector2 res = impl->get_pixel_size_mm(default_val);
    return Vector2(res.x, res.y);
}

void GazeCalibration::set_camera_offset(Vector3 val) {
    auto fixed = std::dynamic_pointer_cast<Gaze::FixedCalibration>(impl);
    if (fixed) {
        Gaze::GazeCalibration data = fixed->get_data();
        data.camera_offset = Gaze::GazeVector3(val.x, val.y, val.z);
        fixed->set_data(data);
        emit_changed();
    }
}

Vector3 GazeCalibration::get_camera_offset(Object* tracker) const {
    Gaze::GazeVector3 default_val(0.0, 148.0, 0.0);
    if (tracker) {
        GazeTracker* gt = Object::cast_to<GazeTracker>(tracker);
        if (gt) {
            Vector3 dev_off = gt->get_derived_camera_offset();
            default_val = Gaze::GazeVector3(dev_off.x, dev_off.y, dev_off.z);
        }
    }
    Gaze::GazeVector3 res = impl->get_camera_offset(default_val);
    return Vector3(res.x, res.y, res.z);
}

void GazeCalibration::set_camera_tilt(double val) {
    auto fixed = std::dynamic_pointer_cast<Gaze::FixedCalibration>(impl);
    if (fixed) {
        Gaze::GazeCalibration data = fixed->get_data();
        data.camera_tilt = val;
        fixed->set_data(data);
        emit_changed();
    }
}

double GazeCalibration::get_camera_tilt(Object* tracker) const {
    double default_val = 0.0;
    if (tracker) {
        GazeTracker* gt = Object::cast_to<GazeTracker>(tracker);
        if (gt) {
            default_val = gt->get_derived_camera_tilt();
        }
    }
    return impl->get_camera_tilt(default_val);
}

void GazeCalibration::set_bias_pitch(double val) {
    auto fixed = std::dynamic_pointer_cast<Gaze::FixedCalibration>(impl);
    if (fixed) {
        Gaze::GazeCalibration data = fixed->get_data();
        data.bias_pitch = val;
        fixed->set_data(data);
    } else {
        auto dev = std::dynamic_pointer_cast<Gaze::DeviceCalibration>(impl);
        if (dev) {
            dev->set_bias_pitch(val);
        }
    }
    emit_changed();
}

double GazeCalibration::get_bias_pitch() const {
    return impl->get_bias_pitch();
}

void GazeCalibration::set_bias_yaw(double val) {
    auto fixed = std::dynamic_pointer_cast<Gaze::FixedCalibration>(impl);
    if (fixed) {
        Gaze::GazeCalibration data = fixed->get_data();
        data.bias_yaw = val;
        fixed->set_data(data);
    } else {
        auto dev = std::dynamic_pointer_cast<Gaze::DeviceCalibration>(impl);
        if (dev) {
            dev->set_bias_yaw(val);
        }
    }
    emit_changed();
}

double GazeCalibration::get_bias_yaw() const {
    return impl->get_bias_yaw();
}

void GazeCalibration::set_bias_pixel_x(double val) {
    auto fixed = std::dynamic_pointer_cast<Gaze::FixedCalibration>(impl);
    if (fixed) {
        Gaze::GazeCalibration data = fixed->get_data();
        data.bias_pixel_x = val;
        fixed->set_data(data);
    } else {
        auto dev = std::dynamic_pointer_cast<Gaze::DeviceCalibration>(impl);
        if (dev) {
            dev->set_bias_pixel_x(val);
        }
    }
    emit_changed();
}

double GazeCalibration::get_bias_pixel_x() const {
    return impl->get_bias_pixel_x();
}

void GazeCalibration::set_bias_pixel_y(double val) {
    auto fixed = std::dynamic_pointer_cast<Gaze::FixedCalibration>(impl);
    if (fixed) {
        Gaze::GazeCalibration data = fixed->get_data();
        data.bias_pixel_y = val;
        fixed->set_data(data);
    } else {
        auto dev = std::dynamic_pointer_cast<Gaze::DeviceCalibration>(impl);
        if (dev) {
            dev->set_bias_pixel_y(val);
        }
    }
    emit_changed();
}

double GazeCalibration::get_bias_pixel_y() const {
    return impl->get_bias_pixel_y();
}


// ==================== GazeDeviceCalibration ====================

GazeDeviceCalibration::GazeDeviceCalibration() {
    impl = std::make_shared<Gaze::DeviceCalibration>();
}

void GazeDeviceCalibration::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_pixel_size_mm_override", "val"), &GazeDeviceCalibration::set_pixel_size_mm_override);
    ClassDB::bind_method(D_METHOD("get_pixel_size_mm_override"), &GazeDeviceCalibration::get_pixel_size_mm_override);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "pixel_size_mm_override"), "set_pixel_size_mm_override", "get_pixel_size_mm_override");

    ClassDB::bind_method(D_METHOD("set_camera_offset_override", "val"), &GazeDeviceCalibration::set_camera_offset_override);
    ClassDB::bind_method(D_METHOD("get_camera_offset_override"), &GazeDeviceCalibration::get_camera_offset_override);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "camera_offset_override"), "set_camera_offset_override", "get_camera_offset_override");

    ClassDB::bind_method(D_METHOD("set_camera_tilt_override", "val"), &GazeDeviceCalibration::set_camera_tilt_override);
    ClassDB::bind_method(D_METHOD("get_camera_tilt_override"), &GazeDeviceCalibration::get_camera_tilt_override);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "camera_tilt_override"), "set_camera_tilt_override", "get_camera_tilt_override");
}

void GazeDeviceCalibration::set_pixel_size_mm_override(Vector2 val) {
    auto dev = get_device_impl();
    if (dev) {
        dev->set_pixel_size_mm_override(Gaze::GazeVector2(val.x, val.y));
        emit_changed();
    }
}

Vector2 GazeDeviceCalibration::get_pixel_size_mm_override() const {
    auto dev = get_device_impl();
    if (dev) {
        Gaze::GazeVector2 res = dev->get_pixel_size_mm_override();
        return Vector2(res.x, res.y);
    }
    return Vector2(-1.0, -1.0);
}

void GazeDeviceCalibration::set_camera_offset_override(Vector3 val) {
    auto dev = get_device_impl();
    if (dev) {
        dev->set_camera_offset_override(Gaze::GazeVector3(val.x, val.y, val.z));
        emit_changed();
    }
}

Vector3 GazeDeviceCalibration::get_camera_offset_override() const {
    auto dev = get_device_impl();
    if (dev) {
        Gaze::GazeVector3 res = dev->get_camera_offset_override();
        return Vector3(res.x, res.y, res.z);
    }
    return Vector3(-1000.0, -1000.0, -1000.0);
}

void GazeDeviceCalibration::set_camera_tilt_override(double val) {
    auto dev = get_device_impl();
    if (dev) {
        dev->set_camera_tilt_override(val);
        emit_changed();
    }
}

double GazeDeviceCalibration::get_camera_tilt_override() const {
    auto dev = get_device_impl();
    if (dev) {
        return dev->get_camera_tilt_override();
    }
    return -1000.0;
}

// ==================== GazeDeviceEstimatedCalibration ====================

GazeDeviceEstimatedCalibration::GazeDeviceEstimatedCalibration() {
    calibration.instantiate();
}

void GazeDeviceEstimatedCalibration::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_calibration"), &GazeDeviceEstimatedCalibration::get_calibration);
}

} // namespace godot
