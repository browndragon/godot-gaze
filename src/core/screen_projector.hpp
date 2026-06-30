/**
 * @file screen_projector.hpp
 * @brief Decoupled component to map projected physical screen coordinates to viewport coordinates (Layer 4)
 */
#pragma once

#include "math_defs.hpp"
#include "projection_engine.hpp"

namespace Gaze {

class ScreenProjector {
public:
    GazeVector2 window_position_pixels; // Physical screen pixels
    GazeVector2 viewport_scale;         // Maps logical viewport units to window physical pixels
    GazeVector2 viewport_offset;        // Viewport offset (origin) in window physical pixels

    ScreenProjector() = default;
    ScreenProjector(const GazeVector2& win_pos, const GazeVector2& vp_scale, const GazeVector2& vp_offset)
        : window_position_pixels(win_pos), viewport_scale(vp_scale), viewport_offset(vp_offset) {}

    static ScreenProjector derive_configuration(
        const GazeVector2& win_pos_ppix,
        const GazeVector2& vp_scale,
        const GazeVector2& vp_origin
    );

    static ScreenProjector from_godot_geometry(
        const GazeVector2& window_pos_physical,
        const GazeVector2& viewport_scale_logical,
        const GazeVector2& viewport_offset_logical,
        double device_pixel_ratio,
        double window_to_screen_scale_ratio = 1.0
    );

    bool project_to_viewport(
        const ProjectionEngine& engine,
        const GazeVector3& origin_cam,
        const GazeVector3& dir_cam,
        GazeVector2& out_viewport_pixel
    ) const;

    GazeVector2 map_logical_to_physical(const GazeVector2& logical_pixel) const;
};

} // namespace Gaze
