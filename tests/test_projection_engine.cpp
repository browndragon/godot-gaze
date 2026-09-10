#include "doctest.h"
#include "projection_engine.hpp"
#include "screen_projector.hpp"
#include "camera_placement.hpp"
#include <cmath>

TEST_CASE("Screen Projection Engine Boundary Invariants and Realistic Laptop Geometry")
{
    Gaze::ProjectionEngine engine;
    engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(3024.0, 1964.0));
    engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(301.5, 188.5));

    // Scenario 1: Standard Laptop Lid Open 105 deg (Camera Tilted Down -15 deg)
    // Camera is at top bezel center (0, 0, 0) mm relative to top bezel
    Gaze::CameraPlacement placement_tilted(Gaze::GodotCameraVector3(0.0, 0.0, 0.0), -15.0);
    engine.set_camera_placement(placement_tilted);

    // User head is below camera at (0, -50, -650) mm in Godot Camera Space
    // User faces forward (+15 deg pitch relative to tilted camera) and looks down -12 deg at screen
    Gaze::GodotCameraVector3 origin_godot(0.0, -50.0, -650.0);
    Gaze::GodotCameraVector3 dir_godot(0.0, -0.2079, 0.9781); // pointing toward screen plane in Godot Camera Space

    Gaze::GodotDisplayVector2 pixel_proj;
    bool proj_ok = engine.project_gaze(origin_godot, dir_godot, pixel_proj);

    REQUIRE(proj_ok == true);
    Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm> mm_proj = engine.pixel_to_millimeter(pixel_proj);

    // Projected point MUST land on the display surface (Y mm near screen center/lower display)
    CHECK(mm_proj.x == doctest::Approx(0.0).epsilon(0.1));
    CHECK(mm_proj.y > -94.25); // Below top bezel
    CHECK(mm_proj.y < 94.25);  // Above bottom bezel

    // Scenario 2: Off-Axis Seating (User offset Right +150mm, looking at screen center)
    Gaze::GodotCameraVector3 off_axis_origin_godot(150.0, -20.0, -600.0);
    Gaze::GodotCameraVector3 off_axis_dir_godot(-0.2425, 0.0323, 0.9696); // pointing toward screen center

    bool off_axis_ok = engine.project_gaze(off_axis_origin_godot, off_axis_dir_godot, pixel_proj);
    REQUIRE(off_axis_ok == true);

    Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm> off_axis_mm = engine.pixel_to_millimeter(pixel_proj);
    CHECK(std::abs(off_axis_mm.x) < 50.0); // Near screen center X

    // Scenario 3: Looking Away Protection (Gaze pointing backward into room wall behind user: dir_godot.z < 0)
    Gaze::GodotCameraVector3 away_dir_godot(0.0, 0.0, -1.0); // pointing backward into room (-Z in Godot Camera space)
    bool away_ok = engine.project_gaze(origin_godot, away_dir_godot, pixel_proj);
    CHECK(away_ok == false); // Safe protection: ray pointing away returns false!
}

