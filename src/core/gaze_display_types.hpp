#pragma once

namespace Gaze {

/**
 * @struct GazeDisplayMetrics
 * @brief Represents physical dimensions and logical resolution of a display.
 */
struct GazeDisplayMetrics {
    double width_mm = 0.0;
    double height_mm = 0.0;
    int pixel_width = 0;
    int pixel_height = 0;
    double scale_factor = 1.0;
};

/**
 * @struct GazeWindowRect
 * @brief Represents position and dimensions of an application window in logical pixels.
 */
struct GazeWindowRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

} // namespace Gaze

