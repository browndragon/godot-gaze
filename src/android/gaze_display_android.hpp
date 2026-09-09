#pragma once

#include "../core/gaze_display_types.hpp"

namespace Gaze {

GazeDisplayMetrics gaze_android_get_display_metrics(int screen_index = 0);
GazeWindowRect gaze_android_get_window_rect(int window_index = 0);

} // namespace Gaze