TEST_CASE("Direct Camera Plane Projection Invariants")
{
    // Invariant 1: Staring directly at camera from 500mm away in Godot Camera Space
    // Origin (0, 0, -500), Dir (0, 0, +1) -> intersects camera plane Z=0 at (0, 0, 0)
    Gaze::GodotCameraVector3 orig_center(0.0, 0.0, -500.0);
    Gaze::GodotCameraVector3 dir_at_cam(0.0, 0.0, 1.0);
    Gaze::GodotCameraVector3 pt_cam = Gaze::project_ray_to_camera_plane(orig_center, dir_at_cam);
    REQUIRE(pt_cam.is_finite() == true);
    CHECK(pt_cam.x == doctest::Approx(0.0).epsilon(0.01));
    CHECK(pt_cam.y == doctest::Approx(0.0).epsilon(0.01));
    CHECK(pt_cam.z == doctest::Approx(0.0).epsilon(0.01));

    // Invariant 2: Looking downward from (0, 0, -500) towards (0, -100, 0) on camera plane
    Gaze::GodotCameraVector3 dir_to_center = Gaze::GodotCameraVector3(0.0, -100.0, 500.0).normalized();
    Gaze::GodotCameraVector3 pt_down = Gaze::project_ray_to_camera_plane(orig_center, dir_to_center);
    REQUIRE(pt_down.is_finite() == true);
    CHECK(pt_down.x == doctest::Approx(0.0).epsilon(0.01));
    CHECK(pt_down.y == doctest::Approx(-100.0).epsilon(0.01));
    CHECK(pt_down.z == doctest::Approx(0.0).epsilon(0.01));

    // Invariant 3: Looking Camera-Left (-X_cam) lands at negative X in camera space
    Gaze::GodotCameraVector3 dir_cam_left = Gaze::GodotCameraVector3(-100.0, -100.0, 500.0).normalized();
    Gaze::GodotCameraVector3 pt_left = Gaze::project_ray_to_camera_plane(orig_center, dir_cam_left);
    REQUIRE(pt_left.is_finite() == true);
    CHECK(pt_left.x == doctest::Approx(-100.0).epsilon(0.01));
    CHECK(pt_left.y == doctest::Approx(-100.0).epsilon(0.01));

    // Invariant 3b: Looking Camera-Right (+X_cam) lands at positive X in camera space
    Gaze::GodotCameraVector3 dir_cam_right = Gaze::GodotCameraVector3(100.0, -100.0, 500.0).normalized();
    Gaze::GodotCameraVector3 pt_right = Gaze::project_ray_to_camera_plane(orig_center, dir_cam_right);
    REQUIRE(pt_right.is_finite() == true);
    CHECK(pt_right.x == doctest::Approx(100.0).epsilon(0.01));
    CHECK(pt_right.y == doctest::Approx(-100.0).epsilon(0.01));

    // Invariant 4: Ray pointing away (Z < 0) or parallel (Z = 0) returns non-finite / infinite vector
    Gaze::GodotCameraVector3 dir_away(0.0, 0.0, -1.0);
    Gaze::GodotCameraVector3 pt_away = Gaze::project_ray_to_camera_plane(orig_center, dir_away);
    CHECK(pt_away.is_finite() == false);

    Gaze::GodotCameraVector3 dir_parallel(1.0, 0.0, 0.0);
    Gaze::GodotCameraVector3 pt_parallel = Gaze::project_ray_to_camera_plane(orig_center, dir_parallel);
    CHECK(pt_parallel.is_finite() == false);

    // Invariant 5: Focal length calculation with standard 65 deg FOV
    double f1440 = Gaze::calculate_default_focal_length(1440.0, 65.0);
    CHECK(f1440 == doctest::Approx(1130.16).epsilon(0.01));
}

