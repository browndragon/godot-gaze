#include "gaze_display_fallback.hpp"

namespace Gaze {

GazeDisplayMetrics gaze_fallback_get_display_metrics(int screen_index) {
    GazeDisplayMetrics metrics;
    // Standard desktop monitor fallback (24" 1080p @ 96 DPI: 531mm x 298mm)
    metrics.pixel_width = 1920;
    metrics.pixel_height = 1080;
    metrics.scale_factor = 1.0;
    metrics.width_mm = 531.3;
    metrics.height_mm = 298.8;
    return metrics;
}

GazeWindowRect gaze_fallback_get_window_rect(int window_index) {
    GazeWindowRect rect;
    rect.x = 0;
    rect.y = 0;
    rect.width = 1920;
    rect.height = 1080;
    return rect;
}

GazeMousePoint gaze_fallback_get_mouse_position() {
    return {0.0, 0.0};
}

int64_t gaze_fallback_get_mouse_button_state() {
    return 0;
}

} // namespace Gaze


