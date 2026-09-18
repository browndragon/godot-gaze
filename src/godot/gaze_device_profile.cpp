/**
 * @file gaze_device_profile.cpp
 * @brief Implement GazeDeviceProfile resource
 */
#include "gaze_device_profile.hpp"
#include "gaze_display_server.hpp"
#include "../core/math_defs.hpp"
#include "../core/projection_engine.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <cmath>

namespace godot {

void GazeDeviceProfile::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_pixel_pitch_mm", "pitch"), &GazeDeviceProfile::set_pixel_pitch_mm);
    ClassDB::bind_method(D_METHOD("get_pixel_pitch_mm"), &GazeDeviceProfile::get_pixel_pitch_mm);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "pixel_pitch_mm"), "set_pixel_pitch_mm", "get_pixel_pitch_mm");

    ClassDB::bind_method(D_METHOD("set_logical_size_px", "size"), &GazeDeviceProfile::set_logical_size_px);
    ClassDB::bind_method(D_METHOD("get_logical_size_px"), &GazeDeviceProfile::get_logical_size_px);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "logical_size_px"), "set_logical_size_px", "get_logical_size_px");

    ClassDB::bind_method(D_METHOD("set_camera_offset_mm", "offset"), &GazeDeviceProfile::set_camera_offset_mm);
    ClassDB::bind_method(D_METHOD("get_camera_offset_mm"), &GazeDeviceProfile::get_camera_offset_mm);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "camera_offset_mm"), "set_camera_offset_mm", "get_camera_offset_mm");

    ClassDB::bind_method(D_METHOD("set_camera_roll_deg", "roll"), &GazeDeviceProfile::set_camera_roll_deg);
    ClassDB::bind_method(D_METHOD("get_camera_roll_deg"), &GazeDeviceProfile::get_camera_roll_deg);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "camera_roll_deg"), "set_camera_roll_deg", "get_camera_roll_deg");

    ClassDB::bind_method(D_METHOD("set_camera_tilt_deg", "tilt"), &GazeDeviceProfile::set_camera_tilt_deg);
    ClassDB::bind_method(D_METHOD("get_camera_tilt_deg"), &GazeDeviceProfile::get_camera_tilt_deg);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "camera_tilt_deg"), "set_camera_tilt_deg", "get_camera_tilt_deg");

    ClassDB::bind_method(D_METHOD("set_camera_hfov_deg", "hfov"), &GazeDeviceProfile::set_camera_hfov_deg);
    ClassDB::bind_method(D_METHOD("get_camera_hfov_deg"), &GazeDeviceProfile::get_camera_hfov_deg);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "camera_hfov_deg"), "set_camera_hfov_deg", "get_camera_hfov_deg");

    ClassDB::bind_method(D_METHOD("set_physical_size_mm", "size"), &GazeDeviceProfile::set_physical_size_mm);
    ClassDB::bind_method(D_METHOD("get_physical_size_mm"), &GazeDeviceProfile::get_physical_size_mm);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "physical_size_mm"), "set_physical_size_mm", "get_physical_size_mm");

    ClassDB::bind_method(D_METHOD("get_dpi"), &GazeDeviceProfile::get_dpi);
    ClassDB::bind_method(D_METHOD("get_focal_length_px", "frame_width_px"), &GazeDeviceProfile::get_focal_length_px);
    ClassDB::bind_method(D_METHOD("calibrate_from_card_width", "card_width_lpix", "card_width_mm"), &GazeDeviceProfile::calibrate_from_card_width, DEFVAL(85.603));

    ClassDB::bind_method(D_METHOD("project_gaze_px", "origin_cam_mm", "direction_cam"), &GazeDeviceProfile::project_gaze_px);
    ClassDB::bind_method(D_METHOD("unproject_px_to_cam_mm", "screen_px"), &GazeDeviceProfile::unproject_px_to_cam_mm);

    ClassDB::bind_static_method("GazeDeviceProfile", D_METHOD("create_system_guess"), &GazeDeviceProfile::create_system_guess);
    ClassDB::bind_static_method("GazeDeviceProfile", D_METHOD("get_focal_length_under_scaling", "f_original", "original_dim", "new_dim"), &GazeDeviceProfile::get_focal_length_under_scaling_static);
    ClassDB::bind_static_method("GazeDeviceProfile", D_METHOD("get_card_width_px", "fov_degrees", "card_distance_mm", "frame_width", "card_width_mm"), &GazeDeviceProfile::get_card_width_px_static, DEFVAL(85.603));
    ClassDB::bind_static_method("GazeDeviceProfile", D_METHOD("diagonal_to_horizontal_fov", "diagonal_fov_degrees", "width", "height"), &GazeDeviceProfile::diagonal_to_horizontal_fov_static);
}

