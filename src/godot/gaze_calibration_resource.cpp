#include "gaze_calibration_resource.hpp"
#include "gaze_server.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include "../core/math_defs.hpp"
#include <godot_cpp/variant/callable.hpp>

namespace godot {

// ==================== DeviceCalibration ====================

void DeviceCalibration::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_physical_size_mm", "size"), &DeviceCalibration::set_physical_size_mm);
    ClassDB::bind_method(D_METHOD("get_physical_size_mm"), &DeviceCalibration::get_physical_size_mm);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "physical_size_mm"), "set_physical_size_mm", "get_physical_size_mm");

    ClassDB::bind_method(D_METHOD("set_logical_size_px", "size"), &DeviceCalibration::set_logical_size_px);
    ClassDB::bind_method(D_METHOD("get_logical_size_px"), &DeviceCalibration::get_logical_size_px);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "logical_size_px"), "set_logical_size_px", "get_logical_size_px");

    ClassDB::bind_method(D_METHOD("set_camera_offset", "val"), &DeviceCalibration::set_camera_offset);
    ClassDB::bind_method(D_METHOD("get_camera_offset"), &DeviceCalibration::get_camera_offset);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "camera_offset"), "set_camera_offset", "get_camera_offset");

    ClassDB::bind_method(D_METHOD("set_camera_tilt", "val"), &DeviceCalibration::set_camera_tilt);
    ClassDB::bind_method(D_METHOD("get_camera_tilt"), &DeviceCalibration::get_camera_tilt);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "camera_tilt"), "set_camera_tilt", "get_camera_tilt");

    ClassDB::bind_method(D_METHOD("get_window_position_lpix"), &DeviceCalibration::get_window_position_lpix);
    ClassDB::bind_method(D_METHOD("get_pixel_size_mm"), &DeviceCalibration::get_pixel_size_mm);
    ClassDB::bind_method(D_METHOD("get_dpi"), &DeviceCalibration::get_dpi);

    ClassDB::bind_static_method("DeviceCalibration", D_METHOD("get_focal_length_under_scaling", "f_original", "original_dim", "new_dim"), &DeviceCalibration::get_focal_length_under_scaling_static);
    ClassDB::bind_static_method("DeviceCalibration", D_METHOD("get_card_width_px", "fov_degrees", "card_distance_mm", "frame_width", "card_width_mm"), &DeviceCalibration::get_card_width_px_static, DEFVAL(85.603));
    ClassDB::bind_static_method("DeviceCalibration", D_METHOD("diagonal_to_horizontal_fov", "diagonal_fov_degrees", "width", "height"), &DeviceCalibration::diagonal_to_horizontal_fov_static);
}

Vector2 DeviceCalibration::get_physical_size_mm() const {
    if (physical_size_mm.x > 0.0 && physical_size_mm.y > 0.0) {
        return physical_size_mm;
    }
    return Vector2(345.0, 215.0);
}

void DeviceCalibration::set_physical_size_mm(Vector2 p_size) {
    physical_size_mm = p_size;
    emit_changed();
}

Vector2i DeviceCalibration::get_logical_size_px() const {
    if (logical_size_px.x > 0 && logical_size_px.y > 0) {
        return logical_size_px;
    }
    return Vector2i(1920, 1080);
}

void DeviceCalibration::set_logical_size_px(Vector2i p_size) {
    logical_size_px = p_size;
    emit_changed();
}

Vector3 DeviceCalibration::get_camera_offset() const {
    if (camera_offset.x > -999.0 && camera_offset.y > -999.0 && camera_offset.z > -999.0) {
        return camera_offset;
    }
    Vector2 phys = get_physical_size_mm();
    return Vector3(0.0, phys.y * 0.5, 0.0);
}

void DeviceCalibration::set_camera_offset(Vector3 p_offset) {
    camera_offset = p_offset;
    emit_changed();
}

double DeviceCalibration::get_camera_tilt() const {
    if (camera_tilt > -999.0) {
        return camera_tilt;
    }
    return 0.0;
}

void DeviceCalibration::set_camera_tilt(double p_tilt) {
    camera_tilt = p_tilt;
    emit_changed();
}

Vector2 DeviceCalibration::get_window_position_lpix() const {
    return Vector2(0.0, 0.0);
}

Vector2 DeviceCalibration::get_pixel_size_mm() const {
    Vector2 phys = get_physical_size_mm();
    Vector2i log_sz = get_logical_size_px();
    if (log_sz.x > 0 && log_sz.y > 0 && phys.x > 0.0 && phys.y > 0.0) {
        return Vector2(phys.x / log_sz.x, phys.y / log_sz.y);
    }
    return Vector2(0.25, 0.25);
}

Vector2 DeviceCalibration::get_dpi() const {
    Vector2 p_sz = get_pixel_size_mm();
    if (p_sz.x > 0.0 && p_sz.y > 0.0) {
        return Vector2(25.4 / p_sz.x, 25.4 / p_sz.y);
    }
    return Vector2(96.0, 96.0);
}

