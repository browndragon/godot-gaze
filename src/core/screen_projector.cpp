/**
 * @file screen_projector.cpp
 * @brief Implement screen projector coordinate transformation math (Layer 4)
 */
#include "screen_projector.hpp"
#include <cmath>

namespace Gaze {

ScreenProjector ScreenProjector::derive_configuration(
    const GazeVector2& win_pos_px,
    const GazeVector2& vp_scale,
    const GazeVector2& vp_origin
) {
    return ScreenProjector(win_pos_px, vp_scale, vp_origin);
}

ScreenProjector ScreenProjector::from_godot_geometry(
    const GazeVector2& window_pos_logical,
    const GazeVector2& viewport_scale_logical,
    const GazeVector2& viewport_offset_logical
) {
    return ScreenProjector(
        window_pos_logical,
        viewport_scale_logical,
        viewport_offset_logical
    );
}

bool ScreenProjector::project_to_viewport(
    const ProjectionEngine& engine,
    const GazeVector3& origin_cam,
    const GazeVector3& dir_cam,
    GazeVector2& out_viewport_pixel
) const {
    GazeVector2 screen_pixel;
    
    // 1. Project 3D gaze ray onto screen plane in logical screen pixels
    if (!engine.project_gaze(origin_cam, dir_cam, screen_pixel)) {
        return false;
    }

    // 2. Subtract window position to get window-local logical pixels
    double local_x = screen_pixel.x - window_position_px.x;
    double local_y = screen_pixel.y - window_position_px.y;

    // 3. Map window-local logical pixels to logical viewport coordinates
    if (std::abs(viewport_scale.x) < 1e-6 || std::abs(viewport_scale.y) < 1e-6) {
        return false;
    }

    out_viewport_pixel.x = (local_x - viewport_offset_px.x) / viewport_scale.x;
    out_viewport_pixel.y = (local_y - viewport_offset_px.y) / viewport_scale.y;

    return true;
}

GazeVector2 ScreenProjector::map_viewport_to_screen_px(const GazeVector2& logical_pixel) const {
    return GazeVector2(
        logical_pixel.x * viewport_scale.x + viewport_offset_px.x + window_position_px.x,
        logical_pixel.y * viewport_scale.y + viewport_offset_px.y + window_position_px.y
    );
}

} // namespace Gaze
