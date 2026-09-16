#include "gaze_display_android.hpp"

namespace Gaze {

GazeDisplayMetrics gaze_android_get_display_metrics(int screen_index) {
    GazeDisplayMetrics metrics;
    // Standard default mobile display parameters (e.g. 1080x2400 @ ~400 DPI)
    metrics.pixel_width = 1080;
    metrics.pixel_height = 2400;
    metrics.scale_factor = 2.625;
    
    // ~400 DPI -> ~0.0635 mm/pixel -> ~68.5mm x ~152.4mm
    metrics.width_mm = 68.58;
    metrics.height_mm = 152.40;
    return metrics;
}

GazeWindowRect gaze_android_get_window_rect(int window_index) {
    GazeWindowRect rect;
    GazeDisplayMetrics metrics = gaze_android_get_display_metrics(0);
    rect.x = 0;
    rect.y = 0;
    rect.width = metrics.pixel_width;
    rect.height = metrics.pixel_height;
    return rect;
}

GazeMousePoint gaze_android_get_mouse_position() {
    return {0.0, 0.0};
}

int64_t gaze_android_get_mouse_button_state() {
    return 0;
}

} // namespace Gaze


