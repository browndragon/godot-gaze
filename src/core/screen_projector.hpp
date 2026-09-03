/**
 * @file screen_projector.hpp
 * @brief Maps projected physical screen coordinates to viewport coordinates (Layer 4)
 */
#pragma once

#include "opencv_space_conversions.hpp"
#include "projection_engine.hpp"

namespace Gaze
{

    class ScreenProjector
    {
    public:
        GodotDisplayVector2 window_position_px;                       // Logical screen pixels
        SpacedVector2<Space::GodotViewportPx> viewport_scale;         // Maps logical viewport units to window logical pixels
        SpacedVector2<Space::GodotViewportPx> viewport_offset_px;     // Viewport offset (origin) in window logical pixels

        ScreenProjector() = default;
        ScreenProjector(
            const GodotDisplayVector2 &win_pos_px,
            const SpacedVector2<Space::GodotViewportPx> &vp_scale,
            const SpacedVector2<Space::GodotViewportPx> &vp_offset_px)
            : window_position_px(win_pos_px), viewport_scale(vp_scale), viewport_offset_px(vp_offset_px) {}

        static ScreenProjector derive_configuration(
            const GodotDisplayVector2 &win_pos_px,
            const SpacedVector2<Space::GodotViewportPx> &vp_scale,
            const SpacedVector2<Space::GodotViewportPx> &vp_origin);

        static ScreenProjector from_godot_geometry(
            const GodotDisplayVector2 &window_pos_logical,
            const SpacedVector2<Space::GodotViewportPx> &viewport_scale_logical,
            const SpacedVector2<Space::GodotViewportPx> &viewport_offset_logical);

        bool project_to_viewport(
            const ProjectionEngine &engine,
            const GodotCameraVector3 &origin_cam,
            const GodotCameraVector3 &dir_cam,
            SpacedVector2<Space::GodotViewportPx> &out_viewport_pixel) const;

        GodotDisplayVector2 map_viewport_to_screen_px(const SpacedVector2<Space::GodotViewportPx> &logical_pixel) const;
    };

} // namespace Gaze
