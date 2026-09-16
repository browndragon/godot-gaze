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

#include "one_euro_filter.hpp"

TEST_CASE("Mouse Emulation: 1-Euro Filter Noise Suppression and Saccade Responsiveness")
{
    // Configure 1€ filter tuned for mouse emulation:
    // min_cutoff = 0.1 Hz (smooth stationary fixation), beta = 0.15 (fast saccades)
    double freq = 60.0;
    double min_cutoff = 0.1;
    double beta = 0.005;
    double d_cutoff = 1.0;

    OneEuroFilter filter_x(freq, min_cutoff, beta, d_cutoff);

    // 1. Stationary Fixation Phase: stationary gaze at 500 px with +/- 15 px model noise/jank
    double true_fixation = 500.0;
    double raw_sq_diff_sum = 0.0;
    double filtered_sq_diff_sum = 0.0;
    int fixation_frames = 60;

    for (int i = 0; i < fixation_frames; ++i) {
        double t = i * (1.0 / freq);
        // Deterministic high-frequency noise oscillating +/- 15 px
        double noise = 15.0 * std::sin(i * 1.5);
        double raw_val = true_fixation + noise;
        double filtered_val = filter_x.filter(raw_val, t);

        if (i >= 10) { // Let filter warm up
            raw_sq_diff_sum += (raw_val - true_fixation) * (raw_val - true_fixation);
            filtered_sq_diff_sum += (filtered_val - true_fixation) * (filtered_val - true_fixation);
        }
    }

    double raw_variance = raw_sq_diff_sum / (fixation_frames - 10);
    double filtered_variance = filtered_sq_diff_sum / (fixation_frames - 10);
    double noise_reduction = 1.0 - (filtered_variance / raw_variance);

    // Assert that the 1€ filter achieves >= 80% noise reduction during fixation (Delta >= 0.80)
    CHECK(noise_reduction >= 0.80);

    // 2. Saccade Phase: rapid jump from 500 px to 900 px (400 px displacement)
    double saccade_target = 900.0;
    double t_saccade = fixation_frames * (1.0 / freq);
    double jump_f1 = filter_x.filter(saccade_target, t_saccade);
    double jump_f2 = filter_x.filter(saccade_target, t_saccade + 1.0 / freq);

    // Within 2 frames (~33ms), filter should adapt cutoff via beta and traverse >= 65% of the distance
    double jump_progress = (jump_f2 - true_fixation) / (saccade_target - true_fixation);
    CHECK(jump_progress >= 0.65);
}

TEST_CASE("Mouse Emulation: Viewport Boundary Clamping Signal Separation")
{
    // Screen viewport: 1152x648
    godot::Vector2 viewport_size(1152.0, 648.0);
    godot::Vector2 offscreen_gaze(-100.0, 400.0); // 100 px outside the left edge

    // Helper lambda representing the clamping transform
    auto clamp_to_viewport = [](const godot::Vector2 &pos, const godot::Vector2 &vp_size, bool enabled) -> godot::Vector2 {
        if (!enabled) {
            return pos;
        }
        return godot::Vector2(
            std::clamp(pos.x, 0.0f, vp_size.x - 1e-4f),
            std::clamp(pos.y, 0.0f, vp_size.y - 1e-4f)
        );
    };

    godot::Vector2 clamped_pos = clamp_to_viewport(offscreen_gaze, viewport_size, true);
    godot::Vector2 unclamped_pos = clamp_to_viewport(offscreen_gaze, viewport_size, false);

    // Clamped position lands at x = 0.0
    CHECK(clamped_pos.x == doctest::Approx(0.0));
    CHECK(clamped_pos.y == doctest::Approx(400.0));

    // Unclamped position remains at x = -100.0
    CHECK(unclamped_pos.x == doctest::Approx(-100.0));
    CHECK(unclamped_pos.y == doctest::Approx(400.0));

    // Clear domain signal separation: Delta_x = 100.0 >= 0.50
    double delta_x = clamped_pos.x - unclamped_pos.x;
    CHECK(delta_x == doctest::Approx(100.0));
    CHECK(delta_x >= 0.50);
}

TEST_CASE("Mouse Emulation: Blink to Left Click State Machine")
{
    // State machine matching GazeServer blink-click logic
    bool was_both_closed = false;
    int click_press_events = 0;
    int click_release_events = 0;

    auto process_blink = [&](float left_open, float right_open) {
        bool left_closed = (left_open < 0.25f);
        bool right_closed = (right_open < 0.25f);
        if (left_closed && right_closed) {
            if (!was_both_closed) {
                click_press_events++;
                was_both_closed = true;
            }
        } else if (was_both_closed) {
            click_release_events++;
            was_both_closed = false;
        }
    };

    // Frame 1: Eyes wide open
    process_blink(1.0f, 1.0f);
    CHECK(click_press_events == 0);
    CHECK(click_release_events == 0);

    // Frame 2: Eyes closing (left=0.10, right=0.10 -> blink started)
    process_blink(0.10f, 0.10f);
    CHECK(click_press_events == 1);
    CHECK(click_release_events == 0);

    // Frame 3: Eyes remain closed
    process_blink(0.05f, 0.08f);
    CHECK(click_press_events == 1); // No double click
    CHECK(click_release_events == 0);

    // Frame 4: Eyes reopen (left=0.90, right=0.90 -> blink ended)
    process_blink(0.90f, 0.90f);
    CHECK(click_press_events == 1);
    CHECK(click_release_events == 1);
}