// NOTE on Device Geometries: We test synthetic screen aspect ratios (tall phone-like proportions, tablets, ultrawide)
// and non-standard camera bezel locations (center-left and center-right bezels). These test that 3D-to-2D projection math
// behaves consistently across diverse form factors without implying dynamic sensor/device rotation support yet.
TEST_CASE("Screen Shape & Bezel Matrix Invariants: Tall, Wide Left/Right Bezel, Tablet, and Ultrawide")
{
    // 1. Tall Screen with Screen-Center Camera (Phone-like proportions: 71.5mm x 146.7mm, 1170 x 2532 px)
    {
        Gaze::ProjectionEngine engine;
        engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(1170.0, 2532.0));
        engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(71.5, 146.7));
        engine.set_camera_placement(Gaze::CameraPlacement(Gaze::GodotCameraVector3(0.0, 0.0, 0.0), 0.0));

        // Staring straight ahead from (0, 0, -400) mm hits screen center (X=585px, Y=1266px)
        Gaze::GodotCameraVector3 origin(0.0, 0.0, -400.0);
        Gaze::GodotCameraVector3 dir_center = Gaze::GodotCameraVector3(0.0, 0.0, 1.0);

        Gaze::GodotDisplayVector2 px_out;
        REQUIRE(engine.project_gaze(origin, dir_center, px_out) == true);
        CHECK(px_out.x == doctest::Approx(585.0).epsilon(1.0));   // 1170 / 2
        CHECK(px_out.y == doctest::Approx(1266.0).epsilon(1.0));  // 2532 / 2

        // Top-edge gaze (Y = 0 px): aim +73.35mm UP from center
        Gaze::GodotCameraVector3 dir_top = Gaze::GodotCameraVector3(0.0, 73.35, 400.0).normalized();
        REQUIRE(engine.project_gaze(origin, dir_top, px_out) == true);
        CHECK(px_out.x == doctest::Approx(585.0).epsilon(1.0));
        CHECK(px_out.y == doctest::Approx(0.0).epsilon(1.0));

        // Bottom-edge gaze (Y = 2532 px): aim -73.35mm DOWN from center
        Gaze::GodotCameraVector3 dir_bottom = Gaze::GodotCameraVector3(0.0, -73.35, 400.0).normalized();
        REQUIRE(engine.project_gaze(origin, dir_bottom, px_out) == true);
        CHECK(px_out.x == doctest::Approx(585.0).epsilon(1.0));
        CHECK(px_out.y == doctest::Approx(2532.0).epsilon(1.0));
    }

    // 2. Wide Screen with Center-Left Bezel Camera (e.g. Landscape shape: 146.7mm x 71.5mm, 2532 x 1170 px, camera on left bezel)
    {
        Gaze::ProjectionEngine engine;
        engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(2532.0, 1170.0));
        engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(146.7, 71.5));
        // Camera offset relative to display center is at left bezel: X = -73.35 mm, Y = 0 mm
        engine.set_camera_placement(Gaze::CameraPlacement(Gaze::GodotCameraVector3(-73.35, 0.0, 0.0), 0.0));

        Gaze::GodotCameraVector3 origin(0.0, 0.0, -400.0);
        // Looking straight ahead (+Z in camera space) hits screen at camera position (Left edge X=0 px, Mid Y=585 px)
        Gaze::GodotCameraVector3 dir_straight(0.0, 0.0, 1.0);
        Gaze::GodotDisplayVector2 px_out;
        REQUIRE(engine.project_gaze(origin, dir_straight, px_out) == true);
        CHECK(px_out.x == doctest::Approx(0.0).epsilon(1.0));
        CHECK(px_out.y == doctest::Approx(585.0).epsilon(1.0));

        // Looking at display center (+73.35mm X in camera space) lands on center (X=1266 px, Y=585 px)
        Gaze::GodotCameraVector3 dir_to_center = Gaze::GodotCameraVector3(-73.35, 0.0, 400.0).normalized();
        REQUIRE(engine.project_gaze(origin, dir_to_center, px_out) == true);
        CHECK(px_out.x == doctest::Approx(1266.0).epsilon(1.0));
        CHECK(px_out.y == doctest::Approx(585.0).epsilon(1.0));
    }

    // 3. Wide Screen with Center-Right Bezel Camera (146.7mm x 71.5mm, 2532 x 1170 px, camera on right bezel)
    {
        Gaze::ProjectionEngine engine;
        engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(2532.0, 1170.0));
        engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(146.7, 71.5));
        // Camera on right bezel: X = +73.35 mm
        engine.set_camera_placement(Gaze::CameraPlacement(Gaze::GodotCameraVector3(73.35, 0.0, 0.0), 0.0));

        Gaze::GodotCameraVector3 origin(0.0, 0.0, -400.0);
        // Looking straight ahead (+Z) hits right edge (X=2532 px, Y=585 px)
        Gaze::GodotCameraVector3 dir_straight(0.0, 0.0, 1.0);
        Gaze::GodotDisplayVector2 px_out;
        REQUIRE(engine.project_gaze(origin, dir_straight, px_out) == true);
        CHECK(px_out.x == doctest::Approx(2532.0).epsilon(1.0));
        CHECK(px_out.y == doctest::Approx(585.0).epsilon(1.0));

        // Looking at display center (-73.35mm from right bezel in display space -> -X_cam towards display right / camera left) lands on center (X=1266 px, Y=585 px)
        Gaze::GodotCameraVector3 dir_to_center = Gaze::GodotCameraVector3(-73.35, 0.0, 400.0).normalized();
        REQUIRE(engine.project_gaze(origin, dir_to_center, px_out) == true);
        CHECK(px_out.x == doctest::Approx(1266.0).epsilon(1.0));
        CHECK(px_out.y == doctest::Approx(585.0).epsilon(1.0));
    }

    // 4. Tablet Form Factor (4:3 aspect ratio: 197.0mm x 148.0mm, 2048 x 1536 px, screen center camera)
    {
        Gaze::ProjectionEngine engine;
        engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(2048.0, 1536.0));
        engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(197.0, 148.0));
        engine.set_camera_placement(Gaze::CameraPlacement(Gaze::GodotCameraVector3(0.0, 0.0, 0.0), 0.0));

        Gaze::GodotCameraVector3 origin(0.0, 0.0, -450.0);
        Gaze::GodotCameraVector3 dir_center = Gaze::GodotCameraVector3(0.0, 0.0, 1.0);
        Gaze::GodotDisplayVector2 px_out;
        REQUIRE(engine.project_gaze(origin, dir_center, px_out) == true);
        CHECK(px_out.x == doctest::Approx(1024.0).epsilon(1.0));
        CHECK(px_out.y == doctest::Approx(768.0).epsilon(1.0));
    }

    // 5. Ultrawide Form Factor (21:9 aspect ratio: 800.0mm x 340.0mm, 3440 x 1440 px, screen center camera)
    {
        Gaze::ProjectionEngine engine;
        engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(3440.0, 1440.0));
        engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(800.0, 340.0));
        engine.set_camera_placement(Gaze::CameraPlacement(Gaze::GodotCameraVector3(0.0, 0.0, 0.0), 0.0));

        Gaze::GodotCameraVector3 origin(0.0, 0.0, -700.0);
        // Center
        Gaze::GodotCameraVector3 dir_center = Gaze::GodotCameraVector3(0.0, 0.0, 1.0);
        Gaze::GodotDisplayVector2 px_out;
        REQUIRE(engine.project_gaze(origin, dir_center, px_out) == true);
        CHECK(px_out.x == doctest::Approx(1720.0).epsilon(1.0));
        CHECK(px_out.y == doctest::Approx(720.0).epsilon(1.0));

        // Far Display Left (camera right +X = 350mm)
        Gaze::GodotCameraVector3 dir_far_left = Gaze::GodotCameraVector3(350.0, 0.0, 700.0).normalized();
        REQUIRE(engine.project_gaze(origin, dir_far_left, px_out) == true);
        CHECK(px_out.x < 500.0);
        CHECK(px_out.x > 0.0);

        // Far Display Right (camera left -X = -350mm)
        Gaze::GodotCameraVector3 dir_far_right = Gaze::GodotCameraVector3(-350.0, 0.0, 700.0).normalized();
        REQUIRE(engine.project_gaze(origin, dir_far_right, px_out) == true);
        CHECK(px_out.x > 2940.0);
        CHECK(px_out.x < 3440.0);
    }
}

