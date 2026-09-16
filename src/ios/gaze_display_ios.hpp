#pragma once

#include "../core/gaze_display_types.hpp"

namespace Gaze {

GazeDisplayMetrics gaze_ios_get_display_metrics(int screen_index = 0);
GazeWindowRect gaze_ios_get_window_rect(int window_index = 0);
GazeMousePoint gaze_ios_get_mouse_position();
int64_t gaze_ios_get_mouse_button_state();

} // namespace Gaze

