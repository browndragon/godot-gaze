#pragma once

#include <cstdint>

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

/**
 * @struct GazeMousePoint
 * @brief Represents position of mouse cursor on screen in logical pixels.
 */
struct GazeMousePoint {
    double x = 0.0;
    double y = 0.0;
};

} // namespace Gaze

