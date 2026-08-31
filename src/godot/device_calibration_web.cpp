/**
 * @file device_calibration_web.cpp
 * @brief Web/WASM implementation of DOM-derived DeviceCalibration strategies
 */
#ifdef WEB_ENABLED

#include "gaze_calibration_resource.hpp"
#include <emscripten.h>

namespace godot {

static inline double em_eval_float(const char *code) {
    return emscripten_run_script_float(code);
}

Vector2 GuessDeviceCalibration::get_physical_size_mm() const {
    if (physical_size_mm.x > 0.0 && physical_size_mm.y > 0.0) {
        return physical_size_mm;
    }
    double w_lpix = em_eval_float("window.screen.width");
    double h_lpix = em_eval_float("window.screen.height");
    double dpr = em_eval_float("window.devicePixelRatio");
    if (dpr <= 0.0) dpr = 1.0;

    if (w_lpix > 0.0 && h_lpix > 0.0) {
        double dpi_lpix = 172.0 - 0.03 * w_lpix;
        if (dpi_lpix < 96.0) dpi_lpix = 96.0;
        double dpi = dpi_lpix * dpr;
        double size_ppix_x = w_lpix * dpr;
        double size_ppix_y = h_lpix * dpr;
        return Vector2((size_ppix_x / dpi) * 25.4, (size_ppix_y / dpi) * 25.4);
    }
    return Vector2(345.0, 215.0);
}

Vector2i GuessDeviceCalibration::get_logical_size_px() const {
    if (logical_size_px.x > 0 && logical_size_px.y > 0) {
        return logical_size_px;
    }
    double w_lpix = em_eval_float("window.screen.width");
    double h_lpix = em_eval_float("window.screen.height");
    if (w_lpix > 0.0 && h_lpix > 0.0) {
        return Vector2i((int)w_lpix, (int)h_lpix);
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
    double left = em_eval_float("(function() { var canvas = (typeof Module !== 'undefined' && Module.canvas) || document.getElementById('canvas') || document.querySelector('canvas'); return canvas ? (window.screenX + canvas.getBoundingClientRect().left) : window.screenX; })()");
    double top = em_eval_float("(function() { var canvas = (typeof Module !== 'undefined' && Module.canvas) || document.getElementById('canvas') || document.querySelector('canvas'); return canvas ? (window.screenY + canvas.getBoundingClientRect().top) : window.screenY; })()");
    return Vector2(left, top);
}

Vector2 StoredDeviceCalibration::get_window_position() const {
    double left = em_eval_float("(function() { var canvas = (typeof Module !== 'undefined' && Module.canvas) || document.getElementById('canvas') || document.querySelector('canvas'); return canvas ? (window.screenX + canvas.getBoundingClientRect().left) : window.screenX; })()");
    double top = em_eval_float("(function() { var canvas = (typeof Module !== 'undefined' && Module.canvas) || document.getElementById('canvas') || document.querySelector('canvas'); return canvas ? (window.screenY + canvas.getBoundingClientRect().top) : window.screenY; })()");
    return Vector2(left, top);
}

} // namespace godot

#endif // WEB_ENABLED
