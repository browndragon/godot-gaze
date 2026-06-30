/**
 * @file screen_projector.cpp
 * @brief Implement screen projector coordinate transformation math (Layer 4)
 */
#include "screen_projector.hpp"
#include <cmath>

namespace Gaze {

ScreenProjector ScreenProjector::derive_configuration(
    const GazeVector2& win_pos_ppix,
    const GazeVector2& vp_scale,
    const GazeVector2& vp_origin
) {
    return ScreenProjector(win_pos_ppix, vp_scale, vp_origin);
}

ScreenProjector ScreenProjector::from_godot_geometry(
    const GazeVector2& window_pos_physical,
    const GazeVector2& viewport_scale_logical,
    const GazeVector2& viewport_offset_logical,
    double device_pixel_ratio,
    double window_to_screen_scale_ratio
) {
    double scale_factor = device_pixel_ratio / (window_to_screen_scale_ratio > 0.0 ? window_to_screen_scale_ratio : 1.0);
    return ScreenProjector(
        window_pos_physical,
        viewport_scale_logical * scale_factor,
        viewport_offset_logical * scale_factor
    );
}

bool ScreenProjector::project_to_viewport(
    const ProjectionEngine& engine,
    const GazeVector3& origin_cam,
    const GazeVector3& dir_cam,
    GazeVector2& out_viewport_pixel
) const {
    GazeVector2 screen_pixel;
    
    // 1. Project 3D gaze ray onto screen plane in physical screen pixels
    if (!engine.project_gaze(origin_cam, dir_cam, screen_pixel)) {
        return false;
    }

    // 2. Subtract physical window position to get window-local physical pixels
    double local_phys_x = screen_pixel.x - window_position_pixels.x;
    double local_phys_y = screen_pixel.y - window_position_pixels.y;

    // 3. Map physical window-local pixels to logical viewport coordinates
    // physical = logical * scale + offset
    // => logical = (physical - offset) / scale
    if (std::abs(viewport_scale.x) < 1e-6 || std::abs(viewport_scale.y) < 1e-6) {
        return false;
    }

    out_viewport_pixel.x = (local_phys_x - viewport_offset.x) / viewport_scale.x;
    out_viewport_pixel.y = (local_phys_y - viewport_offset.y) / viewport_scale.y;

    return true;
}

GazeVector2 ScreenProjector::map_logical_to_physical(const GazeVector2& logical_pixel) const {
    return GazeVector2(
        logical_pixel.x * viewport_scale.x + viewport_offset.x + window_position_pixels.x,
        logical_pixel.y * viewport_scale.y + viewport_offset.y + window_position_pixels.y
    );
}

} // namespace Gaze