void GazeDeviceProfile::set_pixel_pitch_mm(Vector2 p_pitch) {
    pixel_pitch_mm = p_pitch;
    emit_changed();
}

void GazeDeviceProfile::set_logical_size_px(Vector2i p_size) {
    logical_size_px = p_size;
    emit_changed();
}

void GazeDeviceProfile::set_camera_offset_mm(Vector3 p_offset) {
    camera_offset_mm = p_offset;
    emit_changed();
}

void GazeDeviceProfile::set_camera_roll_deg(double p_roll) {
    camera_roll_deg = p_roll;
    emit_changed();
}

void GazeDeviceProfile::set_camera_hfov_deg(double p_hfov) {
    camera_hfov_deg = p_hfov;
    emit_changed();
}

Vector2 GazeDeviceProfile::get_physical_size_mm() const {
    return Vector2(logical_size_px.x * pixel_pitch_mm.x, logical_size_px.y * pixel_pitch_mm.y);
}

void GazeDeviceProfile::set_physical_size_mm(Vector2 p_size) {
    if (logical_size_px.x > 0 && logical_size_px.y > 0 && p_size.x > 0.0 && p_size.y > 0.0) {
        pixel_pitch_mm = Vector2(p_size.x / logical_size_px.x, p_size.y / logical_size_px.y);
        emit_changed();
    }
}

Vector2 GazeDeviceProfile::get_dpi() const {
    if (pixel_pitch_mm.x > 0.0 && pixel_pitch_mm.y > 0.0) {
        return Vector2(25.4 / pixel_pitch_mm.x, 25.4 / pixel_pitch_mm.y);
    }
    return Vector2(96.0, 96.0);
}

double GazeDeviceProfile::get_focal_length_px(double frame_width_px) const {
    if (frame_width_px <= 0.0 || camera_hfov_deg <= 0.0 || camera_hfov_deg >= 180.0) {
        return 1000.0;
    }
    return frame_width_px / (2.0 * std::tan(camera_hfov_deg * Gaze::DEG_TO_RAD * 0.5));
}

void GazeDeviceProfile::calibrate_from_card_width(double card_width_lpix, double card_width_mm) {
    if (card_width_lpix > 0.0 && card_width_mm > 0.0) {
        double pitch = card_width_mm / card_width_lpix;
        pixel_pitch_mm = Vector2(pitch, pitch);
        emit_changed();
    }
}

Ref<GazeDeviceProfile> GazeDeviceProfile::create_system_guess() {
    Ref<GazeDeviceProfile> profile;
    profile.instantiate();

    GazeDisplayServer* gds = GazeDisplayServer::get_singleton();
    if (gds) {
        Vector2 size_px = gds->get_screen_size_pixels();
        Vector2 pitch = gds->get_pixel_pitch_mm();
        Vector3 cam_offset = gds->get_default_camera_offset_mm();

        profile->set_logical_size_px(Vector2i((int)size_px.x, (int)size_px.y));
        profile->set_pixel_pitch_mm(pitch);
        profile->set_camera_offset_mm(cam_offset);
    } else {
        profile->set_logical_size_px(Vector2i(1920, 1080));
        profile->set_pixel_pitch_mm(Vector2(0.26458, 0.26458));
        profile->set_camera_offset_mm(Vector3(0.0, 0.0, 0.0));
    }

    profile->set_camera_roll_deg(0.0);
    profile->set_camera_tilt_deg(0.0);
    profile->set_camera_hfov_deg(65.0);

    return profile;
}

void GazeDeviceProfile::set_camera_tilt_deg(double p_tilt) {
    camera_tilt_deg = p_tilt;
    emit_changed();
}

