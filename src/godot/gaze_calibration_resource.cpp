#include "gaze_calibration_resource.hpp"
#include "gaze_tracker.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include "display_profile.hpp"
#include "../core/math_defs.hpp"

#include <godot_cpp/variant/callable.hpp>

namespace godot {

// ==================== DeviceCalibration ====================

void DeviceCalibration::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_pixel_size_mm", "val"), &DeviceCalibration::set_pixel_size_mm);
    ClassDB::bind_method(D_METHOD("get_pixel_size_mm", "tracker"), &DeviceCalibration::get_pixel_size_mm, DEFVAL(nullptr));
    ClassDB::bind_method(D_METHOD("get_pixel_size_mm_bind"), &DeviceCalibration::get_pixel_size_mm_bind);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "pixel_size_mm"), "set_pixel_size_mm", "get_pixel_size_mm_bind");

    ClassDB::bind_method(D_METHOD("set_camera_offset", "val"), &DeviceCalibration::set_camera_offset);
    ClassDB::bind_method(D_METHOD("get_camera_offset", "tracker"), &DeviceCalibration::get_camera_offset, DEFVAL(nullptr));
    ClassDB::bind_method(D_METHOD("get_camera_offset_bind"), &DeviceCalibration::get_camera_offset_bind);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "camera_offset"), "set_camera_offset", "get_camera_offset_bind");

    ClassDB::bind_method(D_METHOD("set_camera_tilt", "val"), &DeviceCalibration::set_camera_tilt);
    ClassDB::bind_method(D_METHOD("get_camera_tilt", "tracker"), &DeviceCalibration::get_camera_tilt, DEFVAL(nullptr));
    ClassDB::bind_method(D_METHOD("get_camera_tilt_bind"), &DeviceCalibration::get_camera_tilt_bind);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "camera_tilt"), "set_camera_tilt", "get_camera_tilt_bind");

    ClassDB::bind_static_method("DeviceCalibration", D_METHOD("get_focal_length_under_scaling", "f_original", "original_dim", "new_dim"), &DeviceCalibration::get_focal_length_under_scaling_static);
    ClassDB::bind_static_method("DeviceCalibration", D_METHOD("get_card_width_px", "fov_degrees", "card_distance_mm", "frame_width", "card_width_mm"), &DeviceCalibration::get_card_width_px_static, DEFVAL(85.603));
    ClassDB::bind_static_method("DeviceCalibration", D_METHOD("diagonal_to_horizontal_fov", "diagonal_fov_degrees", "width", "height"), &DeviceCalibration::diagonal_to_horizontal_fov_static);
}

Vector2 DeviceCalibration::get_pixel_size_mm(Object* tracker) const {
    return pixel_size_mm;
}

Vector3 DeviceCalibration::get_camera_offset(Object* tracker) const {
    return camera_offset;
}

double DeviceCalibration::get_camera_tilt(Object* tracker) const {
    return camera_tilt;
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

Vector2 GuessDeviceCalibration::get_pixel_size_mm(Object* tracker) const {
    if (pixel_size_mm.x > 0.0 && pixel_size_mm.y > 0.0) {
        return pixel_size_mm;
    }
    if (tracker) {
        GazeTracker* gt = Object::cast_to<GazeTracker>(tracker);
        if (gt) {
            Ref<DisplayProfile> dp = gt->get_display_profile();
            if (dp.is_valid()) {
                Vector2i size_px = dp->get_logical_size_px();
                Vector2 size_mm = dp->get_physical_size_mm();
                if (size_px.x > 0 && size_px.y > 0 && size_mm.x > 0.0 && size_mm.y > 0.0) {
                    return Vector2(size_mm.x / size_px.x, size_mm.y / size_px.y);
                }
            }
            return gt->get_pixel_size_mm();
        }
    }
    DisplayServer* ds = DisplayServer::get_singleton();
    if (ds) {
        int screen_id = ds->window_get_current_screen();
        double scale = ds->screen_get_scale(screen_id);
        Vector2i size_ppix = ds->screen_get_size(screen_id);
        if (size_ppix.x > 0 && size_ppix.y > 0) {
            Vector2i size_lpix = Vector2i((int)(size_ppix.x / scale), (int)(size_ppix.y / scale));
            double dpi = ds->screen_get_dpi(screen_id);
            if (dpi < 120.0 || dpi <= 0.0) {
                double w_lpix = size_lpix.x;
                double dpi_lpix = 172.0 - 0.03 * w_lpix;
                if (dpi_lpix < 96.0) {
                    dpi_lpix = 96.0;
                }
                dpi = dpi_lpix * scale;
            }
            if (dpi > 0.0) {
                Vector2 size_mm = Vector2((size_ppix.x / dpi) * 25.4, (size_ppix.y / dpi) * 25.4);
                return Vector2(size_mm.x / size_lpix.x, size_mm.y / size_lpix.y);
            }
        }
    }
    return Vector2(0.25, 0.25);
}

Vector3 GuessDeviceCalibration::get_camera_offset(Object* tracker) const {
    if (camera_offset.x > -999.0 && camera_offset.y > -999.0 && camera_offset.z > -999.0) {
        return camera_offset;
    }
    if (tracker) {
        GazeTracker* gt = Object::cast_to<GazeTracker>(tracker);
        if (gt) {
            return gt->get_derived_camera_offset();
        }
    }
    return Vector3(0.0, 148.0, 0.0);
}

double GuessDeviceCalibration::get_camera_tilt(Object* tracker) const {
    if (camera_tilt > -999.0) {
        return camera_tilt;
    }
    if (tracker) {
        GazeTracker* gt = Object::cast_to<GazeTracker>(tracker);
        if (gt) {
            return gt->get_derived_camera_tilt();
        }
    }
    return 0.0;
}

// ==================== StoredDeviceCalibration ====================

StoredDeviceCalibration::StoredDeviceCalibration() {
    pixel_size_mm = Vector2(0.25, 0.25);
    camera_offset = Vector3(0.0, 148.0, 0.0);
    camera_tilt = 0.0;
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
    GazeDeviceEstimatedCalibration* sing = Object::cast_to<GazeDeviceEstimatedCalibration>(Engine::get_singleton()->get_singleton("GazeDeviceEstimatedCalibration"));
    if (sing && sing->get_calibration().is_valid()) {
        cached_calibration = sing->get_calibration();
        if (!cached_calibration->is_connected("changed", Callable(const_cast<DefaultDeviceCalibration*>(this), "emit_changed"))) {
            cached_calibration->connect("changed", Callable(const_cast<DefaultDeviceCalibration*>(this), "emit_changed"));
        }
        return cached_calibration;
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

Vector2 DefaultDeviceCalibration::get_pixel_size_mm(Object* tracker) const {
    return get_actual_calibration()->get_pixel_size_mm(tracker);
}

Vector3 DefaultDeviceCalibration::get_camera_offset(Object* tracker) const {
    return get_actual_calibration()->get_camera_offset(tracker);
}

double DefaultDeviceCalibration::get_camera_tilt(Object* tracker) const {
    return get_actual_calibration()->get_camera_tilt(tracker);
}

void DefaultDeviceCalibration::set_pixel_size_mm(Vector2 val) {
    get_actual_calibration()->set_pixel_size_mm(val);
}

void DefaultDeviceCalibration::set_camera_offset(Vector3 val) {
    get_actual_calibration()->set_camera_offset(val);
}

void DefaultDeviceCalibration::set_camera_tilt(double val) {
    get_actual_calibration()->set_camera_tilt(val);
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