TEST_CASE("Screen Projection Engine Out-of-Bounds & Parallel Ray Robustness")
{
    Gaze::ProjectionEngine engine;
    engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(500.0, 300.0));
    engine.set_camera_placement(Gaze::CameraPlacement(Gaze::GodotCameraVector3(0.0, 0.0, 0.0), 0.0));

    Gaze::GodotCameraVector3 origin(0.0, 0.0, -500.0);
    Gaze::GodotDisplayVector2 px_out;

    // 1. Parallel Ray to Screen Plane (Z = 0)
    Gaze::GodotCameraVector3 dir_parallel(1.0, 0.0, 0.0);
    CHECK(engine.project_gaze(origin, dir_parallel, px_out) == false);

    // 2. Backward Ray Away From Screen (Z < 0)
    Gaze::GodotCameraVector3 dir_backward(0.0, 0.0, -1.0);
    CHECK(engine.project_gaze(origin, dir_backward, px_out) == false);

    // 3. Ray Behind Screen (Origin located behind screen plane: Z = +100)
    Gaze::GodotCameraVector3 origin_behind(0.0, 0.0, 100.0);
    Gaze::GodotCameraVector3 dir_forward(0.0, 0.0, 1.0);
    CHECK(engine.project_gaze(origin_behind, dir_forward, px_out) == false);
}

