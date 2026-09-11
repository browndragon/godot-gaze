/**
 * @file test_canvas_transforms.cpp
 * @brief Unit tests for HiDPI/Retina canvas coordinate transformations across window modes.
 */

#include "doctest.h"
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include "projection_engine.hpp"

using namespace Gaze;

TEST_CASE("Canvas Transforms: Fullscreen HiDPI Scale Parity")
{
    // A 14" MacBook Pro Retina screen: 1512x945 logical points, scale factor 2.0.
    // In Godot 4, the window backing render buffer has size 3024x1890 (root.size).
    // With design resolution 1152x648 (stretch=canvas_items, aspect=expand),
    // canvas size is 1152x720.
    // root.get_final_transform() has scale:
    //   scale_x = 3024 / 1152 = 2.625
    //   scale_y = 1890 / 720  = 2.625
    //
    // Therefore, affine_inverse() maps from render buffer pixels (0..3024) to canvas (0..1152).
    double screen_scale = 2.0;
    godot::Vector2 logical_screen_size(1512.0, 945.0);
    godot::Vector2 canvas_size(1152.0, 720.0);
    godot::Vector2 render_target_size = logical_screen_size * screen_scale; // (3024.0, 1890.0)

    godot::Transform2D final_xform(
        godot::Vector2(render_target_size.x / canvas_size.x, 0.0),
        godot::Vector2(0.0, render_target_size.y / canvas_size.y),
        godot::Vector2(0.0, 0.0)
    );
    godot::Transform2D canvas_inv = final_xform.affine_inverse();

    // 1. Center gaze in logical points from ProjectionEngine
    godot::Vector2 logical_center = logical_screen_size * 0.5; // (756.0, 472.5)

    // Red Phase / Flaw: Passing logical points directly to canvas_inv divides by 2.625,
    // producing 288.0 instead of canvas center 576.0 (halved!).
    godot::Vector2 flawed_canvas_pos = canvas_inv.xform(logical_center);
    CHECK(flawed_canvas_pos.x == doctest::Approx(288.0));
    CHECK(flawed_canvas_pos.y == doctest::Approx(180.0));

    // Green Phase: Converting logical points to render buffer pixels (* screen_scale)
    // before canvas_inv lands exactly on canvas center (576.0, 360.0).
    godot::Vector2 render_buffer_pos = logical_center * screen_scale;
    godot::Vector2 correct_canvas_pos = canvas_inv.xform(render_buffer_pos);
    CHECK(correct_canvas_pos.x == doctest::Approx(576.0));
    CHECK(correct_canvas_pos.y == doctest::Approx(360.0));
    CHECK(correct_canvas_pos.x == doctest::Approx(canvas_size.x * 0.5));
    CHECK(correct_canvas_pos.y == doctest::Approx(canvas_size.y * 0.5));

    // 2. Right edge gaze in logical points (x = 1512.0)
    godot::Vector2 logical_right_edge(1512.0, logical_center.y);
    godot::Vector2 correct_right_canvas = canvas_inv.xform(logical_right_edge * screen_scale);
    CHECK(correct_right_canvas.x == doctest::Approx(1152.0));
    CHECK(correct_right_canvas.x == doctest::Approx(canvas_size.x));
    // Offset from center is +576.0 (strictly positive to the right of center!)
    CHECK((correct_right_canvas.x - correct_canvas_pos.x) == doctest::Approx(576.0));
}

TEST_CASE("Canvas Transforms: Centered Window Mode HiDPI Scale Parity")
{
    // Centered window of size 1152x648 on a 1512x982 display (Retina scale 2.0).
    // In Cocoa points, window size is 576x324 at offset (468, 329).
    // Backing render buffer is 1152x648. Canvas is 1152x648.
    // final_xform is identity (1.0, 1.0).
    double screen_scale = 2.0;
    godot::Vector2 window_logical_size(576.0, 324.0);
    godot::Vector2 window_offset_logical(468.0, 329.0);
    godot::Vector2 canvas_size(1152.0, 648.0);
    godot::Vector2 render_target_size = window_logical_size * screen_scale; // (1152.0, 648.0)

    godot::Transform2D final_xform(
        godot::Vector2(render_target_size.x / canvas_size.x, 0.0),
        godot::Vector2(0.0, render_target_size.y / canvas_size.y),
        godot::Vector2(0.0, 0.0)
    );
    godot::Transform2D canvas_inv = final_xform.affine_inverse();

    // Center of display in logical points is (756.0, 491.0).
    // Center relative to window in logical points: (756 - 468, 491 - 329) = (288.0, 162.0).
    godot::Vector2 center_window_logical(288.0, 162.0);

    // Flawed: without * screen_scale, lands at (288.0, 162.0) (half of 576.0, 324.0)
    godot::Vector2 flawed_pos = canvas_inv.xform(center_window_logical);
    CHECK(flawed_pos.x == doctest::Approx(288.0));
    CHECK(flawed_pos.y == doctest::Approx(162.0));

    // Correct: with * screen_scale, lands at (576.0, 324.0) (exact canvas center)
    godot::Vector2 correct_pos = canvas_inv.xform(center_window_logical * screen_scale);
    CHECK(correct_pos.x == doctest::Approx(576.0));
    CHECK(correct_pos.y == doctest::Approx(324.0));
    CHECK(correct_pos.x == doctest::Approx(canvas_size.x * 0.5));
    CHECK(correct_pos.y == doctest::Approx(canvas_size.y * 0.5));

    // Right edge of display (1512.0 pt): relative to window is (1512 - 468) = 1044.0 pt.
    godot::Vector2 right_edge_logical(1044.0, 162.0);
    godot::Vector2 right_edge_canvas = canvas_inv.xform(right_edge_logical * screen_scale);
    CHECK(right_edge_canvas.x == doctest::Approx(2088.0));
    // Offset from center is +1512.0 (strictly positive to the right of center!)
    CHECK((right_edge_canvas.x - correct_pos.x) == doctest::Approx(1512.0));
}

