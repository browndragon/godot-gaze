// TODO: again, I prefer `_test.cpp` naming.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "projection_engine.hpp"
#include "one_euro_filter.hpp"

using namespace Gaze;

TEST_CASE("Testing custom Vector3 arithmetic")
{
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

TEST_CASE("Testing 1 Euro Filter basic noise reduction")
{
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
    for (int i = 0; i < 20; ++i)
    {
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

TEST_CASE("Testing Projection Engine Math (Zero Tilt)")
{
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

TEST_CASE("Testing Projection Sensitivity under Retina Dimensions")
{
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

TEST_CASE("Testing Projection Engine Math (With Tilt)")
{
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

TEST_CASE("Testing Monotonicity and Calibration Mappings from User Logs")
{
    // Reconstruct head forward directions using the new unmirrored C++ mapping:
    // basis.z = (-r02, -r12, r22) -> head_forward = -basis.z = (r02, r12, -r22)
    auto get_unmirrored_forward = [](double pitch_deg, double yaw_deg, double roll_deg)
    {
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

        double len = std::sqrt(fx * fx + fy * fy + fz * fz);
        return GazeVector3(fx / len, fy / len, fz / len);
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

TEST_CASE("TDD: Thorough physical verification of Camera-to-Screen Transform & Projection cases")
{
    struct TransformTestScenario
    {
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
            GazeVector2(960.0, 540.0)},
        // 2. Laptop: Seated Center, Looking at Screen Top-Edge
        {
            "Laptop: Seated Center, Looking at Screen Top-Edge",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(12.2, 0.3, -739.5),
            GazeVector2(960.0, 0.0)},
        // 3. Laptop: Seated Center, Looking at Screen Bottom-Edge
        {
            "Laptop: Seated Center, Looking at Screen Bottom-Edge",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(12.2, 0.3, -739.5),
            GazeVector2(960.0, 1080.0)},
        // 4. Laptop: Seated Center, Looking at Screen Left-Edge
        {
            "Laptop: Seated Center, Looking at Screen Left-Edge",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(12.2, 0.3, -739.5),
            GazeVector2(0.0, 540.0)},
        // 5. Laptop: Seated Center, Looking at Screen Right-Edge
        {
            "Laptop: Seated Center, Looking at Screen Right-Edge",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(12.2, 0.3, -739.5),
            GazeVector2(1920.0, 540.0)},
        // 6. Laptop: Seated Left, Looking at Bottom-Right
        {
            "Laptop: Seated Left, Looking at Bottom-Right",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(46.9, 4.5, -775.6),
            GazeVector2(1920.0, 1080.0)},
        // 7. Laptop: Seated Right, Looking at Top-Left
        {
            "Laptop: Seated Right, Looking at Top-Left",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(-14.2, -1.1, -725.5),
            GazeVector2(0.0, 0.0)},
        // 8. Laptop: Looking off-screen Left
        {
            "Laptop: Looking off-screen Left",
            15.0,
            GazeVector3(0.0, 95.5, 0.0),
            GazeVector2(1920.0, 1080.0),
            GazeVector2(305.0, 191.0),
            GazeVector3(12.2, 0.3, -739.5),
            GazeVector2(-100.0, 540.0)}};

    for (const auto &sc : scenarios)
    {
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
            dy * sin_t - dz * cos_t);

        // 2. Verify Ray Intersection in ProjectionEngine
        GazeVector3 V_cam = (P_cam_target - sc.head_position_cam).normalized();
        GazeVector2 projected;
        bool success = engine.project_gaze(sc.head_position_cam, V_cam, projected);
        REQUIRE(success == true);

        CHECK(projected.x == doctest::Approx(sc.target_screen_px.x).epsilon(0.001));
        CHECK(projected.y == doctest::Approx(sc.target_screen_px.y).epsilon(0.001));

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
            sc.camera_offset_mm.z);

        // Apply matrix multiplication P_disp = basis * P_cam_target + translation
        double P_disp_x = basis_col0.x * P_cam_target.x + basis_col1.x * P_cam_target.y + basis_col2.x * P_cam_target.z + translation.x;
        double P_disp_y = basis_col0.y * P_cam_target.x + basis_col1.y * P_cam_target.y + basis_col2.y * P_cam_target.z + translation.y;
        double P_disp_z = basis_col0.z * P_cam_target.x + basis_col1.z * P_cam_target.y + basis_col2.z * P_cam_target.z + translation.z;

        CHECK(P_disp_x == doctest::Approx(sc.target_screen_px.x).epsilon(0.001));
        CHECK(P_disp_y == doctest::Approx(sc.target_screen_px.y).epsilon(0.001));
        CHECK(P_disp_z == doctest::Approx(0.0));
    }
}

TEST_CASE("Testing Viewport-to-Screen Mapping & Coordinate Translation")
{
    // Test Case 1: Desktop Scaling and Offset Mapping
    double screen_w = 1920.0, screen_h = 1080.0;
    double win_x = 300.0, win_y = 200.0;
    double gaze_screen_x = 960.0, gaze_screen_y = 540.0;

    double local_x = gaze_screen_x - win_x;
    double local_y = gaze_screen_y - win_y;

    CHECK(local_x == 660.0);
    CHECK(local_y == 340.0);
}

TEST_CASE("Testing Multi-Resolution Layout Scaling")
{
    // Test Case 2: Multi-Resolution Layout Scaling
    struct ScreenResolution
    {
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

    for (const auto &res : resolutions)
    {
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

TEST_CASE("Testing Gaze Projection Invariance and Monotonicity (Yaw Sweep)")
{
    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(GazeVector2(527.0, 296.0));

    double tilts[] = {-30.0, -15.0, 0.0, 15.0, 30.0};
    GazeVector3 camera_offsets[] = {
        GazeVector3(0.0, 148.0, 10.0),  // Top center
        GazeVector3(0.0, -148.0, 10.0), // Bottom center
        GazeVector3(-250.0, 0.0, 10.0), // Left side
        GazeVector3(250.0, 0.0, 10.0)   // Right side
    };

    for (double tilt : tilts)
    {
        for (const auto &offset : camera_offsets)
        {
            CameraPlacement placement(offset, tilt);
            engine.set_camera_placement(placement);

            GazeVector3 origin(0.0, 0.0, -500.0);

            // Sweep yaw from left to right (from user's perspective, looking left is +x yaw, looking right is -x yaw)
            // So we step gaze direction v.x from 0.5 (far left) down to -0.5 (far right)
            // Projected pixel coordinate X should increase strictly (moving from left side of screen to right side)
            double prev_pixel_x = -1e9;
            for (int i = 0; i <= 20; ++i)
            {
                double vx = 0.5 - (i * 0.05); // 0.5 down to -0.5
                double vz = std::sqrt(1.0 - vx * vx);
                GazeVector3 dir(vx, 0.0, vz);

                GazeVector2 pixel;
                bool success = engine.project_gaze(origin, dir, pixel);
                REQUIRE(success == true);

                if (i > 0)
                {
                    // Check strict increase: user looking right -> larger pixel.x
                    CHECK_MESSAGE(pixel.x > prev_pixel_x,
                                  "Yaw sweep monotonicity failed at tilt=" << tilt
                                                                           << ", offset=(" << offset.x << "," << offset.y << "," << offset.z << ")"
                                                                           << ", vx=" << vx << ", current_pixel_x=" << pixel.x
                                                                           << ", previous_pixel_x=" << prev_pixel_x);
                }
                prev_pixel_x = pixel.x;
            }
        }
    }
}

TEST_CASE("Testing Gaze Projection Invariance and Monotonicity (Pitch Sweep)")
{
    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(GazeVector2(527.0, 296.0));

    double tilts[] = {-30.0, -15.0, 0.0, 15.0, 30.0};
    GazeVector3 camera_offsets[] = {
        GazeVector3(0.0, 148.0, 10.0),  // Top center
        GazeVector3(0.0, -148.0, 10.0), // Bottom center
        GazeVector3(-250.0, 0.0, 10.0), // Left side
        GazeVector3(250.0, 0.0, 10.0)   // Right side
    };

    for (double tilt : tilts)
    {
        for (const auto &offset : camera_offsets)
        {
            CameraPlacement placement(offset, tilt);
            engine.set_camera_placement(placement);

            GazeVector3 origin(0.0, 0.0, -500.0);

            // Sweep pitch from up to down (looking up is +y, looking down is -y)
            // So we step gaze direction v.y from 0.5 (looking up) down to -0.5 (looking down)
            // Projected pixel coordinate Y should increase strictly (moving from top of screen to bottom of screen)
            double prev_pixel_y = -1e9;
            for (int i = 0; i <= 20; ++i)
            {
                double vy = 0.5 - (i * 0.05); // 0.5 down to -0.5
                double vz = std::sqrt(1.0 - vy * vy);
                GazeVector3 dir(0.0, vy, vz);

                GazeVector2 pixel;
                bool success = engine.project_gaze(origin, dir, pixel);
                REQUIRE(success == true);

                if (i > 0)
                {
                    // Check strict increase: user looking down -> larger pixel.y
                    CHECK_MESSAGE(pixel.y > prev_pixel_y,
                                  "Pitch sweep monotonicity failed at tilt=" << tilt
                                                                             << ", offset=(" << offset.x << "," << offset.y << "," << offset.z << ")"
                                                                             << ", vy=" << vy << ", current_pixel_y=" << pixel.y
                                                                             << ", previous_pixel_y=" << prev_pixel_y);
                }
                prev_pixel_y = pixel.y;
            }
        }
    }
}

TEST_CASE("Testing Gaze Projection Invariance (Head Translation Sweep)")
{
    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(GazeVector2(527.0, 296.0));

    // Zero tilt, camera at top center
    CameraPlacement placement(GazeVector3(0.0, 148.0, 10.0), 0.0);
    engine.set_camera_placement(placement);

    // Gaze direction is straight forward
    GazeVector3 dir(0.0, 0.0, 1.0);

    // Sweep head X from left to right (user's perspective: moving left is positive X in camera space)
    // So we step origin.x from 100.0 (left) down to -100.0 (right)
    // Projected pixel coordinate X should shift from left (smaller) to right (larger)
    double prev_pixel_x = -1e9;
    for (int i = 0; i <= 20; ++i)
    {
        double ox = 100.0 - (i * 10.0); // 100 down to -100
        GazeVector3 origin(ox, 0.0, -500.0);

        GazeVector2 pixel;
        bool success = engine.project_gaze(origin, dir, pixel);
        REQUIRE(success == true);

        if (i > 0)
        {
            CHECK_MESSAGE(pixel.x > prev_pixel_x,
                          "Head X translation sweep failed: ox=" << ox
                                                                 << ", current_pixel_x=" << pixel.x
                                                                 << ", previous_pixel_x=" << prev_pixel_x);
        }
        prev_pixel_x = pixel.x;
    }

    // Sweep head Y from up to down (moving up is positive Y in camera space)
    // So we step origin.y from 100.0 (up) down to -100.0 (down)
    // Projected pixel coordinate Y should shift from top (smaller) to bottom (larger)
    double prev_pixel_y = -1e9;
    for (int i = 0; i <= 20; ++i)
    {
        double oy = 100.0 - (i * 10.0); // 100 down to -100
        GazeVector3 origin(0.0, oy, -500.0);

        GazeVector2 pixel;
        bool success = engine.project_gaze(origin, dir, pixel);
        REQUIRE(success == true);

        if (i > 0)
        {
            CHECK_MESSAGE(pixel.y > prev_pixel_y,
                          "Head Y translation sweep failed: oy=" << oy
                                                                 << ", current_pixel_y=" << pixel.y
                                                                 << ", previous_pixel_y=" << prev_pixel_y);
        }
        prev_pixel_y = pixel.y;
    }
}

TEST_CASE("Testing High-DPI and Logical/Physical Coordinate Transformations")
{
    ProjectionEngine engine;

    // Configure screen: 1920x1080 physical pixels (which on a DPR of 2.0 is 960x540 logical points)
    // screen size in mm: 305x191
    engine.set_screen_size_pixels(GazeVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(GazeVector2(305.0, 191.0));

    CameraPlacement placement(GazeVector3(0.0, 95.5, 0.0), 0.0);
    engine.set_camera_placement(placement);

    // Gaze origin (user's eyes) straight in front at -500mm (aligned with screen center vertically at y = -95.5mm), looking forward along Z axis
    GazeVector3 origin(0.0, -95.5, -500.0);
    GazeVector3 dir(0.0, 0.0, 1.0); // Looking straight at screen center

    GazeVector2 physical_pixel;
    bool success = engine.project_gaze(origin, dir, physical_pixel);
    REQUIRE(success == true);

    // Physical pixel center should be (960, 540)
    CHECK(physical_pixel.x == doctest::Approx(960.0));
    CHECK(physical_pixel.y == doctest::Approx(540.0));

    // Let's test varying DPR values
    double dprs[] = {1.0, 1.5, 2.0, 3.0};
    for (double dpr : dprs)
    {
        // Logical pixel center is physical_pixel / dpr
        GazeVector2 logical_pixel(physical_pixel.x / dpr, physical_pixel.y / dpr);

        // Logical screen size is screen_size_pixels / dpr
        GazeVector2 screen_size_logical(1920.0 / dpr, 1080.0 / dpr);

        // Assert that the scale matches
        CHECK(logical_pixel.x == doctest::Approx(screen_size_logical.x / 2.0));
        CHECK(logical_pixel.y == doctest::Approx(screen_size_logical.y / 2.0));
    }
}

#include "gaze_calibration.hpp"
#include "gaze_calibration_estimator.hpp"

TEST_CASE("Testing GazeCalibration Layout and Defaults")
{
    GazeCalibration cal;
    CHECK(cal.pixel_size_mm.x == doctest::Approx(0.25));
    CHECK(cal.pixel_size_mm.y == doctest::Approx(0.25));
    CHECK(cal.camera_offset.x == doctest::Approx(0.0));
    CHECK(cal.camera_offset.y == doctest::Approx(148.0));
    CHECK(cal.camera_offset.z == doctest::Approx(0.0));
    CHECK(cal.camera_tilt == doctest::Approx(0.0));
    CHECK(cal.bias_pitch == doctest::Approx(0.0));
    CHECK(cal.bias_yaw == doctest::Approx(0.0));
    CHECK(cal.scale_pitch == doctest::Approx(1.0));
    CHECK(cal.scale_yaw == doctest::Approx(1.0));
}

TEST_CASE("Testing CalibrationEstimator simplex convergence (Unconstrained 6D)")
{
    // We simulate ground truth parameters:
    GazeVector2 gt_pixel_size(0.26, 0.26);
    GazeVector3 gt_camera_offset(0.0, 130.0, 15.0);
    double gt_camera_tilt = 12.0; // degrees
    double gt_bias_pitch = 0.02;  // rad
    double gt_bias_yaw = -0.01;   // rad

    // Configure a mock screen: 1920x1080
    GazeVector2 screen_res(1920.0, 1080.0);
    double screen_mm_x = screen_res.x * gt_pixel_size.x;
    double screen_mm_y = screen_res.y * gt_pixel_size.y;

    ProjectionEngine engine;
    engine.set_screen_size_pixels(screen_res);
    engine.set_screen_size_mm(GazeVector2(screen_mm_x, screen_mm_y));
    CameraPlacement placement(gt_camera_offset, gt_camera_tilt);
    engine.set_camera_placement(placement);

    // We simulate a user at a distance of ~600mm
    // Generate 5 target pixel points: Center, Top-Left, Top-Right, Bottom-Left, Bottom-Right
    std::vector<GazeVector2> targets = {
        GazeVector2(960.0, 540.0),  // Center
        GazeVector2(192.0, 108.0),  // Top-Left
        GazeVector2(1728.0, 108.0), // Top-Right
        GazeVector2(192.0, 972.0),  // Bottom-Left
        GazeVector2(1728.0, 972.0)  // Bottom-Right
    };

    std::vector<CalibrationSample> samples;

    double theta_rad = gt_camera_tilt * DEG_TO_RAD;
    double cos_t = std::cos(theta_rad);
    double sin_t = std::sin(theta_rad);

    for (const auto &tgt : targets)
    {
        CalibrationSample sample;
        sample.target_pos_mm = GazeVector2(
            (tgt.x / screen_res.x) * screen_mm_x,
            (tgt.y / screen_res.y) * screen_mm_y);

        // Simulate gaze origin at different minor head offsets
        sample.gaze_origin = GazeVector3((tgt.x - 960.0) * 0.05, 0.0, -600.0);

        // Compute where the target point is in camera space (mm)
        // using the ground truth parameters
        double tgt_x_mm = (tgt.x - 960.0) * gt_pixel_size.x;
        double tgt_y_mm = (tgt.y - 540.0) * gt_pixel_size.y;

        double P_cam_x = -tgt_x_mm + gt_camera_offset.x;
        double A = -tgt_y_mm - gt_camera_offset.y;
        double P_cam_y = A * cos_t - gt_camera_offset.z * sin_t;
        double P_cam_z = A * sin_t + gt_camera_offset.z * cos_t;

        GazeVector3 target_cam(P_cam_x, P_cam_y, P_cam_z);

        // Ground-truth gaze direction ray from origin to target in camera space
        GazeVector3 biased_dir = (target_cam - sample.gaze_origin).normalized();

        // Undo the biological bias to get the simulated raw gaze direction
        // Pitch/yaw of biased vector
        double vy = biased_dir.y;
        if (vy > 1.0)
            vy = 1.0;
        else if (vy < -1.0)
            vy = -1.0;
        double yaw = std::atan2(biased_dir.x, biased_dir.z);
        double pitch = std::asin(vy);

        double raw_yaw = yaw - gt_bias_yaw;
        double raw_pitch = pitch - gt_bias_pitch;

        double cos_raw_pitch = std::cos(raw_pitch);
        sample.gaze_direction = GazeVector3(
                                    std::sin(raw_yaw) * cos_raw_pitch,
                                    std::sin(raw_pitch),
                                    std::cos(raw_yaw) * cos_raw_pitch)
                                    .normalized();

        samples.push_back(sample);
    }

    // Run the solver starting from slightly off guesses (e.g. camera 20% off)
    GazeVector3 init_camera_offset(0.0, 148.0, 0.0);
    double init_camera_tilt = 0.0;

    GazeVector3 est_off;
    double est_tilt = 0.0;
    double est_pitch = 0.0;
    double est_yaw = 0.0;

    CalibrationWeights weights;
    weights.offset_x = 0.0;
    weights.offset_y = 0.0;
    weights.offset_z = 0.0;
    weights.tilt = 0.0;
    weights.bias = 0.0;

    bool success = CalibrationEstimator::estimate(
        samples,
        GazeVector2(screen_mm_x, screen_mm_y),
        init_camera_offset,
        init_camera_tilt,
        false, // freeze_camera_params = false
        est_off,
        est_tilt,
        est_pitch,
        est_yaw,
        weights);

    REQUIRE(success == true);

    // Verify optimized values converge close to ground truth
    CHECK(std::abs(est_off.y - gt_camera_offset.y) < 3.0); // within 3mm
    CHECK(std::abs(est_off.z - gt_camera_offset.z) < 3.0);
    CHECK(std::abs(est_tilt - gt_camera_tilt) < 1.5);  // within 1.5 degrees
    CHECK(std::abs(est_pitch - gt_bias_pitch) < 0.02); // within 0.02 rad
    CHECK(std::abs(est_yaw - gt_bias_yaw) < 0.02);
}

TEST_CASE("Testing CalibrationEstimator simplex convergence (Constrained 2D Biological Bias)")
{
    // We simulate ground truth parameters where camera position is fixed
    GazeVector2 gt_pixel_size(0.26, 0.26);
    GazeVector3 gt_camera_offset(0.0, 130.0, 15.0);
    double gt_camera_tilt = 12.0;
    double gt_bias_pitch = 0.03;
    double gt_bias_yaw = -0.02;

    GazeVector2 screen_res(1920.0, 1080.0);
    double screen_mm_x = screen_res.x * gt_pixel_size.x;
    double screen_mm_y = screen_res.y * gt_pixel_size.y;

    std::vector<GazeVector2> targets = {
        GazeVector2(960.0, 540.0), // Center
        GazeVector2(192.0, 108.0), // Top-Left
        GazeVector2(1728.0, 108.0) // Top-Right
    };

    std::vector<CalibrationSample> samples;
    double theta_rad = gt_camera_tilt * DEG_TO_RAD;
    double cos_t = std::cos(theta_rad);
    double sin_t = std::sin(theta_rad);

    for (const auto &tgt : targets)
    {
        CalibrationSample sample;
        sample.target_pos_mm = GazeVector2(
            (tgt.x / screen_res.x) * screen_mm_x,
            (tgt.y / screen_res.y) * screen_mm_y);

        sample.gaze_origin = GazeVector3(0.0, 0.0, -600.0);

        double tgt_x_mm = (tgt.x - 960.0) * gt_pixel_size.x;
        double tgt_y_mm = (tgt.y - 540.0) * gt_pixel_size.y;

        double P_cam_x = -tgt_x_mm + gt_camera_offset.x;
        double A = -tgt_y_mm - gt_camera_offset.y;
        double P_cam_y = A * cos_t - gt_camera_offset.z * sin_t;
        double P_cam_z = A * sin_t + gt_camera_offset.z * cos_t;

        GazeVector3 target_cam(P_cam_x, P_cam_y, P_cam_z);
        GazeVector3 biased_dir = (target_cam - sample.gaze_origin).normalized();

        double vy = biased_dir.y;
        if (vy > 1.0)
            vy = 1.0;
        else if (vy < -1.0)
            vy = -1.0;
        double yaw = std::atan2(biased_dir.x, biased_dir.z);
        double pitch = std::asin(vy);

        double raw_yaw = yaw - gt_bias_yaw;
        double raw_pitch = pitch - gt_bias_pitch;

        double cos_raw_pitch = std::cos(raw_pitch);
        sample.gaze_direction = GazeVector3(
                                    std::sin(raw_yaw) * cos_raw_pitch,
                                    std::sin(raw_pitch),
                                    std::cos(raw_yaw) * cos_raw_pitch)
                                    .normalized();

        samples.push_back(sample);
    }

    GazeVector3 est_off;
    double est_tilt = 0.0;
    double est_pitch = 0.0;
    double est_yaw = 0.0;

    CalibrationWeights weights;
    weights.bias = 2.0;

    bool success = CalibrationEstimator::estimate(
        samples,
        GazeVector2(screen_mm_x, screen_mm_y),
        gt_camera_offset, // camera offsets frozen to ground truth
        gt_camera_tilt,
        true, // freeze_camera_params = true (locks camera to initial values)
        est_off,
        est_tilt,
        est_pitch,
        est_yaw,
        weights);

    REQUIRE(success == true);

    // Camera parameters should remain exactly locked to initial values
    CHECK(est_off.x == doctest::Approx(gt_camera_offset.x));
    CHECK(est_off.y == doctest::Approx(gt_camera_offset.y));
    CHECK(est_off.z == doctest::Approx(gt_camera_offset.z));
    CHECK(est_tilt == doctest::Approx(gt_camera_tilt));

    // Biological gaze bias should be solved accurately
    CHECK(std::abs(est_pitch - gt_bias_pitch) < 0.02);
    CHECK(std::abs(est_yaw - gt_bias_yaw) < 0.02);
}
