#include "gaze_display_server_web.hpp"

#ifdef WEB_ENABLED
#include <emscripten.h>

namespace Gaze {

GazeDisplayMetrics gaze_web_get_display_metrics(int screen_index) {
    GazeDisplayMetrics metrics;

    int screen_w = EM_ASM_INT({
        return window.screen ? window.screen.width : 1920;
    });
    int screen_h = EM_ASM_INT({
        return window.screen ? window.screen.height : 1080;
    });
    double dpr = EM_ASM_DOUBLE({
        return window.devicePixelRatio || 1.0;
    });

    metrics.pixel_width = screen_w;
    metrics.pixel_height = screen_h;
    metrics.scale_factor = dpr;

    // Standard CSS 96 DPI pixel pitch (~0.26458 mm/CSS px)
    metrics.width_mm = screen_w * 0.26458;
    metrics.height_mm = screen_h * 0.26458;

    return metrics;
}

GazeWindowRect gaze_web_get_window_rect(int window_index) {
    GazeWindowRect rect;

    rect.x = EM_ASM_INT({
        var canvas = document.querySelector('canvas#canvas') || document.querySelector('canvas');
        if (canvas) {
            var r = canvas.getBoundingClientRect();
            return Math.round(r.left);
        }
        return 0;
    });
    rect.y = EM_ASM_INT({
        var canvas = document.querySelector('canvas#canvas') || document.querySelector('canvas');
        if (canvas) {
            var r = canvas.getBoundingClientRect();
            return Math.round(r.top);
        }
        return 0;
    });
    rect.width = EM_ASM_INT({
        var canvas = document.querySelector('canvas#canvas') || document.querySelector('canvas');
        if (canvas) {
            var r = canvas.getBoundingClientRect();
            return Math.round(r.width);
        }
        return window.innerWidth || 1920;
    });
    rect.height = EM_ASM_INT({
        var canvas = document.querySelector('canvas#canvas') || document.querySelector('canvas');
        if (canvas) {
            var r = canvas.getBoundingClientRect();
            return Math.round(r.height);
        }
        return window.innerHeight || 1080;
    });

    return rect;
}

GazeMousePoint gaze_web_get_mouse_position() {
    GazeMousePoint pt;
    pt.x = EM_ASM_DOUBLE({
        return (window.__gaze_last_mouse_x !== undefined) ? window.__gaze_last_mouse_x : 0.0;
    });
    pt.y = EM_ASM_DOUBLE({
        return (window.__gaze_last_mouse_y !== undefined) ? window.__gaze_last_mouse_y : 0.0;
    });
    return pt;
}

int64_t gaze_web_get_mouse_button_state() {
    return EM_ASM_INT({
        return (window.__gaze_last_mouse_buttons !== undefined) ? window.__gaze_last_mouse_buttons : 0;
    });
}

} // namespace Gaze
#else
namespace Gaze {
GazeDisplayMetrics gaze_web_get_display_metrics(int screen_index) {
    GazeDisplayMetrics metrics;
    metrics.pixel_width = 1920;
    metrics.pixel_height = 1080;
    metrics.scale_factor = 1.0;
    metrics.width_mm = 508.0;
    metrics.height_mm = 285.75;
    return metrics;
}

GazeWindowRect gaze_web_get_window_rect(int window_index) {
    GazeWindowRect rect;
    rect.x = 0;
    rect.y = 0;
    rect.width = 1920;
    rect.height = 1080;
    return rect;
}

GazeMousePoint gaze_web_get_mouse_position() {
    return {0.0, 0.0};
}

int64_t gaze_web_get_mouse_button_state() {
    return 0;
}

} // namespace Gaze
#endif