double DeviceCalibration::get_focal_length_under_scaling_static(double f_original, double original_dim, double new_dim) {
    return Gaze::get_focal_length_under_scaling(f_original, original_dim, new_dim);
}

double DeviceCalibration::get_card_width_px_static(double fov_degrees, double card_distance_mm, double frame_width, double card_width_mm) {
    return Gaze::get_card_width_px(fov_degrees, card_distance_mm, frame_width, card_width_mm);
}

double DeviceCalibration::diagonal_to_horizontal_fov_static(double diagonal_fov_degrees, double width, double height) {
    return Gaze::diagonal_to_horizontal_fov(diagonal_fov_degrees, width, height);
}

// ==================== GuessDeviceCalibration ====================

void GuessDeviceCalibration::_bind_methods() {}

// ==================== StoredDeviceCalibration ====================

void StoredDeviceCalibration::_bind_methods() {}

StoredDeviceCalibration::StoredDeviceCalibration() {
    physical_size_mm = Vector2(345.0, 215.0);
    logical_size_px = Vector2i(1920, 1080);
    camera_offset = Vector3(0.0, 107.5, 0.0);
    camera_tilt = 0.0;
}

// ==================== MockDeviceCalibration ====================

void MockDeviceCalibration::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_window_position_lpix", "pos"), &MockDeviceCalibration::set_window_position_lpix);
}

MockDeviceCalibration::MockDeviceCalibration() {
    physical_size_mm = Vector2(345.0, 215.0);
    logical_size_px = Vector2i(1920, 1080);
    camera_offset = Vector3(0.0, 107.5, 0.0);
    camera_tilt = 0.0;
    window_position_px = Vector2(0.0, 0.0);
}

// ==================== DefaultDeviceCalibration ====================

void DefaultDeviceCalibration::_bind_methods() {
    ClassDB::bind_method(D_METHOD("clear_cache"), &DefaultDeviceCalibration::clear_cache);
}

Ref<DeviceCalibration> DefaultDeviceCalibration::get_actual_calibration() const {
    if (cached_calibration.is_valid()) {
        return cached_calibration;
    }
    ProjectSettings* ps = ProjectSettings::get_singleton();
    if (ps) {
        String path = ps->get_setting("gaze/calibration/device_calibration_path");
        if (!path.is_empty() && FileAccess::file_exists(path)) {
            Ref<Resource> res = ResourceLoader::get_singleton()->load(path);
            Ref<StoredDeviceCalibration> stored = res;
            if (stored.is_valid()) {
                cached_calibration = stored;
                if (!cached_calibration->is_connected("changed", Callable(const_cast<DefaultDeviceCalibration*>(this), "emit_changed"))) {
                    cached_calibration->connect("changed", Callable(const_cast<DefaultDeviceCalibration*>(this), "emit_changed"));
                }
                return cached_calibration;
            }
        }
    }
    if (Engine::get_singleton()->has_singleton("GazeDeviceEstimatedCalibration")) {
        GazeDeviceEstimatedCalibration* sing = Object::cast_to<GazeDeviceEstimatedCalibration>(Engine::get_singleton()->get_singleton("GazeDeviceEstimatedCalibration"));
        if (sing && sing->get_calibration().is_valid()) {
            cached_calibration = sing->get_calibration();
            if (!cached_calibration->is_connected("changed", Callable(const_cast<DefaultDeviceCalibration*>(this), "emit_changed"))) {
                cached_calibration->connect("changed", Callable(const_cast<DefaultDeviceCalibration*>(this), "emit_changed"));
            }
            return cached_calibration;
        }
    }
    Ref<GuessDeviceCalibration> guess;
    guess.instantiate();
    cached_calibration = guess;
    if (!cached_calibration->is_connected("changed", Callable(const_cast<DefaultDeviceCalibration*>(this), "emit_changed"))) {
        cached_calibration->connect("changed", Callable(const_cast<DefaultDeviceCalibration*>(this), "emit_changed"));
    }
    return cached_calibration;
}

void DefaultDeviceCalibration::clear_cache() {
    if (cached_calibration.is_valid()) {
        if (cached_calibration->is_connected("changed", Callable(this, "emit_changed"))) {
            cached_calibration->disconnect("changed", Callable(this, "emit_changed"));
        }
        cached_calibration.unref();
    }
    emit_changed();
}

Vector2 DefaultDeviceCalibration::get_physical_size_mm() const {
    return get_actual_calibration()->get_physical_size_mm();
}

void DefaultDeviceCalibration::set_physical_size_mm(Vector2 p_size) {
    get_actual_calibration()->set_physical_size_mm(p_size);
}

Vector2i DefaultDeviceCalibration::get_logical_size_px() const {
    return get_actual_calibration()->get_logical_size_px();
}

void DefaultDeviceCalibration::set_logical_size_px(Vector2i p_size) {
    get_actual_calibration()->set_logical_size_px(p_size);
}

Vector3 DefaultDeviceCalibration::get_camera_offset() const {
    return get_actual_calibration()->get_camera_offset();
}

