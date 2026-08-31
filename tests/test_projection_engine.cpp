#include "doctest.h"
#include "projection_engine.hpp"
#include "screen_projector.hpp"
#include "camera_placement.hpp"
#include <cmath>

TEST_CASE("Screen Projection Engine Boundary Invariants and Realistic Laptop Geometry")
{
    Gaze::ProjectionEngine engine;
    engine.set_screen_size_pixels(Gaze::GazeVector2(3024.0, 1964.0));
    engine.set_screen_size_mm(Gaze::GazeVector2(301.5, 188.5));

    // Scenario 1: Standard Laptop Lid Open 105 deg (Camera Tilted Down -15 deg)
    // Camera is at top bezel center (0, 0, 0) mm relative to top bezel
    Gaze::CameraPlacement placement_tilted(Gaze::GazeVector3(0.0, 0.0, 0.0), -15.0);
    engine.set_camera_placement(placement_tilted);

    // User head is below camera at (0, -50, -650) mm in Godot Camera Space
    // User faces forward (+15 deg pitch relative to tilted camera) and looks down -12 deg at screen
    Gaze::GazeVector3 origin_godot(0.0, -50.0, -650.0);
    Gaze::GazeVector3 dir_godot(0.0, -0.2079, 0.9781); // pointing toward screen plane in Godot Camera Space

    // Convert Godot Camera Space to OpenCV Camera Space for projection engine:
    // Godot Camera Space: origin (0, -50, -650), dir (0, -0.2079, 0.9781)
    // OpenCV Camera Space: origin_cv (0, +50, +650), dir_cv (0, -0.2079, -0.9781)
    Gaze::GazeVector3 origin_cv(origin_godot.x, -origin_godot.y, -origin_godot.z);
    Gaze::GazeVector3 dir_cv(dir_godot.x, dir_godot.y, -dir_godot.z);

    Gaze::GazeVector2 pixel_proj;
    bool proj_ok = engine.project_gaze(origin_cv, dir_cv, pixel_proj);

    REQUIRE(proj_ok == true);
    Gaze::GazeVector2 mm_proj = engine.pixel_to_millimeter(pixel_proj);

    // Projected point MUST land on the display surface (Y mm near screen center/lower display)
    CHECK(mm_proj.x == doctest::Approx(0.0).epsilon(0.1));
    CHECK(mm_proj.y > -94.25); // Below top bezel
    CHECK(mm_proj.y < 94.25);  // Above bottom bezel

    // Scenario 2: Off-Axis Seating (User offset Right +150mm, looking at screen center)
    Gaze::GazeVector3 off_axis_origin_godot(150.0, -20.0, -600.0);
    Gaze::GazeVector3 off_axis_dir_godot(-0.2425, 0.0323, 0.9696); // pointing toward screen center

    Gaze::GazeVector3 off_axis_origin_cv(off_axis_origin_godot.x, -off_axis_origin_godot.y, -off_axis_origin_godot.z);
    Gaze::GazeVector3 off_axis_dir_cv(off_axis_dir_godot.x, off_axis_dir_godot.y, -off_axis_dir_godot.z);

    bool off_axis_ok = engine.project_gaze(off_axis_origin_cv, off_axis_dir_cv, pixel_proj);
    REQUIRE(off_axis_ok == true);

    Gaze::GazeVector2 off_axis_mm = engine.pixel_to_millimeter(pixel_proj);
    CHECK(std::abs(off_axis_mm.x) < 50.0); // Near screen center X

    // Scenario 3: Looking Away Protection (Gaze pointing backward into room wall behind user: dir_cv.z > 0)
    Gaze::GazeVector3 away_dir_cv(0.0, 0.0, 1.0); // pointing backward (+Z in OpenCV space)
    bool away_ok = engine.project_gaze(origin_cv, away_dir_cv, pixel_proj);
    CHECK(away_ok == false); // Safe protection: ray pointing away returns false!
}

TEST_CASE("Direct Camera-to-Display 2-Space Projection Invariants")
{
    Gaze::GazeVector2 screen_size_mm(300.0, 200.0); // 300mm x 200mm monitor
    Gaze::GazeVector3 camera_offset(0.0, 0.0, 0.0);  // Top bezel center (X=150mm, Y=0mm)
    double tilt_deg = 0.0;

    // Invariant 1: Staring directly at top-bezel camera from 500mm away in Godot Camera Space
    // Origin (0, 0, -500), Dir (0, 0, +1)
    Gaze::GazeVector3 orig_center(0.0, 0.0, -500.0);
    Gaze::GazeVector3 dir_at_cam(0.0, 0.0, 1.0);
    Gaze::GazeVector2 pos_mm;
    bool ok1 = Gaze::project_ray_to_screen_mm(orig_center, dir_at_cam, camera_offset, tilt_deg, screen_size_mm, pos_mm);
    REQUIRE(ok1 == true);
    CHECK(pos_mm.x == doctest::Approx(150.0).epsilon(0.01)); // Screen center X = W_mm / 2
    CHECK(pos_mm.y == doctest::Approx(0.0).epsilon(0.01));   // Top bezel Y = 0 mm

    // Invariant 2: Looking downward from (0, 0, -500) towards screen center (150mm, 100mm)
    // Target is at camera space (0, -100, 0), so dir = (0, -100, 500).normalized()
    Gaze::GazeVector3 dir_to_center = Gaze::GazeVector3(0.0, -100.0, 500.0).normalized();
    Gaze::GazeVector2 pos_center_mm;
    bool ok2 = Gaze::project_ray_to_screen_mm(orig_center, dir_to_center, camera_offset, tilt_deg, screen_size_mm, pos_center_mm);
    REQUIRE(ok2 == true);
    CHECK(pos_center_mm.x == doctest::Approx(150.0).epsilon(0.01)); // Screen center X = 150 mm
    CHECK(pos_center_mm.y == doctest::Approx(100.0).epsilon(0.01)); // Screen center Y = 100 mm

    // Invariant 3: Looking Camera-Left (-X_cam) lands on Display Right (X > 150mm)
    Gaze::GazeVector3 dir_cam_left = Gaze::GazeVector3(-100.0, -100.0, 500.0).normalized();
    Gaze::GazeVector2 pos_right_mm;
    bool ok3 = Gaze::project_ray_to_screen_mm(orig_center, dir_cam_left, camera_offset, tilt_deg, screen_size_mm, pos_right_mm);
    REQUIRE(ok3 == true);
    CHECK(pos_right_mm.x > 150.0); // Display Right
    CHECK(pos_right_mm.y == doctest::Approx(100.0).epsilon(0.01));

    // Invariant 3b: Looking Camera-Right (+X_cam) lands on Display Left (X < 150mm)
    Gaze::GazeVector3 dir_cam_right = Gaze::GazeVector3(100.0, -100.0, 500.0).normalized();
    Gaze::GazeVector2 pos_left_mm;
    bool ok3b = Gaze::project_ray_to_screen_mm(orig_center, dir_cam_right, camera_offset, tilt_deg, screen_size_mm, pos_left_mm);
    REQUIRE(ok3b == true);
    CHECK(pos_left_mm.x < 150.0); // Display Left
    CHECK(pos_left_mm.y == doctest::Approx(100.0).epsilon(0.01));

    // Invariant 4: Focal length calculation with standard 65 deg FOV
    double f1440 = Gaze::calculate_default_focal_length(1440.0, 65.0);
    CHECK(f1440 == doctest::Approx(1130.16).epsilon(0.01));
}

