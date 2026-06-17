// tests/test_main.cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "projection_engine.hpp"
#include "one_euro_filter.hpp"

using namespace Gaze;

TEST_CASE("Testing custom Vector3 arithmetic") {
    GazeVector3 a(1.0, 2.0, 3.0);
    GazeVector3 b(4.0, 5.0, 6.0);

    GazeVector3 c = a + b;
    CHECK(c.x == doctest::Approx(5.0));
    CHECK(c.y == doctest::Approx(7.0));
    CHECK(c.z == doctest::Approx(9.0));

    GazeVector3 d = b - a;
    CHECK(d.x == doctest::Approx(3.0));
    CHECK(d.y == doctest::Approx(3.0));
    CHECK(d.z == doctest::Approx(3.0));

    double dot_val = a.dot(b);
    CHECK(dot_val == doctest::Approx(4.0 + 10.0 + 18.0)); // 32

    GazeVector3 cross_val = a.cross(b);
    // (2*6 - 3*5, 3*4 - 1*6, 1*5 - 2*4) = (-3, 6, -3)
    CHECK(cross_val.x == doctest::Approx(-3.0));
    CHECK(cross_val.y == doctest::Approx(6.0));
    CHECK(cross_val.z == doctest::Approx(-3.0));

    GazeVector3 norm = a.normalized();
    CHECK(norm.length() == doctest::Approx(1.0));
}

TEST_CASE("Testing 1 Euro Filter basic noise reduction") {
    // Initialize filter: mincutoff = 1.0Hz, beta = 0.0 (constant low-pass filter)
    OneEuroFilter filter(60.0, 1.0, 0.0, 1.0);

    double value = 100.0;
    double timestamp = 0.0;
    double dt = 1.0 / 60.0;

    // Initialize filter state
    double filtered = filter.filter(value, timestamp);
    CHECK(filtered == doctest::Approx(100.0));

    // Feed noisy signal centered at 100
    double sum = 0.0;
    for (int i = 0; i < 20; ++i) {
        timestamp += dt;
        double noise = (i % 2 == 0) ? 5.0 : -5.0;
        filtered = filter.filter(value + noise, timestamp);
        sum += filtered;
    }
    double avg = sum / 20.0;
    // The filter should significantly damp the oscillation (+/- 5.0)
    CHECK(std::abs(avg - 100.0) < 1.0);
    // Individual values should stay close to 100 (much smaller deviation than 5.0)
    CHECK(std::abs(filtered - 100.0) < 2.0);
}

TEST_CASE("Testing Projection Engine Math (Zero Tilt)") {
    ProjectionEngine engine;
    
    // Configure standard screen: 1920x1080px, 527x296mm
    engine.set_screen_size_pixels(GazeVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(GazeVector2(527.0, 296.0));

    // Camera placed top-center, 10mm in front of screen plane
    CameraPlacement placement(GazeVector3(0.0, -148.0, 10.0), 0.0);
    engine.set_camera_placement(placement);

    // Gaze origin (user's eyes) straight in front at 500mm, looking back
    GazeVector3 origin(0.0, 0.0, 500.0);
    GazeVector3 dir(0.0, 0.0, -1.0); // Looking straight at camera

    GazeVector2 pixel;
    bool success = engine.project_gaze(origin, dir, pixel);
    
    REQUIRE(success == true);
    // Center of screen horizontally, top of screen vertically (10mm offset below camera)
    // Camera is at y = -148mm (top edge). Gaze hits at y = 0mm camera space.
    // Screen coordinate y_s = 0 + (-148) = -148mm.
    // Since -148mm is top-center, pixel should be:
    // x = 960
    // y = 540 + (-148 * (1080 / 296)) = 540 - 540 = 0
    CHECK(pixel.x == doctest::Approx(960.0));
    CHECK(pixel.y == doctest::Approx(0.0));
}

TEST_CASE("Testing Projection Engine Math (With Tilt)") {
    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(GazeVector2(527.0, 296.0));

    // Camera top-center, tilted down by 15 degrees
    CameraPlacement placement(GazeVector3(0.0, -148.0, 10.0), 15.0);
    engine.set_camera_placement(placement);

    GazeVector3 origin(0.0, 0.0, 500.0);
    GazeVector3 dir(0.0, 0.0, -1.0); // Looking straight back along optical axis

    GazeVector2 pixel;
    bool success = engine.project_gaze(origin, dir, pixel);
    
    REQUIRE(success == true);
    // Check that vertical coordinate incorporates the 15-degree tilt
    CHECK(pixel.x == doctest::Approx(960.0));
    // Since camera is tilted down, looking straight along the optical axis projects lower down on the screen
    CHECK(pixel.y > 0.0); 
}

TEST_CASE("Testing 3D Calibration Bias Correction") {
    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(GazeVector2(527.0, 296.0));

    // Camera at top center, tilted down by 10 degrees
    CameraPlacement placement(GazeVector3(0.0, -148.0, 15.0), 10.0);
    engine.set_camera_placement(placement);

    // Initial state: no calibration
    GazeCalibration calib;
    engine.set_calibration(calib);

    // User is looking from (10.0, -20.0, 600.0) with a raw gaze direction
    GazeVector3 origin(10.0, -20.0, 600.0);
    GazeVector3 raw_dir(-0.02, 0.05, -0.99);

    // Let's say the user is instructed to look at the center of the screen (960, 540)
    GazeVector2 target(960.0, 540.0);

    // Perform 3D calibration calculation
    bool success = engine.calibrate_3d_bias(origin, raw_dir, target, calib);
    REQUIRE(success == true);

    // Check that computed angular biases are non-zero
    CHECK((calib.bias_pitch != 0.0 || calib.bias_yaw != 0.0));

    // Apply the calibration to the engine
    engine.set_calibration(calib);

    // Reproject the gaze and verify it hits the target pixel exactly
    GazeVector2 calibrated_pixel;
    success = engine.project_gaze(origin, raw_dir, calibrated_pixel);
    
    REQUIRE(success == true);
    CHECK(calibrated_pixel.x == doctest::Approx(target.x));
    CHECK(calibrated_pixel.y == doctest::Approx(target.y));
}

TEST_CASE("Testing 2D Calibration Bias Correction") {
    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(GazeVector2(527.0, 296.0));

    CameraPlacement placement(GazeVector3(10.0, -140.0, 20.0), 12.0);
    engine.set_camera_placement(placement);

    GazeCalibration calib;
    engine.set_calibration(calib);

    GazeVector3 origin(-5.0, 15.0, 550.0);
    GazeVector3 raw_dir(0.04, -0.02, -0.98);

    // Target a specific point on screen
    GazeVector2 target(1200.0, 800.0);

    // Perform 2D pixel calibration
    bool success = engine.calibrate_2d_bias(origin, raw_dir, target, calib);
    REQUIRE(success == true);

    // Check that 2D pixel biases are non-zero
    CHECK(calib.bias_pixel_x != 0.0);
    CHECK(calib.bias_pixel_y != 0.0);
    // 3D biases should remain untouched (zero)
    CHECK(calib.bias_pitch == 0.0);
    CHECK(calib.bias_yaw == 0.0);

    // Apply the calibration to the engine
    engine.set_calibration(calib);

    // Reproject and verify it hits the target pixel exactly
    GazeVector2 calibrated_pixel;
    success = engine.project_gaze(origin, raw_dir, calibrated_pixel);
    
    REQUIRE(success == true);
    CHECK(calibrated_pixel.x == doctest::Approx(target.x));
    CHECK(calibrated_pixel.y == doctest::Approx(target.y));
}
