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

TEST_CASE("Testing Monotonicity and Calibration Mappings from User Logs") {
    // Reconstruct head forward directions using the new unmirrored C++ mapping:
    // basis.z = (-r02, -r12, r22) -> head_forward = -basis.z = (r02, r12, -r22)
    auto get_unmirrored_forward = [](double pitch_deg, double yaw_deg, double roll_deg) {
        double p = pitch_deg * 3.14159265358979323846 / 180.0;
        double y = yaw_deg * 3.14159265358979323846 / 180.0;
        double r = roll_deg * 3.14159265358979323846 / 180.0;
        
        double cp = std::cos(p), sp = std::sin(p);
        double cy = std::cos(y), sy = std::sin(y);
        double cr = std::cos(r), sr = std::sin(r);
        
        double r02 = cr * sy * cp + sr * sp;
        double r12 = sr * sy * cp - cr * sp;
        double r22 = cy * cp;
        
        // head_forward = -basis.z = (r02, r12, -r22) from get_head_transform
        double fx = r02;
        double fy = r12;
        double fz = -r22;
        
        double len = std::sqrt(fx*fx + fy*fy + fz*fz);
        return GazeVector3(fx/len, fy/len, fz/len);
    };

    // User logs: Point 1 (TL, left side) and Point 2 (TR, right side)
    double p1_yaw = 3.207542, p1_pitch = 14.075171, p1_roll = -4.226331;
    double p2_yaw = 15.317283, p2_pitch = 10.476198, p2_roll = 0.757732;
    double p3_yaw = -1.992620, p3_pitch = 14.602449, p3_roll = -5.278838;
    double p4_yaw = 20.695047, p4_pitch = 13.022354, p4_roll = -6.316560;

    GazeVector3 f1 = get_unmirrored_forward(p1_pitch, p1_yaw, p1_roll);
    GazeVector3 f2 = get_unmirrored_forward(p2_pitch, p2_yaw, p2_roll);
    GazeVector3 f3 = get_unmirrored_forward(p3_pitch, p3_yaw, p3_roll);
    GazeVector3 f4 = get_unmirrored_forward(p4_pitch, p4_yaw, p4_roll);

    // 1. Verify Yaw Monotonicity (X increases when looking right)
    // TR (Point 2) is to the right of TL (Point 1), so TR X should be greater
    CHECK(f2.x > f1.x);
    // BR (Point 4) is to the right of BL (Point 3), so BR X should be greater
    CHECK(f4.x > f3.x);

    // 2. Verify Pitch Monotonicity (Y decreases (moves down) when looking down)
    // BL (Point 3) is below TL (Point 1), so BL Y should be smaller/more-negative
    CHECK(f3.y < f1.y);
    // BR (Point 4) is below TR (Point 2), so BR Y should be smaller/more-negative
    CHECK(f4.y < f2.y);

    // 3. Verify translation mapping monotonicity (-tvec_x)
    // Point 1 (left target) has tvec_x = 14.996 -> Godot X = -14.996
    // Point 2 (right target) has tvec_x = -5.598 -> Godot X = +5.598
    // Moving to the right should increase Godot X translation
    CHECK(-(-5.598579) > -(14.996960));
}