TEST_CASE("Canvas Transforms: Physical Millimeter Invariance Between Windowed and Fullscreen")
{
    // A 14" MacBook Pro has screen width 301.214 mm (1512 Cocoa points, pitch 0.199216 mm/pt).
    // Gazing at right edge of physical screen: distance from center is +150.607 mm.
    double pitch_mm_per_pt = 0.199216;
    double expected_dist_from_center_mm = (1512.0 * 0.5) * pitch_mm_per_pt; // 150.607 mm

    // Fullscreen:
    // Canvas width 1152 represents 1512 points = 301.214 mm.
    // Canvas pixel size in mm:
    double fullscreen_mm_per_canvas_px = (1512.0 * pitch_mm_per_pt) / 1152.0;
    double fullscreen_offset_canvas_px = 576.0; // from previous test
    double fullscreen_dist_mm = fullscreen_offset_canvas_px * fullscreen_mm_per_canvas_px;

    // Windowed:
    // Window width 576 points = 114.748 mm represented across 1152 canvas pixels.
    // Canvas pixel size in mm:
    double windowed_mm_per_canvas_px = (576.0 * pitch_mm_per_pt) / 1152.0;
    double windowed_offset_canvas_px = 1512.0; // from previous test
    double windowed_dist_mm = windowed_offset_canvas_px * windowed_mm_per_canvas_px;

    CHECK(fullscreen_dist_mm == doctest::Approx(expected_dist_from_center_mm));
    CHECK(windowed_dist_mm == doctest::Approx(expected_dist_from_center_mm));
    CHECK(std::abs(fullscreen_dist_mm - windowed_dist_mm) < 1e-4);
}

TEST_CASE("Canvas Transforms: Mouse Emulation Canvas Space Parity")
{
    // In Godot 4, window-local mouse position (mouse_get_position - window_get_position)
    // is mapped into canvas space via affine_inverse() without extra screen_scale multiplication.
    // For a centered 1152x648 project on Retina (576x324 Cocoa points):
    godot::Vector2 canvas_size(1152.0, 648.0);
    godot::Vector2 render_target_size(1152.0, 648.0);

    godot::Transform2D final_xform(
        godot::Vector2(render_target_size.x / canvas_size.x, 0.0),
        godot::Vector2(0.0, render_target_size.y / canvas_size.y),
        godot::Vector2(0.0, 0.0)
    );
    godot::Transform2D canvas_inv = final_xform.affine_inverse();

    // Mouse positioned at (576, 324) in the window
    godot::Vector2 mouse_pos(576.0, 324.0);
    godot::Vector2 mouse_canvas = canvas_inv.xform(mouse_pos);
    CHECK(mouse_canvas.x == doctest::Approx(576.0));
    CHECK(mouse_canvas.y == doctest::Approx(324.0));

    // Camera gaze position is already in canvas space
    godot::Vector2 cam_gaze_canvas(576.0, 324.0);

    // Blending in canvas space at ease_factor 0.5
    float ease_factor = 0.5f;
    godot::Vector2 blended_canvas = cam_gaze_canvas.lerp(mouse_canvas, ease_factor);
    CHECK(blended_canvas.x == doctest::Approx(576.0));
    CHECK(blended_canvas.y == doctest::Approx(324.0));

    // Notice: if cam_gaze_canvas had been passed to canvas_inv a second time or multiplied by 2x,
    // it would diverge or double. In our single canvas-space blend, it is strictly invariant.
    CHECK(blended_canvas == mouse_canvas);
}

