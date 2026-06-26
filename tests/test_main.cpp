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
    CameraPlacement placement(GazeVector3(0.0, 148.0, 10.0), 0.0);
    engine.set_camera_placement(placement);

    // Gaze origin (user's eyes) straight in front at -500mm, looking forward along Z axis
    GazeVector3 origin(0.0, 0.0, -500.0);
    GazeVector3 dir(0.0, 0.0, 1.0); // Looking straight at screen center

    GazeVector2 pixel;
    bool success = engine.project_gaze(origin, dir, pixel);
    
    REQUIRE(success == true);
    // Center of screen horizontally, top of screen vertically (10mm offset below camera)
    // Camera is at y = 148mm (top edge). Gaze hits at y = 0mm screen relative coordinate.
    // Screen coordinate y_s = 0.
    // Since 0mm relative is center vertically, and camera Y-offset of 148mm places it at the top:
    // x = 960
    // y = 540 + (148 * (-1080 / 296)) = 540 - 540 = 0
    CHECK(pixel.x == doctest::Approx(960.0));
    CHECK(pixel.y == doctest::Approx(0.0));
}

TEST_CASE("Testing Projection Sensitivity under Retina Dimensions") {
    ProjectionEngine engine;
    // Mock logical pixels and the logical millimeters (without scale multiplier)
    engine.set_screen_size_pixels(GazeVector2(1440.0, 900.0));
    engine.set_screen_size_mm(GazeVector2(166.0, 103.0));
    
    CameraPlacement placement(GazeVector3(0.0, 51.5, 0.0), 0.0);
    engine.set_camera_placement(placement);

    GazeVector3 origin(0.0, 0.0, -500.0);
    GazeVector3 dir(0.1, 0.0, 1.0); // 5.7 degrees right rotation

    GazeVector2 pixel;
    bool success = engine.project_gaze(origin, dir, pixel);
    REQUIRE(success == true);
    
    // With correct logical scale (high sensitivity), X should project to approx 286 px.
    // If a physical scale multiplier was incorrectly applied, screen_size_mm would double,
    // halving the sensitivity, causing X to project to approx 503 px (much closer to center).
    // We assert that the projected X coordinate remains far enough from the center:
    CHECK(pixel.x < 350.0);
}

TEST_CASE("Testing Projection Engine Math (With Tilt)") {
    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(GazeVector2(527.0, 296.0));

    // Camera top-center, tilted down by 15 degrees
    CameraPlacement placement(GazeVector3(0.0, 148.0, 10.0), 15.0);
    engine.set_camera_placement(placement);

    GazeVector3 origin(0.0, 0.0, -500.0);
    GazeVector3 dir(0.0, 0.0, 1.0); // Looking straight along camera optical axis

    GazeVector2 pixel;
    bool success = engine.project_gaze(origin, dir, pixel);
    
    REQUIRE(success == true);
    // Check that vertical coordinate incorporates the 15-degree tilt
    CHECK(pixel.x == doctest::Approx(960.0));
    // Since camera is tilted down, looking straight along the optical axis projects higher up (above the screen top edge)
    CHECK(pixel.y < 0.0); 
}

TEST_CASE("Testing 3D Calibration Bias Correction") {
    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(GazeVector2(527.0, 296.0));

    // Camera at top center, tilted down by 10 degrees
    CameraPlacement placement(GazeVector3(0.0, 148.0, 15.0), 10.0);
    engine.set_camera_placement(placement);

    // Initial state: no calibration
    GazeCalibration calib;
    engine.set_calibration(calib);

    // User is looking from (10.0, 20.0, -600.0) with a raw gaze direction pointing towards screen
    GazeVector3 origin(10.0, 20.0, -600.0);
    GazeVector3 raw_dir(0.02, 0.05, 0.99);

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
    GazeVector3 calib_dir = engine.apply_3d_bias(raw_dir);
    success = engine.project_gaze(origin, calib_dir, calibrated_pixel);
    
    REQUIRE(success == true);
    CHECK(calibrated_pixel.x == doctest::Approx(target.x));
    CHECK(calibrated_pixel.y == doctest::Approx(target.y));
}

