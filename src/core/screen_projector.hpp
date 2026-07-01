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
    GazeVector2 window_position_px;   // Logical screen pixels
    GazeVector2 viewport_scale;       // Maps logical viewport units to window logical pixels
    GazeVector2 viewport_offset_px;   // Viewport offset (origin) in window logical pixels

    ScreenProjector() = default;
    ScreenProjector(const GazeVector2& win_pos_px, const GazeVector2& vp_scale, const GazeVector2& vp_offset_px)
        : window_position_px(win_pos_px), viewport_scale(vp_scale), viewport_offset_px(vp_offset_px) {}

    static ScreenProjector derive_configuration(
        const GazeVector2& win_pos_px,
        const GazeVector2& vp_scale,
        const GazeVector2& vp_origin
    );

    static ScreenProjector from_godot_geometry(
        const GazeVector2& window_pos_logical,
        const GazeVector2& viewport_scale_logical,
        const GazeVector2& viewport_offset_logical
    );

    bool project_to_viewport(
        const ProjectionEngine& engine,
        const GazeVector3& origin_cam,
        const GazeVector3& dir_cam,
        GazeVector2& out_viewport_pixel
    ) const;

    GazeVector2 map_viewport_to_screen_px(const GazeVector2& logical_pixel) const;
};

} // namespace Gaze