TEST_CASE("Projection Engine Hardware Invariance and Display Orientation Transformations (0, 90, 180, 270 deg)")
{
    Gaze::ProjectionEngine engine;
    // Physical hardware panel dimensions: 301.5mm x 188.5mm, 3024 x 1964 px (14" laptop screen)
    engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(3024.0, 1964.0));
    engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(301.5, 188.5));
    engine.set_camera_placement(Gaze::CameraPlacement(Gaze::GodotCameraVector3(0.0, 0.0, 0.0), 0.0));

    Gaze::GodotCameraVector3 origin(0.0, 0.0, -500.0);
    Gaze::GodotDisplayVector2 px_out;

    Gaze::GodotCameraVector3 dir_center = Gaze::GodotCameraVector3(0.0, 0.0, 1.0);
    Gaze::GodotCameraVector3 dir_top = Gaze::GodotCameraVector3(0.0, 94.25, 500.0).normalized();
    Gaze::GodotCameraVector3 dir_bottom = Gaze::GodotCameraVector3(0.0, -94.25, 500.0).normalized();
    Gaze::GodotCameraVector3 dir_left = Gaze::GodotCameraVector3(150.75, 0.0, 500.0).normalized();
    Gaze::GodotCameraVector3 dir_right = Gaze::GodotCameraVector3(-150.75, 0.0, 500.0).normalized();

    // -------------------------------------------------------------
    // 1. ORIENTATION_0 (Natural Upright Landscape: 3024 x 1964 px)
    // -------------------------------------------------------------
    engine.set_display_orientation(Gaze::DisplayOrientation::ORIENTATION_0);

    // (a) Gaze at screen center
    REQUIRE(engine.project_gaze(origin, dir_center, px_out) == true);
    CHECK(px_out.x == doctest::Approx(1512.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(982.0).epsilon(1.0));

    // (b) Gaze at top edge
    REQUIRE(engine.project_gaze(origin, dir_top, px_out) == true);
    CHECK(px_out.x == doctest::Approx(1512.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(0.0).epsilon(1.0));

    // (c) Gaze at bottom edge
    REQUIRE(engine.project_gaze(origin, dir_bottom, px_out) == true);
    CHECK(px_out.x == doctest::Approx(1512.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(1964.0).epsilon(1.0));

    // (d) Gaze at left edge
    REQUIRE(engine.project_gaze(origin, dir_left, px_out) == true);
    CHECK(px_out.x == doctest::Approx(0.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(982.0).epsilon(1.0));

    // (e) Gaze at right edge
    REQUIRE(engine.project_gaze(origin, dir_right, px_out) == true);
    CHECK(px_out.x == doctest::Approx(3024.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(982.0).epsilon(1.0));

    // -------------------------------------------------------------
    // 2. ORIENTATION_90 (Rotated 90 deg CW - Portrait Right: 1964 x 3024 px)
    // Physical top is on the RIGHT bezel of the rotated viewport.
    // -------------------------------------------------------------
    engine.set_display_orientation(Gaze::DisplayOrientation::ORIENTATION_90);

    // (a) Gaze at center
    REQUIRE(engine.project_gaze(origin, dir_center, px_out) == true);
    CHECK(px_out.x == doctest::Approx(982.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(1512.0).epsilon(1.0));

    // (b) Gaze at physical top edge (now right edge of portrait screen)
    REQUIRE(engine.project_gaze(origin, dir_top, px_out) == true);
    CHECK(px_out.x == doctest::Approx(1964.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(1512.0).epsilon(1.0));

    // (c) Gaze at physical bottom edge (now left edge of portrait screen)
    REQUIRE(engine.project_gaze(origin, dir_bottom, px_out) == true);
    CHECK(px_out.x == doctest::Approx(0.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(1512.0).epsilon(1.0));

    // (d) Gaze at physical left edge (now top edge of portrait screen)
    REQUIRE(engine.project_gaze(origin, dir_left, px_out) == true);
    CHECK(px_out.x == doctest::Approx(982.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(0.0).epsilon(1.0));

    // (e) Gaze at physical right edge (now bottom edge of portrait screen)
    REQUIRE(engine.project_gaze(origin, dir_right, px_out) == true);
    CHECK(px_out.x == doctest::Approx(982.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(3024.0).epsilon(1.0));

    // -------------------------------------------------------------
    // 3. ORIENTATION_180 (Inverted Landscape: 3024 x 1964 px)
    // Physical top is on the BOTTOM bezel of the inverted viewport.
    // -------------------------------------------------------------
    engine.set_display_orientation(Gaze::DisplayOrientation::ORIENTATION_180);

    // (a) Gaze at center
    REQUIRE(engine.project_gaze(origin, dir_center, px_out) == true);
    CHECK(px_out.x == doctest::Approx(1512.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(982.0).epsilon(1.0));

    // (b) Gaze at physical top edge (now bottom edge of inverted screen)
    REQUIRE(engine.project_gaze(origin, dir_top, px_out) == true);
    CHECK(px_out.x == doctest::Approx(1512.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(1964.0).epsilon(1.0));

    // (c) Gaze at physical bottom edge (now top edge of inverted screen)
    REQUIRE(engine.project_gaze(origin, dir_bottom, px_out) == true);
    CHECK(px_out.x == doctest::Approx(1512.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(0.0).epsilon(1.0));

    // -------------------------------------------------------------
    // 4. ORIENTATION_270 (Rotated 270 deg CW - Portrait Left: 1964 x 3024 px)
    // Physical top is on the LEFT bezel of the rotated viewport.
    // -------------------------------------------------------------
    engine.set_display_orientation(Gaze::DisplayOrientation::ORIENTATION_270);

    // (a) Gaze at center
    REQUIRE(engine.project_gaze(origin, dir_center, px_out) == true);
    CHECK(px_out.x == doctest::Approx(982.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(1512.0).epsilon(1.0));

    // (b) Gaze at physical top edge (now left edge of portrait screen)
    REQUIRE(engine.project_gaze(origin, dir_top, px_out) == true);
    CHECK(px_out.x == doctest::Approx(0.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(1512.0).epsilon(1.0));

    // (c) Gaze at physical bottom edge (now right edge of portrait screen)
    REQUIRE(engine.project_gaze(origin, dir_bottom, px_out) == true);
    CHECK(px_out.x == doctest::Approx(1964.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(1512.0).epsilon(1.0));
}

TEST_CASE("Projection Engine Window Translation Offset (T_window)")
{
    Gaze::ProjectionEngine engine;
    engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(3024.0, 1964.0));
    engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(301.5, 188.5));
    engine.set_camera_placement(Gaze::CameraPlacement(Gaze::GodotCameraVector3(0.0, 0.0, 0.0), 0.0));

    // Set Godot game window offset: top-left at (200, 150) px
    engine.set_window_offset_pixels(Gaze::GodotDisplayVector2(200.0, 150.0));

    Gaze::GodotCameraVector3 origin(0.0, 0.0, -500.0);
    Gaze::GodotCameraVector3 dir_center = Gaze::GodotCameraVector3(0.0, 0.0, 1.0);

    Gaze::GodotDisplayVector2 px_out;
    REQUIRE(engine.project_gaze(origin, dir_center, px_out) == true);

    // Without window offset, screen center is (1512, 982).
    // With window offset (200, 150), viewport position MUST be (1312, 832).
    CHECK(px_out.x == doctest::Approx(1312.0).epsilon(1.0));
    CHECK(px_out.y == doctest::Approx(832.0).epsilon(1.0));
}

TEST_CASE("Projection Engine Gravity Adaptation Invariants")
{
    Gaze::ProjectionEngine engine;
    engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(3024.0, 1964.0));
    engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(301.5, 188.5));

    // Default upright gravity vector points in -Y_cam (gravity pulls down)
    CHECK(engine.get_gravity_vector().x == doctest::Approx(0.0).epsilon(0.01));
    CHECK(engine.get_gravity_vector().y == doctest::Approx(-1.0).epsilon(0.01));
    CHECK(engine.get_gravity_vector().z == doctest::Approx(0.0).epsilon(0.01));

    // When rotated 90 deg clockwise, gravity in camera space pulls in -X_cam
    engine.set_gravity_vector(Gaze::GodotCameraVector3(-1.0, 0.0, 0.0));
    CHECK(engine.get_gravity_vector().x == doctest::Approx(-1.0).epsilon(0.01));
    CHECK(engine.get_gravity_vector().y == doctest::Approx(0.0).epsilon(0.01));
    CHECK(engine.get_gravity_vector().z == doctest::Approx(0.0).epsilon(0.01));
}