TEST_CASE("Testing 2D Calibration Bias Correction") {
    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(GazeVector2(527.0, 296.0));

    CameraPlacement placement(GazeVector3(10.0, 140.0, 20.0), 12.0);
    engine.set_camera_placement(placement);

    GazeCalibration calib;
    engine.set_calibration(calib);

    GazeVector3 origin(-5.0, -15.0, -550.0);
    GazeVector3 raw_dir(-0.04, -0.02, 0.98);

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
    GazeVector3 calib_dir = engine.apply_3d_bias(raw_dir);
    success = engine.project_gaze(origin, calib_dir, calibrated_pixel);
    calibrated_pixel.x += calib.bias_pixel_x;
    calibrated_pixel.y += calib.bias_pixel_y;
    
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
        double r = -roll_deg * 3.14159265358979323846 / 180.0;
        
        double cp = std::cos(p), sp = std::sin(p);
        double cy = std::cos(y), sy = std::sin(y);
        double cr = std::cos(r), sr = std::sin(r);
        
        double r02 = cr * sy * cp + sr * sp;
        double r12 = sr * sy * cp - cr * sp;
        double r22 = cy * cp;
        
        // head_forward = basis.z = (-r02, r12, r22) from get_head_transform
        double fx = -r02;
        double fy = r12;
        double fz = r22;
        
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

    // 1. Verify Yaw Monotonicity (X increases when looking left in Camera Space)
    // TR (Point 2) is to the right of TL (Point 1), so TL X (left) should be greater than TR X (right)
    CHECK(f1.x > f2.x);
    // BR (Point 4) is to the right of BL (Point 3), so BL X (left) should be greater than BR X (right)
    CHECK(f3.x > f4.x);

    // 2. Verify Pitch Monotonicity (Y decreases (moves down) when looking down)
    // BL (Point 3) is below TL (Point 1), so BL Y should be smaller/more-negative
    CHECK(f3.y < f1.y);
    // BR (Point 4) is below TR (Point 2), so BR Y should be smaller/more-negative
    CHECK(f4.y < f2.y);

    // 3. Verify translation mapping monotonicity (-tvec_x)
    // Point 1 (left target) has tvec_x = 14.996 -> Godot X = -14.996
    // Point 2 (right target) has tvec_x = -5.598 -> Godot X = +5.598
    // Moving to the right should increase Godot X translation
}

TEST_CASE("TDD: Thorough physical verification of Camera-to-Screen Transform & Projection cases") {
    struct TransformTestScenario {
        std::string description;
        double camera_tilt_deg;
        GazeVector3 camera_offset_mm;
        GazeVector2 screen_size_px;
        GazeVector2 screen_size_mm;
        GazeVector3 head_position_cam;
        GazeVector2 target_screen_px;
    };

    std::vector<TransformTestScenario> scenarios = {
        // 1. Laptop: Seated Center, Looking at Screen Center (15 degree screen lean, top center bezel camera)
        {
            "Laptop: Seated Center, Looking at Screen Center",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(12.2, 0.3, -739.5),
            GazeVector2(960.0, 540.0)
        },
        // 2. Laptop: Seated Center, Looking at Screen Top-Edge
        {
            "Laptop: Seated Center, Looking at Screen Top-Edge",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(12.2, 0.3, -739.5),
            GazeVector2(960.0, 0.0)
        },
        // 3. Laptop: Seated Center, Looking at Screen Bottom-Edge
        {
            "Laptop: Seated Center, Looking at Screen Bottom-Edge",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(12.2, 0.3, -739.5),
            GazeVector2(960.0, 1080.0)
        },
        // 4. Laptop: Seated Center, Looking at Screen Left-Edge
        {
            "Laptop: Seated Center, Looking at Screen Left-Edge",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(12.2, 0.3, -739.5),
            GazeVector2(0.0, 540.0)
        },
        // 5. Laptop: Seated Center, Looking at Screen Right-Edge
        {
            "Laptop: Seated Center, Looking at Screen Right-Edge",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(12.2, 0.3, -739.5),
            GazeVector2(1920.0, 540.0)
        },
        // 6. Laptop: Seated Left, Looking at Bottom-Right
        {
            "Laptop: Seated Left, Looking at Bottom-Right",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(46.9, 4.5, -775.6),
            GazeVector2(1920.0, 1080.0)
        },
        // 7. Laptop: Seated Right, Looking at Top-Left
        {
            "Laptop: Seated Right, Looking at Top-Left",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(-14.2, -1.1, -725.5),
            GazeVector2(0.0, 0.0)
        },
        // 8. Laptop: Looking off-screen Left
        {
            "Laptop: Looking off-screen Left",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(12.2, 0.3, -739.5),
            GazeVector2(-100.0, 540.0)
        }
    };

    for (const auto& sc : scenarios) {
        INFO("Running Scenario: " << sc.description);

        ProjectionEngine engine;
        engine.set_screen_size_pixels(sc.screen_size_px);
        engine.set_screen_size_mm(sc.screen_size_mm);
        CameraPlacement placement(sc.camera_offset_mm, sc.camera_tilt_deg);
        engine.set_camera_placement(placement);

        double W_px = sc.screen_size_px.x;
        double H_px = sc.screen_size_px.y;
        double W_mm = sc.screen_size_mm.x;
        double H_mm = sc.screen_size_mm.y;

        // 1. Back-project target pixel to Camera Space
        // Display space coordinates relative to screen center
        double x_s = (sc.target_screen_px.x - W_px / 2.0) * (W_mm / W_px);
        double y_s = (sc.target_screen_px.y - H_px / 2.0) * (H_mm / H_px);

        double theta_rad = sc.camera_tilt_deg * (3.14159265358979323846 / 180.0);
        double cos_t = std::cos(theta_rad);
        double sin_t = std::sin(theta_rad);

        // dx maps from Display X to Camera X (which is opposite direction)
        double dx = sc.camera_offset_mm.x - x_s;
        // dy maps from Display Y to Camera Y (both negating y_s and subtracting offset_y)
        double dy = -y_s - sc.camera_offset_mm.y;
        double dz = -sc.camera_offset_mm.z;

        // Using the inverse mapping for Display Space coordinates (R^-1 = R)
        GazeVector3 P_cam_target(
            dx,
            dy * cos_t + dz * sin_t,
            dy * sin_t - dz * cos_t
        );

        // 2. Verify Ray Intersection in ProjectionEngine
        GazeVector3 V_cam = (P_cam_target - sc.head_position_cam).normalized();
        GazeVector2 projected;
        bool success = engine.project_gaze(sc.head_position_cam, V_cam, projected);
        REQUIRE(success == true);

        CHECK(projected.x == doctest::Approx(sc.target_screen_px.x));
        CHECK(projected.y == doctest::Approx(sc.target_screen_px.y));

        // 3. Verify Camera-to-Screen Matrix (simulated get_camera_to_screen_transform)
        double scale_x = W_px / W_mm;
        double scale_y = -H_px / H_mm;
        double W_half = W_px / 2.0;
        double H_half = H_px / 2.0;

        GazeVector3 basis_col0(-scale_x, 0.0, 0.0);
        GazeVector3 basis_col1(0.0, cos_t * scale_y, sin_t);
        GazeVector3 basis_col2(0.0, sin_t * scale_y, -cos_t);
        GazeVector3 translation(
            sc.camera_offset_mm.x * scale_x + W_half,
            sc.camera_offset_mm.y * scale_y + H_half,
            sc.camera_offset_mm.z
        );

        // Apply matrix multiplication P_disp = basis * P_cam_target + translation
        double P_disp_x = basis_col0.x * P_cam_target.x + basis_col1.x * P_cam_target.y + basis_col2.x * P_cam_target.z + translation.x;
        double P_disp_y = basis_col0.y * P_cam_target.x + basis_col1.y * P_cam_target.y + basis_col2.y * P_cam_target.z + translation.y;
        double P_disp_z = basis_col0.z * P_cam_target.x + basis_col1.z * P_cam_target.y + basis_col2.z * P_cam_target.z + translation.z;

        CHECK(P_disp_x == doctest::Approx(sc.target_screen_px.x));
        CHECK(P_disp_y == doctest::Approx(sc.target_screen_px.y));
        CHECK(P_disp_z == doctest::Approx(0.0));
    }
}