void DefaultDeviceCalibration::set_camera_offset(Vector3 p_offset) {
    get_actual_calibration()->set_camera_offset(p_offset);
}

double DefaultDeviceCalibration::get_camera_tilt() const {
    return get_actual_calibration()->get_camera_tilt();
}

void DefaultDeviceCalibration::set_camera_tilt(double p_tilt) {
    get_actual_calibration()->set_camera_tilt(p_tilt);
}

Vector2 DefaultDeviceCalibration::get_window_position_lpix() const {
    return get_actual_calibration()->get_window_position_lpix();
}

// ==================== BioCalibration ====================

void BioCalibration::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_bias_pitch", "val"), &BioCalibration::set_bias_pitch);
    ClassDB::bind_method(D_METHOD("get_bias_pitch"), &BioCalibration::get_bias_pitch);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bias_pitch"), "set_bias_pitch", "get_bias_pitch");

    ClassDB::bind_method(D_METHOD("set_bias_yaw", "val"), &BioCalibration::set_bias_yaw);
    ClassDB::bind_method(D_METHOD("get_bias_yaw"), &BioCalibration::get_bias_yaw);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bias_yaw"), "set_bias_yaw", "get_bias_yaw");

    ClassDB::bind_method(D_METHOD("set_scale_pitch", "val"), &BioCalibration::set_scale_pitch);
    ClassDB::bind_method(D_METHOD("get_scale_pitch"), &BioCalibration::get_scale_pitch);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale_pitch"), "set_scale_pitch", "get_scale_pitch");

    ClassDB::bind_method(D_METHOD("set_scale_yaw", "val"), &BioCalibration::set_scale_yaw);
    ClassDB::bind_method(D_METHOD("get_scale_yaw"), &BioCalibration::get_scale_yaw);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale_yaw"), "set_scale_yaw", "get_scale_yaw");
}

// ==================== DefaultBioCalibration ====================

void DefaultBioCalibration::_bind_methods() {
    ClassDB::bind_method(D_METHOD("clear_cache"), &DefaultBioCalibration::clear_cache);
}

Ref<BioCalibration> DefaultBioCalibration::get_actual_calibration() const {
    if (cached_calibration.is_valid()) {
        return cached_calibration;
    }
    ProjectSettings* ps = ProjectSettings::get_singleton();
    if (ps) {
        String path = ps->get_setting("gaze/calibration/bio_calibration_path");
        if (!path.is_empty() && FileAccess::file_exists(path)) {
            Ref<Resource> res = ResourceLoader::get_singleton()->load(path);
            Ref<StoredBioCalibration> stored = res;
            if (stored.is_valid()) {
                cached_calibration = stored;
                if (!cached_calibration->is_connected("changed", Callable(const_cast<DefaultBioCalibration*>(this), "emit_changed"))) {
                    cached_calibration->connect("changed", Callable(const_cast<DefaultBioCalibration*>(this), "emit_changed"));
                }
                return cached_calibration;
            }
        }
    }
    Ref<GuessBioCalibration> guess;
    guess.instantiate();
    cached_calibration = guess;
    if (!cached_calibration->is_connected("changed", Callable(const_cast<DefaultBioCalibration*>(this), "emit_changed"))) {
        cached_calibration->connect("changed", Callable(const_cast<DefaultBioCalibration*>(this), "emit_changed"));
    }
    return cached_calibration;
}

void DefaultBioCalibration::clear_cache() {
    if (cached_calibration.is_valid()) {
        if (cached_calibration->is_connected("changed", Callable(this, "emit_changed"))) {
            cached_calibration->disconnect("changed", Callable(this, "emit_changed"));
        }
        cached_calibration.unref();
    }
    emit_changed();
}

double DefaultBioCalibration::get_bias_pitch() const {
    return get_actual_calibration()->get_bias_pitch();
}

double DefaultBioCalibration::get_bias_yaw() const {
    return get_actual_calibration()->get_bias_yaw();
}

double DefaultBioCalibration::get_scale_pitch() const {
    return get_actual_calibration()->get_scale_pitch();
}

double DefaultBioCalibration::get_scale_yaw() const {
    return get_actual_calibration()->get_scale_yaw();
}

void DefaultBioCalibration::set_bias_pitch(double val) {
    get_actual_calibration()->set_bias_pitch(val);
}

void DefaultBioCalibration::set_bias_yaw(double val) {
    get_actual_calibration()->set_bias_yaw(val);
}

void DefaultBioCalibration::set_scale_pitch(double val) {
    get_actual_calibration()->set_scale_pitch(val);
}

void DefaultBioCalibration::set_scale_yaw(double val) {
    get_actual_calibration()->set_scale_yaw(val);
}

// ==================== GazeDeviceEstimatedCalibration ====================

GazeDeviceEstimatedCalibration::GazeDeviceEstimatedCalibration() {
    Ref<GuessDeviceCalibration> guess;
    guess.instantiate();
    calibration = guess;
}

void GazeDeviceEstimatedCalibration::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_calibration"), &GazeDeviceEstimatedCalibration::get_calibration);
}

} // namespace godot