Vector2 GazeDeviceProfile::project_gaze_px(const Vector3 &p_origin_cam_mm, const Vector3 &p_direction_cam) const {
    Gaze::ProjectionEngine engine;
    Vector2 phys_sz = get_physical_size_mm();
    engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(phys_sz.x, phys_sz.y));
    engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(logical_size_px.x, logical_size_px.y));
    engine.set_camera_placement(Gaze::CameraPlacement(
        Gaze::GodotCameraVector3(camera_offset_mm.x, camera_offset_mm.y, camera_offset_mm.z),
        camera_tilt_deg
    ));

    Gaze::GodotCameraVector3 orig(p_origin_cam_mm.x, p_origin_cam_mm.y, p_origin_cam_mm.z);
    Gaze::GodotCameraVector3 dir(p_direction_cam.x, p_direction_cam.y, p_direction_cam.z);
    Gaze::GodotDisplayVector2 out_px;
    if (!engine.project_gaze(orig, dir, out_px)) {
        return Vector2(INFINITY, INFINITY);
    }
    return Vector2(out_px.x, out_px.y);
}

Vector3 GazeDeviceProfile::unproject_px_to_cam_mm(const Vector2 &p_screen_px) const {
    Gaze::ProjectionEngine engine;
    Vector2 phys_sz = get_physical_size_mm();
    engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(phys_sz.x, phys_sz.y));
    engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(logical_size_px.x, logical_size_px.y));
    engine.set_camera_placement(Gaze::CameraPlacement(
        Gaze::GodotCameraVector3(camera_offset_mm.x, camera_offset_mm.y, camera_offset_mm.z),
        camera_tilt_deg
    ));

    Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm> mm = engine.pixel_to_millimeter(
        Gaze::GodotDisplayVector2(p_screen_px.x, p_screen_px.y)
    );
    Gaze::GodotCameraVector3 pt_cam = engine.screen_mm_to_camera_space(mm);
    return Vector3(pt_cam.x, pt_cam.y, pt_cam.z);
}

void GazeDeviceProfile::_write_to_config(Ref<ConfigFile> &p_cfg) const {
    p_cfg->set_value("device_profile", "pixel_pitch_mm", pixel_pitch_mm);
    p_cfg->set_value("device_profile", "logical_size_px", logical_size_px);
    p_cfg->set_value("device_profile", "camera_offset_mm", camera_offset_mm);
    p_cfg->set_value("device_profile", "camera_roll_deg", camera_roll_deg);
    p_cfg->set_value("device_profile", "camera_tilt_deg", camera_tilt_deg);
    p_cfg->set_value("device_profile", "camera_hfov_deg", camera_hfov_deg);
}

Error GazeDeviceProfile::_read_from_config(const Ref<ConfigFile> &p_cfg) {
    if (!p_cfg->has_section("device_profile")) {
        return ERR_FILE_CORRUPT;
    }
    set_pixel_pitch_mm(p_cfg->get_value("device_profile", "pixel_pitch_mm", pixel_pitch_mm));
    set_logical_size_px(p_cfg->get_value("device_profile", "logical_size_px", logical_size_px));
    set_camera_offset_mm(p_cfg->get_value("device_profile", "camera_offset_mm", camera_offset_mm));
    set_camera_roll_deg(p_cfg->get_value("device_profile", "camera_roll_deg", camera_roll_deg));
    set_camera_tilt_deg(p_cfg->get_value("device_profile", "camera_tilt_deg", camera_tilt_deg));
    set_camera_hfov_deg(p_cfg->get_value("device_profile", "camera_hfov_deg", camera_hfov_deg));
    return OK;
}

double GazeDeviceProfile::get_focal_length_under_scaling_static(double f_original, double original_dim, double new_dim) {
    return Gaze::get_focal_length_under_scaling(f_original, original_dim, new_dim);
}

double GazeDeviceProfile::get_card_width_px_static(double fov_degrees, double card_distance_mm, double frame_width, double card_width_mm) {
    return Gaze::get_card_width_px(fov_degrees, card_distance_mm, frame_width, card_width_mm);
}

double GazeDeviceProfile::diagonal_to_horizontal_fov_static(double diagonal_fov_degrees, double width, double height) {
    return Gaze::diagonal_to_horizontal_fov(diagonal_fov_degrees, width, height);
}

} // namespace godot