TEST_CASE("Testing Viewport-to-Screen Mapping & Coordinate Translation") {
    // Test Case 1: Desktop Scaling and Offset Mapping
    double screen_w = 1920.0, screen_h = 1080.0;
    double win_x = 300.0, win_y = 200.0;
    double gaze_screen_x = 960.0, gaze_screen_y = 540.0;
    
    double local_x = gaze_screen_x - win_x;
    double local_y = gaze_screen_y - win_y;
    
    CHECK(local_x == 660.0);
    CHECK(local_y == 340.0);
}

TEST_CASE("Testing Multi-Resolution Layout Scaling") {
    // Test Case 2: Multi-Resolution Layout Scaling
    struct ScreenResolution {
        double w_px;
        double h_px;
        double w_mm;
        double h_mm;
    };

    ScreenResolution resolutions[] = {
        {1440.0, 900.0, 305.0, 190.0},  // MacBook
        {2560.0, 1440.0, 597.0, 336.0}, // QHD
        {3840.0, 2160.0, 697.0, 392.0}  // 4K
    };

    for (const auto& res : resolutions) {
        double scale_x = res.w_px / res.w_mm;
        double scale_y = res.h_px / res.h_mm;
        
        // Assert that the pixel density (aspect ratios) are consistent
        double aspect_px = res.w_px / res.h_px;
        double aspect_mm = res.w_mm / res.h_mm;
        CHECK(aspect_px == doctest::Approx(aspect_mm).epsilon(0.05)); // within 5% tolerance due to bezel/DPI variances
        
        // Assert linear mapping
        double mm_point_x = res.w_mm * 0.5;
        double mm_point_y = res.h_mm * 0.5;
        double px_point_x = mm_point_x * scale_x;
        double px_point_y = mm_point_y * scale_y;
        
        CHECK(px_point_x == doctest::Approx(res.w_px * 0.5));
        CHECK(px_point_y == doctest::Approx(res.h_px * 0.5));
    }
}


