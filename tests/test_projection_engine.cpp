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
    // Camera is at top bezel center (0, 94.25, 0) mm relative to screen center
    Gaze::CameraPlacement placement_tilted(Gaze::GazeVector3(0.0, 94.25, 0.0), -15.0);
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
