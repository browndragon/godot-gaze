/**
 * @file device_calibration_native.cpp
 * @brief Native desktop implementation of OS-derived DeviceCalibration strategies
 */
#ifndef WEB_ENABLED

#include "gaze_calibration_resource.hpp"
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/core/error_macros.hpp>

namespace godot {

Vector2 GuessDeviceCalibration::get_physical_size_mm() const {
    if (physical_size_mm.x > 0.0 && physical_size_mm.y > 0.0) {
        return physical_size_mm;
    }
    DisplayServer* ds = DisplayServer::get_singleton();
    if (ds) {
        int screen_id = ds->window_get_current_screen();
        Vector2i size_ppix = ds->screen_get_size(screen_id);
        double scale = ds->screen_get_scale(screen_id);
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
                return Vector2((size_ppix.x / dpi) * 25.4, (size_ppix.y / dpi) * 25.4);
            }
        }
    }
    return Vector2(345.0, 215.0);
}

Vector2i GuessDeviceCalibration::get_logical_size_px() const {
    if (logical_size_px.x > 0 && logical_size_px.y > 0) {
        return logical_size_px;
    }
    DisplayServer* ds = DisplayServer::get_singleton();
    if (ds) {
        int screen_id = ds->window_get_current_screen();
        Vector2i size_ppix = ds->screen_get_size(screen_id);
        double scale = ds->screen_get_scale(screen_id);
        if (size_ppix.x > 0 && size_ppix.y > 0 && scale > 0.0) {
            return Vector2i((int)(size_ppix.x / scale), (int)(size_ppix.y / scale));
        }
    }
    return Vector2i(1920, 1080);
}

Vector3 GuessDeviceCalibration::get_camera_offset() const {
    if (camera_offset.x > -999.0 && camera_offset.y > -999.0 && camera_offset.z > -999.0) {
        return camera_offset;
    }
    return Vector3(0.0, 0.0, 0.0);
}

double GuessDeviceCalibration::get_camera_tilt() const {
    if (camera_tilt > -999.0) {
        return camera_tilt;
    }
    return 0.0;
}

Vector2 GuessDeviceCalibration::get_window_position() const {
    DisplayServer* ds = DisplayServer::get_singleton();
    ERR_FAIL_COND_V_MSG(!ds, Vector2(), "DisplayServer is missing; tests must inject MockDeviceCalibration!");
    int screen_id = ds->window_get_current_screen();
    double scale = ds->screen_get_scale(screen_id);
    if (scale <= 0.0) scale = 1.0;
    Vector2i pos_i = ds->window_get_position();
    return Vector2(pos_i.x / scale, pos_i.y / scale);
}

Vector2 StoredDeviceCalibration::get_window_position() const {
    DisplayServer* ds = DisplayServer::get_singleton();
    if (ds) {
        int screen_id = ds->window_get_current_screen();
        double scale = ds->screen_get_scale(screen_id);
        if (scale <= 0.0) scale = 1.0;
        Vector2i pos_i = ds->window_get_position();
        return Vector2(pos_i.x / scale, pos_i.y / scale);
    }
    return Vector2(0.0, 0.0);
}

} // namespace godot

#endif // WEB_ENABLED