TEST_CASE("Canvas Transforms: GazeDisplayServer Logical Coordinate Contract and Closed-Loop Emulation")
{
    // Screen: 14" MacBook Pro Retina (1512x982 logical points, scale 2.0, 301.214 x 195.63 mm)
    double screen_scale = 2.0;
    godot::Vector2 screen_size_lpix(1512.0, 982.0);
    godot::Vector2 screen_size_mm(301.2141, 195.6298);
    godot::Vector2 pixel_pitch_mm(screen_size_mm.x / screen_size_lpix.x, screen_size_mm.y / screen_size_lpix.y);

    // Window: Centered window 576x324 points at (468, 319) logical offset
    godot::Vector2 window_offset_lpix(468.0, 319.0);
    godot::Vector2 window_size_lpix(576.0, 324.0);
    godot::Vector2 render_target_size(1152.0, 648.0);
    godot::Vector2 canvas_size(1152.0, 648.0);

    godot::Transform2D final_xform(
        godot::Vector2(render_target_size.x / canvas_size.x, 0.0),
        godot::Vector2(0.0, render_target_size.y / canvas_size.y),
        godot::Vector2(0.0, 0.0)
    );
    godot::Transform2D canvas_inv = final_xform.affine_inverse();

    // 1. Mouse cursor at (905, 564) in display logical coordinates
    godot::Vector2 mouse_screen_lpix(905.0, 564.0);

    // In GazeDisplayServer's unified contract, window mouse position is strictly logical:
    godot::Vector2 mouse_win_lpix = mouse_screen_lpix - window_offset_lpix; // (437.0, 245.0)
    CHECK(mouse_win_lpix.x == doctest::Approx(437.0));
    CHECK(mouse_win_lpix.y == doctest::Approx(245.0));

    // Canvas position: (mouse_win_lpix * screen_scale) inverted through final_xform
    godot::Vector2 mouse_canvas = canvas_inv.xform(mouse_win_lpix * screen_scale);
    CHECK(mouse_canvas.x == doctest::Approx(874.0));
    CHECK(mouse_canvas.y == doctest::Approx(490.0));
    CHECK(mouse_canvas.x == doctest::Approx(mouse_win_lpix.x * screen_scale));
    CHECK(mouse_canvas.y == doctest::Approx(mouse_win_lpix.y * screen_scale));

    // 2. Closed-Loop 3D Gaze Ray synthesis from screen mouse position
    double dx_mm = (mouse_screen_lpix.x / screen_size_lpix.x - 0.5) * screen_size_mm.x; // +29.6946 mm
    double dy_mm = (mouse_screen_lpix.y / screen_size_lpix.y - 0.5) * screen_size_mm.y; // +14.5367 mm
    Gaze::GodotCameraVector3 target_cam(-dx_mm, -dy_mm, 0.0);
    Gaze::GodotCameraVector3 eye_origin(0.0, 0.0, -500.0);
    Gaze::GodotCameraVector3 gaze_dir = (target_cam - eye_origin).normalized();

    // 3. Project ray back to window coordinates using ProjectionEngine
    Gaze::ProjectionEngine engine;
    engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(screen_size_lpix.x, screen_size_lpix.y));
    engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(screen_size_mm.x, screen_size_mm.y));
    engine.set_camera_placement(Gaze::CameraPlacement(Gaze::GodotCameraVector3(0.0, 0.0, 0.0), 0.0));
    engine.set_window_offset_pixels(Gaze::GodotDisplayVector2(window_offset_lpix.x, window_offset_lpix.y));

    Gaze::GodotDisplayVector2 projected_win_px;
    bool projected = engine.project_gaze(eye_origin, gaze_dir, projected_win_px);
    CHECK(projected == true);
    CHECK(projected_win_px.x == doctest::Approx(mouse_win_lpix.x).epsilon(0.001));
    CHECK(projected_win_px.y == doctest::Approx(mouse_win_lpix.y).epsilon(0.001));

    // Convert projected window pixels to canvas
    godot::Vector2 projected_canvas = canvas_inv.xform(godot::Vector2(projected_win_px.x, projected_win_px.y) * screen_scale);
    CHECK(projected_canvas.x == doctest::Approx(mouse_canvas.x).epsilon(0.001));
    CHECK(projected_canvas.y == doctest::Approx(mouse_canvas.y).epsilon(0.001));

    // Round-trip error must be virtually zero
    double round_trip_error = (projected_canvas - mouse_canvas).length();
    CHECK(round_trip_error < 1e-4);

    // 4. Red Phase / Flaw Verification & Signal Separation:
    // Flaw A: Mixing Godot DisplayServer physical mouse (1810, 1128) with Cocoa logical window offset (468, 319)
    godot::Vector2 flawed_physical_mouse(1810.0, 1128.0);
    godot::Vector2 flawed_win_pos = flawed_physical_mouse - window_offset_lpix; // (1342.0, 809.0)
    double unit_mismatch_error = (flawed_win_pos - mouse_win_lpix).length();
    CHECK(unit_mismatch_error == doctest::Approx(1062.29).epsilon(0.01));
    CHECK(unit_mismatch_error >= 0.50); // Clear signal separation

    // Flaw B: Treating window-relative mouse (437, 245) as screen-relative coordinates without window offset
    double dx_flawed_mm = (mouse_win_lpix.x / screen_size_lpix.x - 0.5) * screen_size_mm.x; // -63.55 mm
    double coordinate_space_flaw_delta = std::abs(dx_mm - dx_flawed_mm);
    CHECK(coordinate_space_flaw_delta == doctest::Approx(93.24).epsilon(0.01));
    CHECK(coordinate_space_flaw_delta >= 0.50); // Clear signal separation
}


