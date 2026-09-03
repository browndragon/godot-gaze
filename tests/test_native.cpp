// TODO: Name `native_test.cpp`. How does this differ `main_test.cpp`?
#include "doctest.h"
#include "ort_yunet_detector.hpp"
#include "ort_landmark_model.hpp"
#include "ort_gaze_model.hpp"
#include "gaze_tracking_pipeline.hpp"
#include "projection_engine.hpp"
#include "screen_projector.hpp"
#include "opencv_space_conversions.hpp"
#include "pnp_solver.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct LoadedImage
{
    int width = 0;
    int height = 0;
    std::vector<unsigned char> data; // BGR format
};

inline LoadedImage load_test_image(const std::string &filepath)
{
    LoadedImage result;
    int width = 0, height = 0, channels = 0;
    unsigned char *data = stbi_load(filepath.c_str(), &width, &height, &channels, 3);
    if (!data)
    {
        return result;
    }
    result.width = width;
    result.height = height;
    result.data.resize(width * height * 3);

    // stbi_load returns RGB, we need BGR
    for (int i = 0; i < width * height; ++i)
    {
        result.data[i * 3 + 0] = data[i * 3 + 2]; // B
        result.data[i * 3 + 1] = data[i * 3 + 1]; // G
        result.data[i * 3 + 2] = data[i * 3 + 0]; // R
    }
    stbi_image_free(data);
    return result;
}
#include <cstring>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <thread>

using namespace Gaze;

TEST_CASE("Testing Native Pipeline Model Initialization & Inference")
{
    try
    {
        // 1. Initialize YuNet Face Detector
        std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        ORTYuNetDetector detector(yunet_path);

        REQUIRE(detector.initialize() == true);

        // 2. Initialize Gaze Model
        std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";
        ORTGazeModel model(gaze_path);

        REQUIRE(model.initialize() == true);

        // 3. Verify YuNet process_frame
        Frame frame;
        frame.width = 640;
        frame.height = 480;
        frame.timestamp = 0.0;

        std::vector<unsigned char> dummy_mat(640 * 480 * 3, 255);
        frame.data = dummy_mat.data();

        YuNetResult yunet_res;
        detector.process_frame(frame, yunet_res);
        CHECK(yunet_res.face_detected == false);

        // 4. Test Gaze Model inference on mock crop data
        EyeCrops crops;
        crops.face_detected = true;
        crops.head_pose_rotation = OpenCVCameraVector3(0.0, 0.0, 0.0);
        crops.head_pose_translation = OpenCVCameraVector3(0.0, 0.0, 500.0);
        crops.left_eye_center_cam = GodotCameraVector3(31.5, 0.0, 480.0);
        crops.right_eye_center_cam = GodotCameraVector3(-31.5, 0.0, 480.0);

        std::memset(crops.left_eye_data, 128, 10800);
        std::memset(crops.right_eye_data, 128, 10800);

        OpenVINOGazeVector3 gaze_dir_cv;
        bool model_success = model.estimate_raw_gaze(crops, gaze_dir_cv);
        REQUIRE(model_success == true);

        CHECK(gaze_dir_cv.length() == doctest::Approx(1.0));
        CHECK(std::abs(gaze_dir_cv.z) > 0.0);
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n!!! TEST CRASHED WITH EXCEPTION: " << e.what() << "\n"
                  << std::endl;
        REQUIRE(false);
    }
    catch (...)
    {
        std::cerr << "\n!!! TEST CRASHED WITH UNKNOWN EXCEPTION\n"
                  << std::endl;
        REQUIRE(false);
    }

}

TEST_CASE("Testing OpenCV Camera Model Scaling and Cropping helpers")
{
    // Test Scaling helper
    double f_scaled = Gaze::get_focal_length_under_scaling(1000.0, 640.0, 1280.0);
    CHECK(f_scaled == doctest::Approx(2000.0));

    double f_scaled_down = Gaze::get_focal_length_under_scaling(1000.0, 640.0, 320.0);
    CHECK(f_scaled_down == doctest::Approx(500.0));

    // Test Card pixel calculation helper
    // If HFOV = 53.13 degrees (tan(FOV/2) = 0.5), card width = 85.603 mm, distance = 500 mm, frame width = 640
    // Expected card pixel width: (640 * 85.603) / (2 * 500 * 0.5) = (54785.92) / 500 = 109.57184 px
    double card_px = Gaze::get_card_width_px(53.13, 500.0, 640.0, 85.603);
    CHECK(card_px == doctest::Approx(109.57184));

    // Test Diagonal to Horizontal FOV conversion helper
    // 4:3 screen (width=4, height=3, diagonal=5)
    // Diagonal FOV = 75 degrees
    // expected: HFOV = 2 * atan((4/5) * tan(37.5 deg)) = 2 * atan(0.8 * 0.767327) = 2 * atan(0.61386) = 2 * 31.54 deg = 63.08 degrees
    double hfov = Gaze::diagonal_to_horizontal_fov(75.0, 4.0, 3.0);
    CHECK(hfov == doctest::Approx(63.08).epsilon(0.01));
}

TEST_CASE("Testing Facial Landmarks and Head Pose Diagnostics")
{
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    ORTYuNetDetector detector(yunet_path);
    REQUIRE(detector.initialize() == true);

    LoadedImage img = load_test_image("tests/resources/self_center.jpg");
    REQUIRE(!img.data.empty());

    Frame frame;
    frame.width = img.width;
    frame.height = img.height;
    frame.timestamp = 0.0;
    frame.data = img.data.data();

    YuNetResult yunet_res;
    bool pipeline_success = detector.process_frame(frame, yunet_res);
    REQUIRE(pipeline_success == true);
    REQUIRE(yunet_res.face_detected == true);

    // Verify head forward vector direction in standard Camera Space
    GodotFaceTransform3D head_transform = Gaze::CoordinateConversions::opencv_pose_to_godot_camera_transform(yunet_res.head_pose.translation(), yunet_res.head_pose.rotation_vector());
    GodotCameraVector3 head_forward = -head_transform.basis.z;

    // For a forward-facing head, the forward vector should point towards the screen (+Z_cam in Godot camera space)
    CHECK(head_forward.z > 0.0);
}

TEST_CASE("Testing Viewport and High-DPI Projection Coordinates")
{
    ProjectionEngine engine;
    engine.set_screen_size_pixels(GodotDisplayVector2(3840.0, 2160.0)); // 4K physical screen
    engine.set_screen_size_mm(SpacedVector2<Space::GodotDisplayMm>(600.0, 340.0));
    engine.set_camera_placement(CameraPlacement(GodotCameraVector3(0, 0, 0), 0.0));

    // Staring at the center of the screen (170mm below top bezel camera)
    GodotCameraVector3 origin(0.0, 0.0, -600.0);
    GodotCameraVector3 direction = GodotCameraVector3(0.0, -170.0, 600.0).normalized();
    GodotDisplayVector2 pixel;
    REQUIRE(engine.project_gaze(origin, direction, pixel) == true);

    // Physical screen center check
    CHECK(pixel.x == doctest::Approx(1920.0));
    CHECK(pixel.y == doctest::Approx(1080.0));

    // Simulate high-DPI scaling: window logical position is at (500, 300) logical points, scale is 2.0
    GodotDisplayVector2 window_pos_logical(500.0, 300.0);
    double scale = 2.0;
    GodotDisplayVector2 window_pos_physical(window_pos_logical.x * scale, window_pos_logical.y * scale);

    // Viewport-local physical position
    GodotDisplayVector2 local_pos_physical(pixel.x - window_pos_physical.x, pixel.y - window_pos_physical.y);
    CHECK(local_pos_physical.x == doctest::Approx(920.0));
    CHECK(local_pos_physical.y == doctest::Approx(480.0));

    // Viewport-local logical position
    GodotDisplayVector2 local_pos_logical(local_pos_physical.x / scale, local_pos_physical.y / scale);
    CHECK(local_pos_logical.x == doctest::Approx(460.0));
    CHECK(local_pos_logical.y == doctest::Approx(240.0));
}

TEST_CASE("Testing HiDPI Scaling Settings and Coordinate Transforms")
{
    // Screen parameters
    GodotDisplayVector2 screen_size_lpix(1920, 1080);
    double os_screen_scale = 2.0;                                                                             // Retina screen
    GodotDisplayVector2 screen_size_ppix(screen_size_lpix.x * os_screen_scale, screen_size_lpix.y * os_screen_scale); // (3840, 2160)

    // Projected pixel from ProjectionEngine (physical pixels)
    GodotDisplayVector2 projected_pixel_physical(1920.0, 1080.0);

    // Window position in logical points
    GodotDisplayVector2 window_pos_logical(100.0, 50.0);
    GodotDisplayVector2 window_pos_physical(window_pos_logical.x * os_screen_scale, window_pos_logical.y * os_screen_scale); // (200, 100)

    // Calculate local position in physical pixels
    GodotDisplayVector2 local_pos_physical(projected_pixel_physical.x - window_pos_physical.x, projected_pixel_physical.y - window_pos_physical.y);

    // Test Scenario A: allow_hidpi = true (Godot window scale is 2.0)
    {
        double godot_window_scale = 2.0;
        double window_to_screen_scale_ratio = godot_window_scale / os_screen_scale;

        // Scale local position to Godot window space
        GodotDisplayVector2 local_pos_godot(local_pos_physical.x * window_to_screen_scale_ratio, local_pos_physical.y * window_to_screen_scale_ratio);
        CHECK(window_to_screen_scale_ratio == doctest::Approx(1.0));
        CHECK(local_pos_godot.x == doctest::Approx(local_pos_physical.x));
    }

    // Test Scenario B: allow_hidpi = false (Godot window scale is 1.0)
    {
        double godot_window_scale = 1.0;
        double window_to_screen_scale_ratio = godot_window_scale / os_screen_scale;

        // Scale local position to Godot window space
        GodotDisplayVector2 local_pos_godot(local_pos_physical.x * window_to_screen_scale_ratio, local_pos_physical.y * window_to_screen_scale_ratio);
        CHECK(window_to_screen_scale_ratio == doctest::Approx(0.5));
        CHECK(local_pos_godot.x == doctest::Approx(local_pos_physical.x * 0.5));
    }
}

TEST_CASE("Testing Web Geometry Scaling and Coordinate Mapping Parity")
{
    // Web Screen metrics (Simulated Retina: 1512x982 logical, 3024x1964 physical)
    GodotDisplayVector2 screen_size_lpix(1512.0, 982.0);
    double dpr = 2.0;
    GodotDisplayVector2 screen_size_ppix(screen_size_lpix.x * dpr, screen_size_lpix.y * dpr); // (3024, 1964)

    // Scenario A: godot_scale = 1.0 (Default Web export, HiDPI disabled)
    {
        double godot_scale = 1.0;
        double window_to_screen_scale_ratio = godot_scale / dpr; // 0.5

        // Gaze intersection point in physical pixels on the screen
        GodotDisplayVector2 projected_pixel_physical(1512.0, 982.0);

        // Canvas position on the screen in physical pixels
        GodotDisplayVector2 canvas_pos_physical(300.0 * dpr, 200.0 * dpr); // (600, 400)

        // Calculate local position in physical pixels
        GodotDisplayVector2 local_pos_physical(
            projected_pixel_physical.x - canvas_pos_physical.x,
            projected_pixel_physical.y - canvas_pos_physical.y); // (912, 582)

        // Scale to Godot viewport space
        GodotDisplayVector2 local_pos_godot(
            local_pos_physical.x * window_to_screen_scale_ratio,
            local_pos_physical.y * window_to_screen_scale_ratio);

        CHECK(window_to_screen_scale_ratio == doctest::Approx(0.5));
        CHECK(local_pos_godot.x == doctest::Approx(456.0));
        CHECK(local_pos_godot.y == doctest::Approx(291.0));
    }

    // Scenario B: godot_scale = 2.0 (HiDPI enabled)
    {
        double godot_scale = 2.0;
        double window_to_screen_scale_ratio = godot_scale / dpr; // 1.0

        GodotDisplayVector2 projected_pixel_physical(1512.0, 982.0);
        GodotDisplayVector2 canvas_pos_physical(300.0 * dpr, 200.0 * dpr);

        GodotDisplayVector2 local_pos_physical(
            projected_pixel_physical.x - canvas_pos_physical.x,
            projected_pixel_physical.y - canvas_pos_physical.y);

        GodotDisplayVector2 local_pos_godot(
            local_pos_physical.x * window_to_screen_scale_ratio,
            local_pos_physical.y * window_to_screen_scale_ratio);

        CHECK(window_to_screen_scale_ratio == doctest::Approx(1.0));
        CHECK(local_pos_godot.x == doctest::Approx(912.0));
        CHECK(local_pos_godot.y == doctest::Approx(582.0));
    }
}

TEST_CASE("Testing ScreenProjector Decoupled Coordinate Mapping")
{
    // 1. Setup projection engine mock metrics (MacBook Pro 15" logical sizing)
    Gaze::ProjectionEngine engine;
    engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(1440.0, 900.0)); // Logical screen width
    engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(300.0, 195.0));      // Physical screen mm
    Gaze::CameraPlacement placement(Gaze::GodotCameraVector3(0.0, 95.5, 0.0), 0.0);
    engine.set_camera_placement(placement);

    // 2. Test standard window (positioned at 0,0)
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::derive_configuration(
            Gaze::GodotDisplayVector2(0.0, 0.0), // Window pos in logical screen pixels
            Gaze::SpacedVector2<Gaze::Space::GodotViewportPx>(1.0, 1.0), // Viewport scale
            Gaze::SpacedVector2<Gaze::Space::GodotViewportPx>(0.0, 0.0)  // Viewport origin offset
        );

        Gaze::GodotCameraVector3 origin_cam(0.0, -95.5, -800.0);
        Gaze::GodotCameraVector3 dir_cam(0.0, 0.0, 1.0);

        Gaze::SpacedVector2<Gaze::Space::GodotViewportPx> viewport_pixel;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, viewport_pixel);

        REQUIRE(ok);
        // Center of 1440.0 screen is 720.0. With win_pos=0 and vp_scale=1, viewport X should be 720.0
        CHECK(viewport_pixel.x == doctest::Approx(720.0));
    }

    // 3. Test window positioned at (180, 82) logical pixels
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::derive_configuration(
            Gaze::GodotDisplayVector2(180.0, 82.0), // Window pos in logical screen pixels
            Gaze::SpacedVector2<Gaze::Space::GodotViewportPx>(1.0, 1.0),    // Viewport scale
            Gaze::SpacedVector2<Gaze::Space::GodotViewportPx>(0.0, 0.0)     // Viewport origin offset
        );

        Gaze::GodotCameraVector3 origin_cam(0.0, -95.5, -800.0);
        Gaze::GodotCameraVector3 dir_cam(0.0, 0.0, 1.0);

        Gaze::SpacedVector2<Gaze::Space::GodotViewportPx> viewport_pixel;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, viewport_pixel);

        REQUIRE(ok);
        // logical screen X = 720.0
        // local window X = 720.0 - 180.0 = 540.0
        CHECK(viewport_pixel.x == doctest::Approx(540.0));
    }

    // 4. Test window positioned at (180, 82) logical pixels using from_godot_geometry
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::from_godot_geometry(
            Gaze::GodotDisplayVector2(180.0, 82.0), // Window pos in logical pixels
            Gaze::SpacedVector2<Gaze::Space::GodotViewportPx>(1.0, 1.0),    // Viewport scale
            Gaze::SpacedVector2<Gaze::Space::GodotViewportPx>(0.0, 0.0)     // Viewport origin offset
        );

        Gaze::GodotCameraVector3 origin_cam(0.0, -95.5, -800.0);
        Gaze::GodotCameraVector3 dir_cam(0.0, 0.0, 1.0);

        Gaze::SpacedVector2<Gaze::Space::GodotViewportPx> viewport_pixel;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, viewport_pixel);

        REQUIRE(ok);
        CHECK(viewport_pixel.x == doctest::Approx(540.0));
    }
}

TEST_CASE("Testing ScreenProjector Scaling & Inverse Parity")
{
    using ViewportPx = Gaze::SpacedVector2<Gaze::Space::GodotViewportPx>;

    // Setup projection engine mock metrics
    Gaze::ProjectionEngine engine;
    engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(1440.0, 900.0)); // Logical screen width
    engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(302.0, 188.0));      // Physical screen mm
    Gaze::CameraPlacement placement(Gaze::GodotCameraVector3(0.0, 94.0, 0.0), 0.0);
    engine.set_camera_placement(placement);

    // 1. Standard Configuration
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::from_godot_geometry(
            Gaze::GodotDisplayVector2(100.0, 50.0), // Window pos in logical screen pixels
            ViewportPx(1.0, 1.0),                  // Logical viewport scale
            ViewportPx(0.0, 0.0)                   // Logical viewport offset
        );

        ViewportPx logical_pixel(512.0, 300.0);
        Gaze::GodotDisplayVector2 physical_pixel = projector.map_viewport_to_screen_px(logical_pixel);

        CHECK(physical_pixel.x == doctest::Approx(612.0));
        CHECK(physical_pixel.y == doctest::Approx(350.0));

        // Setup raw gaze/origin in camera space and project it back to verify inverse parity
        Gaze::GodotCameraVector3 origin_cam(0.0, -94.0, -800.0);
        Gaze::GodotCameraVector3 dir_cam(0.05, -0.02, 0.998); // Random gaze direction pointing forward
        ViewportPx proj_viewport;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, proj_viewport);
        REQUIRE(ok);

        Gaze::GodotDisplayVector2 proj_physical = projector.map_viewport_to_screen_px(proj_viewport);

        double local_x = proj_physical.x - projector.window_position_px.x;
        double local_y = proj_physical.y - projector.window_position_px.y;
        double view_x = (local_x - projector.viewport_offset_px.x) / projector.viewport_scale.x;
        double view_y = (local_y - projector.viewport_offset_px.y) / projector.viewport_scale.y;

        CHECK(view_x == doctest::Approx(proj_viewport.x));
        CHECK(view_y == doctest::Approx(proj_viewport.y));
    }

    // 2. Scaled viewport configuration (e.g. game viewport stretched by 2.0)
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::from_godot_geometry(
            Gaze::GodotDisplayVector2(200.0, 100.0), // window position in logical pixels
            ViewportPx(2.0, 2.0),                   // viewport scale
            ViewportPx(0.0, 0.0)                    // offset
        );

        ViewportPx logical_pixel(512.0, 300.0);
        Gaze::GodotDisplayVector2 physical_pixel = projector.map_viewport_to_screen_px(logical_pixel);
        CHECK(physical_pixel.x == doctest::Approx(1224.0));
        CHECK(physical_pixel.y == doctest::Approx(700.0));

        // Assert inverse parity
        Gaze::GodotCameraVector3 origin_cam(0.0, -94.0, -800.0);
        Gaze::GodotCameraVector3 dir_cam(-0.1, 0.05, 0.99);
        ViewportPx proj_viewport;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, proj_viewport);
        REQUIRE(ok);

        Gaze::GodotDisplayVector2 proj_physical = projector.map_viewport_to_screen_px(proj_viewport);
        double local_x = proj_physical.x - projector.window_position_px.x;
        double local_y = proj_physical.y - projector.window_position_px.y;
        double view_x = (local_x - projector.viewport_offset_px.x) / projector.viewport_scale.x;
        double view_y = (local_y - projector.viewport_offset_px.y) / projector.viewport_scale.y;

        CHECK(view_x == doctest::Approx(proj_viewport.x));
        CHECK(view_y == doctest::Approx(proj_viewport.y));
    }

    // 3. Check Safeguards & Division-by-Zero Protection
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::from_godot_geometry(
            Gaze::GodotDisplayVector2(0.0, 0.0),
            ViewportPx(0.0, 0.0), // Scale = 0 (Division by zero risk!)
            ViewportPx(0.0, 0.0));

        Gaze::GodotCameraVector3 origin_cam(0.0, -94.0, -800.0);
        Gaze::GodotCameraVector3 dir_cam(0.0, 0.0, 1.0);
        ViewportPx proj_viewport;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, proj_viewport);
        CHECK_FALSE(ok); // Must fail gracefully
    }
}

TEST_CASE("Testing Head Rotation Pitch and Yaw Coordinate Signs")
{
    // Test case A: Logical math verification
    // 1. Staring straight ahead (near-zero rotation)
    {
        Gaze::OpenCVCameraVector3 translation(0.0, 0.0, 800.0);
        Gaze::OpenCVCameraVector3 rotation_straight(0.0, 0.0, 0.0);
        Gaze::GodotFaceTransform3D transform = Gaze::CoordinateConversions::opencv_pose_to_godot_camera_transform(translation, rotation_straight);
        Gaze::GodotCameraVector3 head_forward = -transform.basis.z.normalized();

        // Z points towards the screen (positive Z in Godot camera space)
        CHECK(head_forward.z > 0.9);
        CHECK(std::abs(head_forward.x) < 0.01);
        CHECK(std::abs(head_forward.y) < 0.01);
    }

    // 2. Head tilted up (pitch rotation around X is negative in OpenCV: rvec.x < 0)
    {
        Gaze::OpenCVCameraVector3 translation(0.0, 0.0, 800.0);
        Gaze::OpenCVCameraVector3 rotation_up(-0.15, 0.0, 0.0); // ~8.6 degrees up
        Gaze::GodotFaceTransform3D transform = Gaze::CoordinateConversions::opencv_pose_to_godot_camera_transform(translation, rotation_up);
        Gaze::GodotCameraVector3 head_forward = -transform.basis.z.normalized();

        // Pitch up must produce a positive Y component in camera space
        CHECK(head_forward.y > 0.05);
        CHECK(head_forward.z > 0.9);
    }

    // 3. Head tilted down (pitch rotation around X is positive in OpenCV: rvec.x > 0)
    {
        Gaze::OpenCVCameraVector3 translation(0.0, 0.0, 800.0);
        Gaze::OpenCVCameraVector3 rotation_down(0.15, 0.0, 0.0); // ~8.6 degrees down
        Gaze::GodotFaceTransform3D transform = Gaze::CoordinateConversions::opencv_pose_to_godot_camera_transform(translation, rotation_down);
        Gaze::GodotCameraVector3 head_forward = -transform.basis.z.normalized();

        // Pitch down must produce a negative Y component in camera space
        CHECK(head_forward.y < -0.05);
        CHECK(head_forward.z > 0.9);
    }

    // 4. Head turned left (yaw rotation around Y is negative in OpenCV: rvec.y < 0) -> Viewer Right (+X)
    {
        Gaze::OpenCVCameraVector3 translation(0.0, 0.0, 800.0);
        Gaze::OpenCVCameraVector3 rotation_left(0.0, -0.15, 0.0); // ~8.6 degrees left
        Gaze::GodotFaceTransform3D transform = Gaze::CoordinateConversions::opencv_pose_to_godot_camera_transform(translation, rotation_left);
        Gaze::GodotCameraVector3 head_forward = -transform.basis.z.normalized();

        // Turning left (facing camera's positive X direction / viewer right)
        CHECK(head_forward.x > 0.05);
        CHECK(head_forward.z > 0.9);
    }

    // 5. Head turned right (yaw rotation around Y is positive in OpenCV: rvec.y > 0) -> Viewer Left (-X)
    {
        Gaze::OpenCVCameraVector3 translation(0.0, 0.0, 800.0);
        Gaze::OpenCVCameraVector3 rotation_right(0.0, 0.15, 0.0); // ~8.6 degrees right
        Gaze::GodotFaceTransform3D transform = Gaze::CoordinateConversions::opencv_pose_to_godot_camera_transform(translation, rotation_right);
        Gaze::GodotCameraVector3 head_forward = -transform.basis.z.normalized();

        // Turning right (facing camera's negative X direction / viewer left)
        CHECK(head_forward.x < -0.05);
        CHECK(head_forward.z > 0.9);
    }

    // 6. Test PnP solver recovery of pitch down from 2D projected landmarks
    {
        double fx = 1000.0, cx = 320.0, cy = 240.0;
        std::vector<Gaze::OpenCVFaceVector3> model_points = Gaze::FaceModelGeometry::get_5pt_model_points();

        // True pose: pitched down by ~15 degrees (rvec.x = +0.2618)
        Gaze::OpenCVCameraVector3 true_rvec(0.2618, 0.0, 0.0);
        Gaze::OpenCVCameraVector3 true_tvec(0.0, 50.0, 650.0);

        Gaze::SpacedBasis<Gaze::Space::OpenCVFaceModel, Gaze::Space::OpenCVCamera> R_true = Gaze::rodrigues_to_basis<Gaze::Space::OpenCVFaceModel, Gaze::Space::OpenCVCamera>(true_rvec);
        std::vector<Gaze::SpacedVector2<Gaze::Space::GodotCameraWorkingImagePixels>> img_pts(5);
        for (size_t i = 0; i < 5; ++i) {
            Gaze::OpenCVCameraVector3 P_cam = R_true.transform(model_points[i]) + true_tvec;
            img_pts[i] = Gaze::SpacedVector2<Gaze::Space::GodotCameraWorkingImagePixels>(fx * (P_cam.x / P_cam.z) + cx, fx * (P_cam.y / P_cam.z) + cy);
        }

        // Run PnP solver with un-warmed default guess (rvec = 0, tvec = 700)
        Gaze::OpenCVCameraVector3 est_rvec(0.0, 0.0, 0.0);
        Gaze::OpenCVCameraVector3 est_tvec(0.0, 0.0, 700.0);
        bool pnp_ok = Gaze::SQPnPSolver::solve_rvec(model_points, img_pts, fx, fx, cx, cy, est_rvec, est_tvec);
        REQUIRE(pnp_ok == true);

        // Check recovered pitch angle (rvec.x)
        std::cout << "[PnP Test] True pitch: " << true_rvec.x << " | Est pitch: " << est_rvec.x << " | Est ty: " << est_tvec.y << " | Est tz: " << est_tvec.z << std::endl;
        CHECK(est_rvec.x > 0.15); // Must recover significant downward pitch

        // Project ray onto screen plane (600x340 mm screen, camera at top y=170mm)
        Gaze::GodotFaceTransform3D xform = Gaze::CoordinateConversions::opencv_pose_to_godot_camera_transform(est_tvec, est_rvec);
        Gaze::GodotCameraVector3 head_fwd(-xform.basis.z.normalized());
        Gaze::GodotCameraVector3 origin(xform.origin);
        
        Gaze::ProjectionEngine proj_engine;
        proj_engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(1920, 1080));
        proj_engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(600, 340));
        proj_engine.set_camera_placement(Gaze::CameraPlacement(Gaze::GodotCameraVector3(0, 170, 0), 0.0));
        
        Gaze::GodotDisplayVector2 projected_pixel;
        bool proj_ok = proj_engine.project_gaze(origin, head_fwd, projected_pixel);
        std::cout << "[PnP Test] Head forward: (" << head_fwd.x << ", " << head_fwd.y << ", " << head_fwd.z << ") | Projected pixel: (" << projected_pixel.x << ", " << projected_pixel.y << ")" << std::endl;
        CHECK(proj_ok == true);
        CHECK(projected_pixel.y > 540.0); // Pitch down must land in bottom half of screen (>540px)
    }

    // 7. Test PnP solver sensitivity under landmark Y-noise (Cold-start vs Warm-start)
    {
        double fx = 1000.0, cx = 320.0, cy = 240.0;
        std::vector<Gaze::OpenCVFaceVector3> model_points = Gaze::FaceModelGeometry::get_5pt_model_points();

        // Pitched down pose with 1.5px landmark Y-shift / noise (typical YuNet variance)
        Gaze::OpenCVCameraVector3 true_rvec(0.20, 0.0, 0.0); // ~11.5 deg pitch down
        Gaze::OpenCVCameraVector3 true_tvec(0.0, 30.0, 650.0);

        Gaze::SpacedBasis<Gaze::Space::OpenCVFaceModel, Gaze::Space::OpenCVCamera> R_true = Gaze::rodrigues_to_basis<Gaze::Space::OpenCVFaceModel, Gaze::Space::OpenCVCamera>(true_rvec);
        std::vector<Gaze::SpacedVector2<Gaze::Space::GodotCameraWorkingImagePixels>> img_pts(5);
        for (size_t i = 0; i < 5; ++i) {
            Gaze::OpenCVCameraVector3 P_cam = R_true.transform(model_points[i]) + true_tvec;
            img_pts[i] = Gaze::SpacedVector2<Gaze::Space::GodotCameraWorkingImagePixels>(fx * (P_cam.x / P_cam.z) + cx, fx * (P_cam.y / P_cam.z) + cy);
        }
        // Add 1.5px Y distortion to nose & mouth (simulating foreshortening detection noise)
        img_pts[2].y -= 1.5; // Nose detected slightly higher
        img_pts[3].y += 1.5; // Mouth detected slightly lower

        // Cold start (rvec=0, tvec=700)
        Gaze::OpenCVCameraVector3 cold_rvec(0.0, 0.0, 0.0);
        Gaze::OpenCVCameraVector3 cold_tvec(0.0, 0.0, 700.0);
        Gaze::SQPnPSolver::solve_rvec(model_points, img_pts, fx, fx, cx, cy, cold_rvec, cold_tvec);

        // Warm start (starting near previous frame pose)
        Gaze::OpenCVCameraVector3 warm_rvec(0.18, 0.0, 0.0);
        Gaze::OpenCVCameraVector3 warm_tvec(0.0, 28.0, 648.0);
        Gaze::SQPnPSolver::solve_rvec(model_points, img_pts, fx, fx, cx, cy, warm_rvec, warm_tvec);

        std::cout << "[PnP Noise Test] Cold pitch: " << cold_rvec.x << " | Warm pitch: " << warm_rvec.x << std::endl;
        CHECK(warm_rvec.x > 0.10);
    }

    // 8. Test ORTYuNetDetector initialization and verification
    {
        std::string detector_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        Gaze::ORTYuNetDetector detector(detector_path);
        REQUIRE(detector.initialize() == true);
    }
}

TEST_CASE("Testing Edge Conditions and Stress Scenarios")
{
    // 1. Test empty frames (null data)
    {
        std::string detector_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        ORTYuNetDetector detector(detector_path);
        REQUIRE(detector.initialize() == true);

        Frame empty_frame;
        empty_frame.width = 640;
        empty_frame.height = 480;
        empty_frame.timestamp = 0.0;
        empty_frame.data = nullptr;

        YuNetResult res;
        CHECK(detector.process_frame(empty_frame, res, 0.0f) == false);
    }

    // 2. Test invalid dimensions (0x0 frame)
    {
        std::string detector_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        ORTYuNetDetector detector(detector_path);
        REQUIRE(detector.initialize() == true);

        unsigned char dummy_data[1] = {0};
        Frame zero_frame;
        zero_frame.width = 0;
        zero_frame.height = 0;
        zero_frame.timestamp = 0.0;
        zero_frame.data = dummy_data;

        YuNetResult res;
        CHECK(detector.process_frame(zero_frame, res, 0.0f) == false);
    }

    // 3. Test negative dimensions (-640x-480 frame)
    {
        std::string detector_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        ORTYuNetDetector detector(detector_path);
        REQUIRE(detector.initialize() == true);

        unsigned char dummy_data[1] = {0};
        Frame neg_frame;
        neg_frame.width = -640;
        neg_frame.height = -480;
        neg_frame.timestamp = 0.0;
        neg_frame.data = dummy_data;

        YuNetResult res;
        CHECK(detector.process_frame(neg_frame, res, 0.0f) == false);
    }

    // 4. Test extremely small non-zero dimensions (1x1 frame)
    {
        std::string detector_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        ORTYuNetDetector detector(detector_path);
        REQUIRE(detector.initialize() == true);

        unsigned char dummy_data[3] = {128, 128, 128};
        Frame tiny_frame;
        tiny_frame.width = 1;
        tiny_frame.height = 1;
        tiny_frame.timestamp = 0.0;
        tiny_frame.data = dummy_data;

        YuNetResult res;
        bool exec_ok = detector.process_frame(tiny_frame, res, 0.0f);
        CHECK(exec_ok == false);
        CHECK(res.face_detected == false);
    }

    // 5. Test ORTGazeModel with extreme head pose rotations (NaN, Infinity, and Out of Bounds)
    {
        std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";
        ORTGazeModel model(gaze_path);
        REQUIRE(model.initialize() == true);

        EyeCrops crops;
        crops.face_detected = true;
        crops.head_pose_translation = OpenCVCameraVector3(0.0, 0.0, 500.0);
        crops.left_eye_center_cam = GodotCameraVector3(31.5, 0.0, 480.0);
        crops.right_eye_center_cam = GodotCameraVector3(-31.5, 0.0, 480.0);
        std::memset(crops.left_eye_data, 128, 10800);
        std::memset(crops.right_eye_data, 128, 10800);

        // Test with NaN rotation
        crops.head_pose_rotation = OpenCVCameraVector3(NAN, NAN, NAN);
        OpenVINOGazeVector3 gaze_dir_cv;
        bool success = model.estimate_raw_gaze(crops, gaze_dir_cv);
        CHECK((!success || std::isnan(gaze_dir_cv.x) || std::isnan(gaze_dir_cv.y) || std::isnan(gaze_dir_cv.z)));

        // Test with Infinity rotation
        crops.head_pose_rotation = OpenCVCameraVector3(INFINITY, -INFINITY, INFINITY);
        success = model.estimate_raw_gaze(crops, gaze_dir_cv);
        CHECK((!success || std::isnan(gaze_dir_cv.x) || std::isnan(gaze_dir_cv.y) || std::isnan(gaze_dir_cv.z)));

        // Test with extremely large rotation values (e.g. 1e12)
        crops.head_pose_rotation = OpenCVCameraVector3(1e12, -1e12, 1e12);
        success = model.estimate_raw_gaze(crops, gaze_dir_cv);
        CHECK((success || !success));
    }

    // 6. Multithreaded session stress test (run multiple threads concurrently executing model inference)
    {
        std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";
        ORTGazeModel model(gaze_path);
        REQUIRE(model.initialize() == true);

        auto thread_fn = [&model]()
        {
            EyeCrops crops;
            crops.face_detected = true;
            crops.head_pose_rotation = OpenCVCameraVector3(0.05, -0.02, 0.01);
            crops.head_pose_translation = OpenCVCameraVector3(10.0, -15.0, 600.0);
            crops.left_eye_center_cam = GodotCameraVector3(31.5, 0.0, 580.0);
            crops.right_eye_center_cam = GodotCameraVector3(-31.5, 0.0, 580.0);
            std::memset(crops.left_eye_data, 200, 10800);
            std::memset(crops.right_eye_data, 100, 10800);

            OpenVINOGazeVector3 gaze_dir;
            for (int i = 0; i < 50; ++i)
            {
                bool ok = model.estimate_raw_gaze(crops, gaze_dir);
                CHECK(ok == true);
                CHECK(gaze_dir.length() == doctest::Approx(1.0));
            }
        };

        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t)
        {
            threads.emplace_back(thread_fn);
        }
        for (auto &th : threads)
        {
            th.join();
        }
    }

    // 7. Test uninitialized pipeline and model behavior
    {
        std::string detector_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        ORTYuNetDetector detector(detector_path);
        // Do NOT call initialize()
        Frame frame;
        frame.width = 640;
        frame.height = 480;
        frame.timestamp = 0.0;
        unsigned char dummy_data[640 * 480 * 3] = {0};
        frame.data = dummy_data;
        YuNetResult res;
        CHECK(detector.process_frame(frame, res, 0.0f) == false);

        std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";
        ORTGazeModel model(gaze_path);
        // Do NOT call initialize()
        EyeCrops crops;
        OpenVINOGazeVector3 gaze_dir;
        CHECK(model.estimate_raw_gaze(crops, gaze_dir) == false);
    }

    // 8. Test extremely large frame sizes (8K: 7680x4320)
    {
        std::string detector_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        ORTYuNetDetector detector(detector_path);
        REQUIRE(detector.initialize() == true);

        std::unique_ptr<unsigned char[]> huge_mat(new unsigned char[7680 * 4320 * 3]());
        Frame huge_frame;
        huge_frame.width = 7680;
        huge_frame.height = 4320;
        huge_frame.timestamp = 0.0;
        huge_frame.data = huge_mat.get();

        YuNetResult res;
        bool exec_ok = detector.process_frame(huge_frame, res, 0.0f);
        CHECK(exec_ok == false);
        CHECK(res.face_detected == false);
    }
}

#include "pnp_solver.hpp"
#include "cpu_image_warper.hpp"

TEST_CASE("Testing Native Math Solvers and Warping Parity")
{
    // 1. Test Rodrigues Parity (Direct Math Roundtrip / Sanity Check)
    {
        std::vector<OpenCVCameraVector3> test_rvecs = {
            OpenCVCameraVector3(0.1, -0.2, 0.5),
            OpenCVCameraVector3(0.0, 0.0, 0.0),
            OpenCVCameraVector3(1e-7, -2e-7, 1e-7), // Near zero
            OpenCVCameraVector3(1.8, -1.8, 1.8)     // Large angle
        };

        for (const auto &rvec_orig : test_rvecs)
        {
            SpacedBasis<Space::OpenCVFaceModel, Space::OpenCVCamera> R_basis = rodrigues_to_basis<Space::OpenCVFaceModel, Space::OpenCVCamera>(rvec_orig);
            OpenCVCameraVector3 rvec_native = basis_to_rodrigues<Space::OpenCVFaceModel, Space::OpenCVCamera>(R_basis);

            // Assert native back-conversion matches original
            CHECK(rvec_native.x == doctest::Approx(rvec_orig.x).epsilon(5e-3));
            CHECK(rvec_native.y == doctest::Approx(rvec_orig.y).epsilon(5e-3));
            CHECK(rvec_native.z == doctest::Approx(rvec_orig.z).epsilon(5e-3));

            // Check rotation matrix properties (unit length columns, orthogonal columns)
            double len_x = R_basis.x.length();
            double len_y = R_basis.y.length();
            double len_z = R_basis.z.length();
            CHECK(len_x == doctest::Approx(1.0).epsilon(5e-3));
            CHECK(len_y == doctest::Approx(1.0).epsilon(5e-3));
            CHECK(len_z == doctest::Approx(1.0).epsilon(5e-3));

            double dot_xy = R_basis.x.dot(R_basis.y);
            double dot_xz = R_basis.x.dot(R_basis.z);
            double dot_yz = R_basis.y.dot(R_basis.z);
            CHECK(std::abs(dot_xy) <= 1e-4);
            CHECK(std::abs(dot_xz) <= 1e-4);
            CHECK(std::abs(dot_yz) <= 1e-4);
        }
    }

    // 2. Test LM PnP Solver Convergence directly to True Pose
    {
        std::vector<OpenCVFaceVector3> model_pts = {
            OpenCVFaceVector3(-FaceModelGeometry::EYE_X, FaceModelGeometry::EYE_Y, FaceModelGeometry::EYE_Z),
            OpenCVFaceVector3(FaceModelGeometry::EYE_X, FaceModelGeometry::EYE_Y, FaceModelGeometry::EYE_Z),
            OpenCVFaceVector3(0.0, -0.5, -52.0),
            OpenCVFaceVector3(-FaceModelGeometry::MOUTH_X, FaceModelGeometry::MOUTH_Y, FaceModelGeometry::MOUTH_Z),
            OpenCVFaceVector3(FaceModelGeometry::MOUTH_X, FaceModelGeometry::MOUTH_Y, FaceModelGeometry::MOUTH_Z)};

        OpenCVCameraVector3 true_rvec(0.12, -0.08, 0.04);
        OpenCVCameraVector3 true_tvec(-15.0, 10.0, 580.0);
        double fx = 960.0, fy = 960.0, cx = 320.0, cy = 240.0;

        // Generate 2D image points from true pose
        std::vector<SpacedVector2<Space::GodotCameraWorkingImagePixels>> img_pts(5);
        for (int i = 0; i < 5; ++i)
        {
            SpacedBasis<Space::OpenCVFaceModel, Space::OpenCVCamera> R = rodrigues_to_basis<Space::OpenCVFaceModel, Space::OpenCVCamera>(true_rvec);
            OpenCVCameraVector3 P_cam = R.transform(model_pts[i]) + true_tvec;
            img_pts[i] = SpacedVector2<Space::GodotCameraWorkingImagePixels>(fx * P_cam.x / P_cam.z + cx, fy * P_cam.y / P_cam.z + cy);
        }

        // Run native SQPnP
        OpenCVCameraVector3 est_rvec(0.0, 0.0, 0.0);
        OpenCVCameraVector3 est_tvec(0.0, 0.0, 700.0);
        bool pnp_ok = SQPnPSolver::solve_rvec(model_pts, img_pts, fx, fy, cx, cy, est_rvec, est_tvec);
        REQUIRE(pnp_ok);

        // Assert native solver converges directly to the true pose used to project the points
        CHECK(est_rvec.x == doctest::Approx(true_rvec.x).epsilon(5e-3));
        CHECK(est_rvec.y == doctest::Approx(true_rvec.y).epsilon(5e-3));
        CHECK(est_rvec.z == doctest::Approx(true_rvec.z).epsilon(5e-3));
        CHECK(est_tvec.x == doctest::Approx(true_tvec.x).epsilon(5e-3));
        CHECK(est_tvec.y == doctest::Approx(true_tvec.y).epsilon(5e-3));
        CHECK(est_tvec.z == doctest::Approx(true_tvec.z).epsilon(5e-3));
    }

    // 3. Test Bilinear Image Warping Fallback (Verifying successful execution)
    {
        const int src_w = 200;
        const int src_h = 200;
        std::vector<unsigned char> dummy_src(src_h * src_w * 3);
        for (int y = 0; y < src_h; ++y)
        {
            for (int x = 0; x < src_w; ++x)
            {
                int idx = (y * src_w + x) * 3;
                dummy_src[idx + 0] = static_cast<unsigned char>((x * 7 + y * 13) % 256); // B
                dummy_src[idx + 1] = static_cast<unsigned char>((x * 17 + y * 3) % 256); // G
                dummy_src[idx + 2] = static_cast<unsigned char>((x * 3 + y * 19) % 256); // R
            }
        }

        GodotCameraImageVector2 landmarks[5] = {
            GodotCameraImageVector2(70.5, 80.2),  // right eye
            GodotCameraImageVector2(130.8, 85.6), // left eye
            GodotCameraImageVector2(100.0, 110.0),
            GodotCameraImageVector2(80.0, 140.0),
            GodotCameraImageVector2(120.0, 142.0)};

        GodotCameraImageVector2 eye_center = landmarks[0];
        double roll_dx = landmarks[1].x - landmarks[0].x;
        double roll_dy = landmarks[1].y - landmarks[0].y;
        double angle = std::atan2(roll_dy, roll_dx) * (180.0 / 3.141592653589793);
        double dist_px = std::sqrt(roll_dx * roll_dx + roll_dy * roll_dy);
        double scale = 70.0 / (dist_px > 1e-6 ? dist_px : 70.0);

        CPUImageWarper native_warper;
        unsigned char native_out[10800] = {0};
        bool warp_success = native_warper.warp(dummy_src.data(), src_w, src_h, 3, eye_center, angle, scale, native_out);
        CHECK(warp_success);

        // Check that the output is not entirely blank/zeroed out
        bool all_zero = true;
        for (int i = 0; i < 10800; ++i)
        {
            if (native_out[i] != 0)
            {
                all_zero = false;
                break;
            }
        }
        CHECK(!all_zero);
    }
}

#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>

TEST_CASE("Testing Native PnP Solver Stress Test and Benchmark")
{
    std::cout << "\n=== Running PnP Solver Stress Test and Benchmark ===" << std::endl;

    // 3D model points (standard canonical face geometry)
    std::vector<OpenCVFaceVector3> model_pts = {
        OpenCVFaceVector3(-FaceModelGeometry::EYE_X, FaceModelGeometry::EYE_Y, FaceModelGeometry::EYE_Z),
        OpenCVFaceVector3(FaceModelGeometry::EYE_X, FaceModelGeometry::EYE_Y, FaceModelGeometry::EYE_Z),
        OpenCVFaceVector3(0.0, -0.5, -52.0),
        OpenCVFaceVector3(-FaceModelGeometry::MOUTH_X, FaceModelGeometry::MOUTH_Y, FaceModelGeometry::MOUTH_Z),
        OpenCVFaceVector3(FaceModelGeometry::MOUTH_X, FaceModelGeometry::MOUTH_Y, FaceModelGeometry::MOUTH_Z)};

    // Camera parameters
    double fx = 960.0, fy = 960.0, cx = 320.0, cy = 240.0;

    // Setup random generator
    std::mt19937 rng(42);                                       // fixed seed for reproducibility
    std::uniform_real_distribution<double> dist_rot(-0.5, 0.5); // rotations in radians (~28 deg)
    std::uniform_real_distribution<double> dist_trans_xy(-50.0, 50.0);
    std::uniform_real_distribution<double> dist_trans_z(400.0, 1000.0);
    std::uniform_real_distribution<double> dist_noise(-1.0, 1.0); // pixel noise

    struct Scenario
    {
        std::string name;
        bool extreme_rot;
        bool extreme_dist;
        double noise_level;
        bool coplanar;
        bool use_extrinsic_guess;
    };

    std::vector<Scenario> scenarios = {
        {"1. Well-conditioned (Standard pose, no noise, standard guess)", false, false, 0.0, false, true},
        {"2. Ill-conditioned (Extreme rotations, no noise)", true, false, 0.0, false, true},
        {"3. Ill-conditioned (Extreme distances, no noise)", false, true, 0.0, false, true},
        {"4. Ill-conditioned (Standard pose, moderate noise)", false, false, 1.5, false, true},
        {"5. Ill-conditioned (Almost coplanar model points)", false, false, 0.0, true, true},
        {"6. Ill-conditioned (No extrinsic guess - from zero/700 default)", false, false, 0.0, false, false}};

    const int runs_per_scenario = 100; // 100 runs per scenario

    // For performance benchmark
    double total_time_native_ms = 0.0;
    int total_runs = 0;

    for (const auto &sc : scenarios)
    {
        std::cout << "\nRunning scenario: " << sc.name << std::endl;
        int success_count = 0;
        double max_err_rvec = 0.0;
        double max_err_tvec = 0.0;

        for (int run = 0; run < runs_per_scenario; ++run)
        {
            // Generate true pose
            double rx = dist_rot(rng);
            double ry = dist_rot(rng);
            double rz = dist_rot(rng);
            if (sc.extreme_rot)
            {
                // Extreme rotations: 1.0 to 1.3 radians (~57 to 75 deg)
                std::uniform_real_distribution<double> dist_extreme_rot(1.0, 1.3);
                rx = dist_extreme_rot(rng) * (rng() % 2 ? 1.0 : -1.0);
                ry = dist_extreme_rot(rng) * (rng() % 2 ? 1.0 : -1.0);
                rz = dist_extreme_rot(rng) * (rng() % 2 ? 1.0 : -1.0);
            }
            OpenCVCameraVector3 true_rvec(rx, ry, rz);

            double tx = dist_trans_xy(rng);
            double ty = dist_trans_xy(rng);
            double tz = dist_trans_z(rng);
            if (sc.extreme_dist)
            {
                // Extreme distances: either very close (100mm) or very far (3000mm)
                tz = (run % 2) ? 100.0 : 3000.0;
            }
            OpenCVCameraVector3 true_tvec(tx, ty, tz);

            // Configure model points
            std::vector<OpenCVFaceVector3> current_model_pts = model_pts;
            if (sc.coplanar)
            {
                // Make points almost coplanar by setting nose Z close to 0
                current_model_pts[2].z = -1.0;
            }

            // Project model points to get true 2D image points
            std::vector<SpacedVector2<Space::GodotCameraWorkingImagePixels>> img_pts(5);
            SpacedBasis<Space::OpenCVFaceModel, Space::OpenCVCamera> R = rodrigues_to_basis<Space::OpenCVFaceModel, Space::OpenCVCamera>(true_rvec);
            for (int i = 0; i < 5; ++i)
            {
                OpenCVCameraVector3 P_cam = R.transform(current_model_pts[i]) + true_tvec;
                double px = fx * P_cam.x / P_cam.z + cx;
                double py = fy * P_cam.y / P_cam.z + cy;
                if (sc.noise_level > 0.0)
                {
                    px += dist_noise(rng) * sc.noise_level;
                    py += dist_noise(rng) * sc.noise_level;
                }
                img_pts[i] = SpacedVector2<Space::GodotCameraWorkingImagePixels>(px, py);
            }

            // Prepare initial guesses
            OpenCVCameraVector3 est_rvec(0.0, 0.0, 0.0);
            OpenCVCameraVector3 est_tvec(0.0, 0.0, 700.0);
            if (sc.use_extrinsic_guess)
            {
                // Initial guess perturbed from the truth
                est_rvec = OpenCVCameraVector3(true_rvec.x + dist_rot(rng) * 0.1, true_rvec.y + dist_rot(rng) * 0.1, true_rvec.z + dist_rot(rng) * 0.1);
                est_tvec = OpenCVCameraVector3(true_tvec.x + dist_trans_xy(rng) * 0.1, true_tvec.y + dist_trans_xy(rng) * 0.1, true_tvec.z + dist_trans_z(rng) * 0.1);
            }

            // Run native SQPnP solver and measure time
            OpenCVCameraVector3 native_rvec = est_rvec;
            OpenCVCameraVector3 native_tvec = est_tvec;
            auto t0 = std::chrono::high_resolution_clock::now();
            bool native_ok = SQPnPSolver::solve_rvec(current_model_pts, img_pts, fx, fy, cx, cy, native_rvec, native_tvec);
            auto t1 = std::chrono::high_resolution_clock::now();
            double duration_native = std::chrono::duration<double, std::milli>(t1 - t0).count();

            total_time_native_ms += duration_native;
            total_runs++;

            if (native_ok)
            {
                success_count++;

                double tol = (sc.noise_level > 0.0) ? 0.05 : 1e-3;
                if (sc.extreme_dist)
                {
                    tol = 1.0;
                }
                double err_rx = std::abs(native_rvec.x - true_rvec.x);
                double err_ry = std::abs(native_rvec.y - true_rvec.y);
                double err_rz = std::abs(native_rvec.z - true_rvec.z);
                double err_rvec = std::max({err_rx, err_ry, err_rz});

                double err_tx = std::abs(native_tvec.x - true_tvec.x);
                double err_ty = std::abs(native_tvec.y - true_tvec.y);
                double err_tz = std::abs(native_tvec.z - true_tvec.z);
                double err_tvec = std::max({err_tx, err_ty, err_tz});

                max_err_rvec = std::max(max_err_rvec, err_rvec);
                max_err_tvec = std::max(max_err_tvec, err_tvec);

                auto approx_equal = [](double a, double b, double tolerance)
                {
                    return std::abs(a - b) <= tolerance + tolerance * std::abs(b);
                };

                CHECK_MESSAGE(approx_equal(native_rvec.x, true_rvec.x, tol),
                              "Rotation X mismatch vs true: native=" << native_rvec.x << " vs true=" << true_rvec.x);
                CHECK_MESSAGE(approx_equal(native_rvec.y, true_rvec.y, tol),
                              "Rotation Y mismatch vs true: native=" << native_rvec.y << " vs true=" << true_rvec.y);
                CHECK_MESSAGE(approx_equal(native_rvec.z, true_rvec.z, tol),
                              "Rotation Z mismatch vs true: native=" << native_rvec.z << " vs true=" << true_rvec.z);

                CHECK_MESSAGE(approx_equal(native_tvec.x, true_tvec.x, tol * 100.0),
                              "Translation X mismatch vs true: native=" << native_tvec.x << " vs true=" << true_tvec.x);
                CHECK_MESSAGE(approx_equal(native_tvec.y, true_tvec.y, tol * 100.0),
                              "Translation Y mismatch vs true: native=" << native_tvec.y << " vs true=" << true_tvec.y);
                CHECK_MESSAGE(approx_equal(native_tvec.z, true_tvec.z, tol * 100.0),
                              "Translation Z mismatch vs true: native=" << native_tvec.z << " vs true=" << true_tvec.z);
            }
            else
            {
                bool expected_failure = sc.coplanar || sc.noise_level > 0.0 || !sc.use_extrinsic_guess;
                CHECK_MESSAGE(expected_failure, "Solver failed to converge in well-conditioned scenario");
            }
        }

        std::cout << "  Success (converged): " << success_count << "/" << runs_per_scenario << std::endl;
        std::cout << "  Max rotation diff vs true: " << max_err_rvec << std::endl;
        std::cout << "  Max translation diff vs true: " << max_err_tvec << std::endl;
    }

    std::cout << "\n=== Performance Summary ===" << std::endl;
    std::cout << "Total runs: " << total_runs << std::endl;
    std::cout << "Native LM solver total time: " << total_time_native_ms << " ms (avg: " << (total_time_native_ms * 1000.0 / total_runs) << " us/run)" << std::endl;
}


TEST_CASE("Testing POD Math Structures and Frame Serializability & Layout Compatibility")
{
    using TestBasis = Gaze::SpacedBasis<Gaze::Space::GodotFaceLocal, Gaze::Space::GodotCamera>;

    // 1. Verify offsetof checks to ensure zero internal padding/gaps
    CHECK(offsetof(Gaze::GodotDisplayVector2, x) == 0);
    CHECK(offsetof(Gaze::GodotDisplayVector2, y) == 8);
    CHECK(sizeof(Gaze::GodotDisplayVector2) == 16);

    CHECK(offsetof(Gaze::GazeRect, x) == 0);
    CHECK(offsetof(Gaze::GazeRect, y) == 4);
    CHECK(offsetof(Gaze::GazeRect, width) == 8);
    CHECK(offsetof(Gaze::GazeRect, height) == 12);
    CHECK(sizeof(Gaze::GazeRect) == 16);

    CHECK(offsetof(Gaze::GodotCameraVector3, x) == 0);
    CHECK(offsetof(Gaze::GodotCameraVector3, y) == 8);
    CHECK(offsetof(Gaze::GodotCameraVector3, z) == 16);
    CHECK(sizeof(Gaze::GodotCameraVector3) == 24);

    CHECK(offsetof(TestBasis, x) == 0);
    CHECK(offsetof(TestBasis, y) == 24);
    CHECK(offsetof(TestBasis, z) == 48);
    CHECK(sizeof(TestBasis) == 72);

    CHECK(offsetof(Gaze::GodotFaceTransform3D, basis) == 0);
    CHECK(offsetof(Gaze::GodotFaceTransform3D, origin) == 72);
    CHECK(sizeof(Gaze::GodotFaceTransform3D) == 96);

    CHECK(offsetof(Gaze::Frame, width) == 0);
    CHECK(offsetof(Gaze::Frame, height) == 4);
    CHECK(offsetof(Gaze::Frame, data) == 8);
    CHECK(offsetof(Gaze::Frame, timestamp) == 16);
    CHECK(sizeof(Gaze::Frame) == 24);

    // 2. Validate round-trip serializability via memcpy
    {
        Gaze::GodotDisplayVector2 v2(1.23, 4.56);
        unsigned char buf[sizeof(Gaze::GodotDisplayVector2)];
        std::memcpy(buf, &v2, sizeof(v2));
        Gaze::GodotDisplayVector2 v2_out;
        std::memcpy(&v2_out, buf, sizeof(v2_out));
        CHECK(v2_out.x == v2.x);
        CHECK(v2_out.y == v2.y);
    }
    {
        Gaze::GazeRect rect(1.0f, 2.0f, 3.0f, 4.0f);
        unsigned char buf[sizeof(Gaze::GazeRect)];
        std::memcpy(buf, &rect, sizeof(rect));
        Gaze::GazeRect rect_out;
        std::memcpy(&rect_out, buf, sizeof(rect_out));
        CHECK(rect_out.x == rect.x);
        CHECK(rect_out.y == rect.y);
        CHECK(rect_out.width == rect.width);
        CHECK(rect_out.height == rect.height);
    }
    {
        Gaze::GodotCameraVector3 v3(1.1, -2.2, 3.3);
        unsigned char buf[sizeof(Gaze::GodotCameraVector3)];
        std::memcpy(buf, &v3, sizeof(v3));
        Gaze::GodotCameraVector3 v3_out;
        std::memcpy(&v3_out, buf, sizeof(v3_out));
        CHECK(v3_out.x == v3.x);
        CHECK(v3_out.y == v3.y);
        CHECK(v3_out.z == v3.z);
    }
    {
        TestBasis basis(Gaze::GodotCameraVector3(1., 2., 3.), Gaze::GodotCameraVector3(4., 5., 6.), Gaze::GodotCameraVector3(7., 8., 9.));
        unsigned char buf[sizeof(TestBasis)];
        std::memcpy(buf, &basis, sizeof(basis));
        TestBasis basis_out;
        std::memcpy(&basis_out, buf, sizeof(basis_out));
        CHECK(basis_out.x.x == basis.x.x);
        CHECK(basis_out.x.y == basis.x.y);
        CHECK(basis_out.x.z == basis.x.z);
        CHECK(basis_out.y.x == basis.y.x);
        CHECK(basis_out.y.y == basis.y.y);
        CHECK(basis_out.y.z == basis.y.z);
        CHECK(basis_out.z.x == basis.z.x);
        CHECK(basis_out.z.y == basis.z.y);
        CHECK(basis_out.z.z == basis.z.z);
    }
    {
        Gaze::GodotFaceTransform3D transform(
            TestBasis(Gaze::GodotCameraVector3(1., 2., 3.), Gaze::GodotCameraVector3(4., 5., 6.), Gaze::GodotCameraVector3(7., 8., 9.)),
            Gaze::GodotCameraVector3(10., 11., 12.));
        unsigned char buf[sizeof(Gaze::GodotFaceTransform3D)];
        std::memcpy(buf, &transform, sizeof(transform));
        Gaze::GodotFaceTransform3D transform_out;
        std::memcpy(&transform_out, buf, sizeof(transform_out));
        CHECK(transform_out.basis.x.x == transform.basis.x.x);
        CHECK(transform_out.basis.y.y == transform.basis.y.y);
        CHECK(transform_out.basis.z.z == transform.basis.z.z);
        CHECK(transform_out.origin.x == transform.origin.x);
        CHECK(transform_out.origin.y == transform.origin.y);
        CHECK(transform_out.origin.z == transform.origin.z);
    }
    {
        Gaze::Frame frame;
        frame.width = 1920;
        frame.height = 1080;
        uint8_t dummy_data[10] = {1, 2, 3};
        frame.data = dummy_data;
        frame.timestamp = 12345.6789;

        unsigned char buf[sizeof(Gaze::Frame)];
        std::memcpy(buf, &frame, sizeof(frame));
        Gaze::Frame frame_out;
        std::memcpy(&frame_out, buf, sizeof(frame_out));

        CHECK(frame_out.width == frame.width);
        CHECK(frame_out.height == frame.height);
        CHECK(frame_out.data == frame.data);
        CHECK(frame_out.timestamp == frame.timestamp);
    }
}

inline std::vector<uint8_t> read_binary_file(const std::string &filepath)
{
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return {};
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char *>(buffer.data()), size);
    return buffer;
}

TEST_CASE("Testing GazeTrackingPipeline Concurrency and Multi-Frame Queue Stress")
{
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";

    std::vector<uint8_t> yunet_data = read_binary_file(yunet_path);
    std::vector<uint8_t> gaze_data = read_binary_file(gaze_path);
    std::vector<uint8_t> eye_data = read_binary_file("project/addons/godot-gaze/models/open_closed_eye.ort");
    if (eye_data.empty()) {
        eye_data = read_binary_file("project/addons/godot-gaze/models/mediapipe_eye_openness.ort");
    }

    REQUIRE_MESSAGE(!yunet_data.empty(), "Failed to read YuNet model data");
    REQUIRE_MESSAGE(!gaze_data.empty(), "Failed to read Gaze model data");
    REQUIRE_MESSAGE(!eye_data.empty(), "Failed to read Eye state model data");

    // 2. Instantiate and Initialize GazeTrackingPipeline
    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_data, gaze_data, eye_data) == true);

    // 3. Set a sample config and start the worker thread
    PipelineConfig config;
    config.ipd_mm = 63.0;
    pipeline.set_config(config);

    pipeline.start();

    // 4. Load a real face image to feed the pipeline
    LoadedImage img = load_test_image("tests/resources/self_center.jpg");
    REQUIRE_MESSAGE(!img.data.empty(), "Failed to load test image for stress test");

    // 5. Stress test mailbox by pushing 20 frames sequentially, waiting for each result
    const int num_frames = 20;
    std::vector<GazeFrameData> results;
    for (int i = 0; i < num_frames; ++i)
    {
        GazeFrameData *req = pipeline.frame_pool.take();
        REQUIRE(req != nullptr);
        req->camera_raw_bgr = img.data;
        req->camera_width = img.width;
        req->camera_height = img.height;
        req->timestamp = (double)i;
        req->face_rid_val = 1000 + i;
        req->eye_rid_val = 2000 + i;

        pipeline.push_frame_request(req);

        // Wait for the popped result
        GazeFrameData *res = nullptr;
        auto push_time = std::chrono::steady_clock::now();
        while (true)
        {
            if (pipeline.pop_result(&res))
            {
                results.push_back(*res);
                pipeline.frame_pool.release(res);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - push_time);
            if (elapsed >= std::chrono::seconds(2))
            {
                break;
            }
        }
    }

    // Assert that we successfully processed all 20 frames without stalling
    REQUIRE_MESSAGE(results.size() == num_frames,
                    "Pipeline stalled! Processed only " << results.size() << " out of " << num_frames << " frames.");

    // Validate the results contain expected outputs
    for (int i = 0; i < num_frames; ++i)
    {
        const auto &res = results[i];
        CHECK(res.face_detected == true);
        CHECK(res.gaze_success == true);
        CHECK(res.face_rid_val == 1000 + i);
        CHECK(res.eye_rid_val == 2000 + i);
    }

    // 7. Stop the pipeline safely
    pipeline.stop();
}

TEST_CASE("Testing GazeTrackingPipeline Thread-Safety and Race Conditions")
{
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";

    std::vector<uint8_t> yunet_data = read_binary_file(yunet_path);
    std::vector<uint8_t> gaze_data = read_binary_file(gaze_path);
    std::vector<uint8_t> eye_data = read_binary_file("project/addons/godot-gaze/models/open_closed_eye.ort");
    if (eye_data.empty()) {
        eye_data = read_binary_file("project/addons/godot-gaze/models/mediapipe_eye_openness.ort");
    }

    REQUIRE(!yunet_data.empty());
    REQUIRE(!gaze_data.empty());
    REQUIRE(!eye_data.empty());

    LoadedImage img = load_test_image("tests/resources/self_center.jpg");
    REQUIRE(!img.data.empty());

    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_data, gaze_data, eye_data) == true);

    std::atomic<bool> run_test{true};

    // Thread 1: Start/Stop loop
    std::thread start_stop_thread([&]()
                                  {
        while (run_test) {
            pipeline.start();
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            pipeline.stop();
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        } });

    // Thread 2: Config loop
    std::thread config_thread([&]()
                              {
        PipelineConfig c;
        c.ipd_mm = 63.0;
        int count = 0;
        while (run_test) {
            c.ipd_mm = 60.0 + (count++ % 10);
            pipeline.set_config(c);
            std::this_thread::sleep_for(std::chrono::microseconds(5));
        } });

    // Thread 3: Push frame request loop
    std::thread push_thread([&]()
                            {
        int count = 0;
        while (run_test) {
            GazeFrameData* req = pipeline.frame_pool.take();
            if (req) {
                req->camera_raw_bgr = img.data;
                req->camera_width = img.width;
                req->camera_height = img.height;
                req->timestamp = (double)count++;
                req->face_rid_val = 1000;
                req->eye_rid_val = 2000;
                pipeline.push_frame_request(req);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(5));
        } });

    // Thread 4: Pop result loop
    std::thread pop_thread([&]()
                           {
        while (run_test) {
            GazeFrameData* res = nullptr;
            if (pipeline.pop_result(&res)) {
                pipeline.frame_pool.release(res);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(5));
        } });

    // Thread 5: Initialize loop
    std::thread init_thread([&]()
                            {
        while (run_test) {
            pipeline.initialize(yunet_data, gaze_data, eye_data);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        } });

    // Let them run concurrently for 500 ms to see if a crash or deadlock is triggered
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    run_test = false;

    // Join all threads
    if (start_stop_thread.joinable())
        start_stop_thread.join();
    if (config_thread.joinable())
        config_thread.join();
    if (push_thread.joinable())
        push_thread.join();
    if (pop_thread.joinable())
        pop_thread.join();
    if (init_thread.joinable())
        init_thread.join();

    pipeline.stop();
}

TEST_CASE("Testing GazeTrackingPipeline Godot Camera Space Invariance Across Benchmark Fixtures")
{
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";
    std::string eye_path = "project/addons/godot-gaze/models/open_closed_eye.ort";

    std::vector<uint8_t> yunet_data = read_binary_file(yunet_path);
    std::vector<uint8_t> gaze_data = read_binary_file(gaze_path);
    std::vector<uint8_t> eye_data = read_binary_file(eye_path);

    REQUIRE(!yunet_data.empty());
    REQUIRE(!gaze_data.empty());
    REQUIRE(!eye_data.empty());

    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_data, gaze_data, eye_data) == true);
    pipeline.start();

    auto process_fixture = [&](const std::string &filename) -> GazeFrameData {
        pipeline.clear_work_queue();
        LoadedImage img = load_test_image("tests/resources/" + filename);
        REQUIRE(!img.data.empty());
        GazeFrameData *req = pipeline.frame_pool.take();
        REQUIRE(req != nullptr);
        req->camera_raw_bgr = img.data;
        req->camera_width = img.width;
        req->camera_height = img.height;
        req->timestamp = 1.0;
        req->face_rid_val = 1;
        req->eye_rid_val = 2;
        pipeline.push_frame_request(req);

        GazeFrameData result;
        GazeFrameData *res = nullptr;
        for (int retry = 0; retry < 500; ++retry) {
            if (pipeline.pop_result(&res)) {
                result = *res;
                pipeline.frame_pool.release(res);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return result;
    };

    // 1. self_center.jpg: Facing camera, head below camera
    {
        GazeFrameData res = process_fixture("self_center.jpg");
        REQUIRE(res.face_detected == true);
        REQUIRE(res.gaze_success == true);

        // In Godot Camera Space, head in front of camera is along -Z, below camera is along -Y
        CHECK(res.head_translation.z < -200.0);
        CHECK(res.head_translation.y < 0.0);

        // Head transform origin matches head_translation in Godot Camera Space
        CHECK(res.head_transform.origin.x == doctest::Approx(res.head_translation.x));
        CHECK(res.head_transform.origin.y == doctest::Approx(res.head_translation.y));
        CHECK(res.head_transform.origin.z == doctest::Approx(res.head_translation.z));

        // Forward vector -basis.z points towards the camera/screen (+Z > 0)
        GodotCameraVector3 head_fwd = -res.head_transform.basis.z;
        CHECK(head_fwd.z > 0.9);
        CHECK(std::abs(head_fwd.x) < 0.15);

        // Gaze direction points towards the screen (+Z > 0)
        CHECK(res.gaze_direction.z > 0.85);
    }

    // 2. self_left_left.jpg: Head turned display left (+X in Godot Camera Space), Gaze looking display left (+X in Godot Camera Space)
    {
        GazeFrameData res = process_fixture("self_left_left.jpg");
        REQUIRE(res.face_detected == true);
        REQUIRE(res.gaze_success == true);

        GodotCameraVector3 head_fwd = -res.head_transform.basis.z;
        CHECK(head_fwd.z > 0.85);
        CHECK(head_fwd.x > 0.1);
    }

    // 3. self_right_right.jpg: Head turned display right (-X in Godot Camera Space), Gaze looking display right (-X in Godot Camera Space)
    {
        GazeFrameData res = process_fixture("self_right_right.jpg");
        REQUIRE(res.face_detected == true);
        REQUIRE(res.gaze_success == true);

        GodotCameraVector3 head_fwd = -res.head_transform.basis.z;
        CHECK(head_fwd.z > 0.85);
        CHECK(head_fwd.x < -0.1);
    }

    // 4. self_roll_right.jpg: Head rolled to subject's right shoulder
    {
        GazeFrameData res = process_fixture("self_roll_right.jpg");
        REQUIRE(res.face_detected == true);
        REQUIRE(res.has_landmarks_2d == true);
    }

    // 5. self_roll_left.jpg: Head rolled to subject's left shoulder
    {
        GazeFrameData res = process_fixture("self_roll_left.jpg");
        REQUIRE(res.face_detected == true);
        REQUIRE(res.has_landmarks_2d == true);
    }

    pipeline.stop();
}

TEST_CASE("Testing resize_bgr_to_rgb bilinear filtering and color swap")
{
    // 2x2 source BGR image:
    // Pixel (0,0): B=10, G=20, R=30
    // Pixel (1,0): B=40, G=50, R=60
    // Pixel (0,1): B=70, G=80, R=90
    // Pixel (1,1): B=100, G=110, R=120
    uint8_t src[12] = {
        10, 20, 30,   40, 50, 60,
        70, 80, 90,   100, 110, 120
    };
    uint8_t dst[3] = {0};

    resize_bgr_to_rgb(src, 2, 2, dst, 1, 1);

    CHECK(dst[0] == 30); // R
    CHECK(dst[1] == 20); // G
    CHECK(dst[2] == 10); // B
}

#include <godot_cpp/variant/transform2d.hpp>

TEST_CASE("Testing Godot C++ Bindings Transform2D::xform_inv Scaling Bug")
{
    // Under scaling (e.g. Retina/High-DPI stretching), godot-cpp's Transform2D::xform_inv()
    // does not perform a mathematically correct inverse transform. It assumes the basis is
    // orthonormal (scale = 1.0) and uses transpose multiplication, which multiplies by the
    // scale instead of dividing by it.
    //
    // For a scale of 2.0 and vector (10, 10), the correct inverse is (5, 5).
    // But xform_inv(10, 10) returns (20, 20).
    //
    // We document and verify this behavior here as a regression check for the workaround
    // implemented using `viewport_transform.affine_inverse().xform()`.

    godot::Transform2D t(godot::Vector2(2.0, 0.0), godot::Vector2(0.0, 2.0), godot::Vector2(0.0, 0.0));
    godot::Vector2 v(10.0, 10.0);

    godot::Vector2 result = t.xform_inv(v);

    // Assert that the scaling bug multiplies the vector instead of dividing
    CHECK(result.x == doctest::Approx(20.0));
    CHECK(result.y == doctest::Approx(20.0));

    // Assert that using affine_inverse().xform() yields the correct inverse coordinate (5.0)
    godot::Vector2 correct_inverse = t.affine_inverse().xform(v);
    CHECK(correct_inverse.x == doctest::Approx(5.0));
    CHECK(correct_inverse.y == doctest::Approx(5.0));
}

#include "log.hpp"
#include <vector>
#include <string>

TEST_CASE("Testing Log Verbosity Filtering")
{
    // Save original verbosity and log handler
    int original_verbosity = Gaze::get_log_verbosity().load();
    auto original_handler = Gaze::get_log_handler().load();

    // Set up a mock log handler to collect logs
    static std::vector<std::pair<bool, std::string>> captured_logs;
    captured_logs.clear();

    Gaze::register_log_handler([](bool is_error, const char* msg) {
        captured_logs.push_back({is_error, std::string(msg)});
    });

    // Test Case 1: Verbosity 1 (Default)
    Gaze::set_log_verbosity(1);
    Gaze::log_info("standard_event");
    Gaze::log_info(2, "verbose_event");
    Gaze::log_info(3, "analysis_event");
    Gaze::log_info(4, "capture_event");

    REQUIRE(captured_logs.size() == 1);
    CHECK(captured_logs[0].second.find("standard_event") != std::string::npos);
    captured_logs.clear();

    // Test Case 2: Verbosity 2
    Gaze::set_log_verbosity(2);
    Gaze::log_info("standard_event");
    Gaze::log_info(2, "verbose_event");
    Gaze::log_info(3, "analysis_event");
    Gaze::log_info(4, "capture_event");

    REQUIRE(captured_logs.size() == 2);
    CHECK(captured_logs[0].second.find("standard_event") != std::string::npos);
    CHECK(captured_logs[1].second.find("verbose_event") != std::string::npos);
    captured_logs.clear();

    // Test Case 3: Verbosity 3
    Gaze::set_log_verbosity(3);
    Gaze::log_info("standard_event");
    Gaze::log_info(2, "verbose_event");
    Gaze::log_info(3, "analysis_event");
    Gaze::log_info(4, "capture_event");

    REQUIRE(captured_logs.size() == 3);
    CHECK(captured_logs[0].second.find("standard_event") != std::string::npos);
    CHECK(captured_logs[1].second.find("verbose_event") != std::string::npos);
    CHECK(captured_logs[2].second.find("analysis_event") != std::string::npos);
    captured_logs.clear();

    // Test Case 4: Verbosity 4
    Gaze::set_log_verbosity(4);
    Gaze::log_info("standard_event");
    Gaze::log_info(2, "verbose_event");
    Gaze::log_info(3, "analysis_event");
    Gaze::log_info(4, "capture_event");

    REQUIRE(captured_logs.size() == 4);
    CHECK(captured_logs[0].second.find("standard_event") != std::string::npos);
    CHECK(captured_logs[1].second.find("verbose_event") != std::string::npos);
    CHECK(captured_logs[2].second.find("analysis_event") != std::string::npos);
    CHECK(captured_logs[3].second.find("capture_event") != std::string::npos);
    captured_logs.clear();

    // Test Case 5: Verbosity 0 (Quiet)
    Gaze::set_log_verbosity(0);
    Gaze::log_info("standard_event");
    Gaze::log_info(2, "verbose_event");
    Gaze::log_info(3, "analysis_event");
    Gaze::log_info(4, "capture_event");

    CHECK(captured_logs.empty() == true);

    // Restore original handler and verbosity
    Gaze::register_log_handler(original_handler);
    Gaze::set_log_verbosity(original_verbosity);
}

TEST_CASE("Testing Dynamic Continuous Head Roll Sequence with Zero Frame Drops")
{
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";
    std::string eye_path = "project/addons/godot-gaze/models/open_closed_eye.ort";

    std::vector<uint8_t> yunet_data = read_binary_file(yunet_path);
    std::vector<uint8_t> gaze_data = read_binary_file(gaze_path);
    std::vector<uint8_t> eye_data = read_binary_file(eye_path);

    REQUIRE(!yunet_data.empty());
    REQUIRE(!gaze_data.empty());
    REQUIRE(!eye_data.empty());

    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_data, gaze_data, eye_data) == true);
    pipeline.start();

    LoadedImage base_img = load_test_image("tests/resources/self_center2.jpg");
    REQUIRE(!base_img.data.empty());

    // Generate dynamic sweep trajectory: 0 -> +45 -> -45 -> 0 in 3-degree steps (61 frames)
    std::vector<float> trajectory_deg;
    for (int deg = 0; deg <= 45; deg += 3) trajectory_deg.push_back(static_cast<float>(deg));
    for (int deg = 42; deg >= -45; deg -= 3) trajectory_deg.push_back(static_cast<float>(deg));
    for (int deg = -42; deg <= 0; deg += 3) trajectory_deg.push_back(static_cast<float>(deg));

    int total_frames = static_cast<int>(trajectory_deg.size());
    int detected_frames = 0;

    for (int i = 0; i < total_frames; ++i) {
        float angle_deg = trajectory_deg[i];
        float angle_rad = angle_deg * (3.141592653589793f / 180.0f);

        std::vector<unsigned char> rot_bgr(base_img.width * base_img.height * 3);
        rotate_image(base_img.data.data(), base_img.width, base_img.height, rot_bgr.data(), -angle_rad);

        GazeFrameData *req = pipeline.frame_pool.take();
        REQUIRE(req != nullptr);
        req->camera_raw_bgr = rot_bgr;
        req->camera_width = base_img.width;
        req->camera_height = base_img.height;
        req->timestamp = static_cast<double>(i) * 0.033;
        req->auto_roll_enabled = true;
        pipeline.push_frame_request(req);

        GazeFrameData *res = nullptr;
        for (int retry = 0; retry < 500; ++retry) {
            if (pipeline.pop_result(&res)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        REQUIRE(res != nullptr);

        if (res->face_detected) {
            detected_frames++;
            float solved_roll_deg = static_cast<float>(res->head_rotation.z * (180.0 / 3.141592653589793));
            CHECK(std::abs(solved_roll_deg - angle_deg) < 6.0f);
        } else {
            MESSAGE("Dropped frame at angle: ", angle_deg, " deg (frame index ", i, ")");
        }

        pipeline.frame_pool.release(res);
    }

    pipeline.stop();

    // Strict zero-drop invariant: all frames in dynamic sweep must be detected continuously
    CHECK_MESSAGE(detected_frames == total_frames, "Dropped ", (total_frames - detected_frames), " / ", total_frames, " frames during dynamic head roll sweep!");
}

TEST_CASE("Testing Head Roll Landmark Detection")
{
    std::string face_detector_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    
    // 1. Test Anatomical Left Ear to Shoulder Tilt (self_roll_left.jpg)
    {
        ORTYuNetDetector detector(face_detector_path);
        REQUIRE(detector.initialize() == true);

        LoadedImage img = load_test_image("tests/resources/self_roll_left.jpg");
        REQUIRE(!img.data.empty());

        Frame frame;
        frame.width = img.width;
        frame.height = img.height;
        frame.timestamp = 0.0;
        frame.data = img.data.data();

        YuNetResult res;
        bool detector_success = detector.process_frame(frame, res, 0.0f);
        REQUIRE(detector_success == true);
        REQUIRE(res.face_detected == true);
    }

    // 2. Test Anatomical Right Ear to Shoulder Tilt (self_roll_right.jpg)
    {
        ORTYuNetDetector detector(face_detector_path);
        REQUIRE(detector.initialize() == true);

        LoadedImage img = load_test_image("tests/resources/self_roll_right.jpg");
        REQUIRE(!img.data.empty());

        Frame frame;
        frame.width = img.width;
        frame.height = img.height;
        frame.timestamp = 0.0;
        frame.data = img.data.data();

        YuNetResult res;
        bool detector_success = detector.process_frame(frame, res, 0.0f);
        REQUIRE(detector_success == true);
        REQUIRE(res.face_detected == true);
    }
}

TEST_CASE("Testing Gaze Direction Vector Sign and Ray Projection in Calibration")
{
    // Simulate raw gaze model output in OpenCV space: dx=0, dy=0, dz=1 (staring straight at camera)
    OpenCVCameraVector3 raw_gaze_dir_cv(0.0, 0.0, 1.0);
    OpenCVCameraVector3 origin_cv(0.0, 0.0, 600.0); // 600mm in front of camera

    // Convert to Godot camera space
    GodotCameraVector3 origin_cam = CoordinateConversions::to_godot_camera(origin_cv);
    GodotCameraVector3 dir_cam = CoordinateConversions::to_godot_camera(OpenVINOGazeVector3(0.0, 0.0, -1.0));

    // Gaze origin in camera space must be at Z < 0 (in front of camera/screen plane)
    CHECK(origin_cam.z < 0.0);

    // Forward gaze direction vector in camera space MUST point towards screen (Z > 0)
    CHECK(dir_cam.z > 0.0);

    // Test ray projection onto a 600x340 mm screen with camera at top offset (0, 170, 0)
    Gaze::ProjectionEngine proj_engine;
    proj_engine.set_screen_size_pixels(GodotDisplayVector2(1920.0, 1080.0));
    proj_engine.set_screen_size_mm(SpacedVector2<Space::GodotDisplayMm>(600.0, 340.0));
    proj_engine.set_camera_placement(Gaze::CameraPlacement(GodotCameraVector3(0.0, 170.0, 0.0), 0.0));

    GodotDisplayVector2 pixel;
    bool proj_success = proj_engine.project_gaze(origin_cam, dir_cam, pixel);
    CHECK(proj_success == true);

    // Ray projection should land near center of screen
    CHECK(pixel.x == doctest::Approx(960.0).epsilon(50.0));
    CHECK(pixel.y == doctest::Approx(540.0).epsilon(50.0));
}

TEST_CASE("Testing BioCalibration Isolation on Eye Gaze vs Nose Gaze")
{
    Gaze::ProjectionEngine proj_engine;
    proj_engine.set_screen_size_pixels(GodotDisplayVector2(1920.0, 1080.0));
    proj_engine.set_screen_size_mm(SpacedVector2<Space::GodotDisplayMm>(600.0, 340.0));
    proj_engine.set_camera_placement(Gaze::CameraPlacement(GodotCameraVector3(0.0, 170.0, 0.0), 0.0));

    GodotCameraVector3 uncal_dir(0.0, 0.0, 1.0);
    GodotCameraVector3 head_fwd(0.0, 0.0, 1.0);

    // 1. Uncalibrated state (bias = 0)
    proj_engine.set_calibration(Gaze::GazeCalibration(0.0, 0.0));
    GodotCameraVector3 biased_dir_zero = proj_engine.apply_3d_bias(uncal_dir);
    CHECK(biased_dir_zero.x == doctest::Approx(0.0));
    CHECK(biased_dir_zero.y == doctest::Approx(0.0));
    CHECK(biased_dir_zero.z == doctest::Approx(1.0));

    // 2. Set significant BioCalibration pitch/yaw bias
    proj_engine.set_calibration(Gaze::GazeCalibration(0.1, -0.08)); // 0.1 pitch, -0.08 yaw
    GodotCameraVector3 biased_dir_cal = proj_engine.apply_3d_bias(uncal_dir);

    // BioCalibration MUST mutate eye gaze direction
    CHECK(biased_dir_cal.x != doctest::Approx(uncal_dir.x));
    CHECK(biased_dir_cal.y != doctest::Approx(uncal_dir.y));

    // 3. Head pose / Nose gaze ray MUST remain uncalibrated
    CHECK(head_fwd.x == doctest::Approx(0.0));
    CHECK(head_fwd.y == doctest::Approx(0.0));
    CHECK(head_fwd.z == doctest::Approx(1.0));
}

TEST_CASE("Testing Full Pipeline Rotation Counter-Measures")
{
    // Test that head forward vector remains invariant under image rotation
    GodotFaceTransform3D t_unrot = Gaze::CoordinateConversions::opencv_pose_to_godot_camera_transform(
        OpenCVCameraVector3(0.0, 0.0, 600.0),
        OpenCVCameraVector3(0.0, 0.0, 0.0)
    );
    GodotCameraVector3 head_fwd = -t_unrot.basis.z.normalized();
    CHECK(head_fwd.x == doctest::Approx(0.0));
    CHECK(head_fwd.y == doctest::Approx(0.0));
    CHECK(head_fwd.z == doctest::Approx(1.0));
}

TEST_CASE("Testing Physical Gaze Ray Direction Invariants")
{
    Gaze::ProjectionEngine proj_engine;
    proj_engine.set_screen_size_pixels(GodotDisplayVector2(1920.0, 1080.0));
    proj_engine.set_screen_size_mm(SpacedVector2<Space::GodotDisplayMm>(600.0, 340.0));
    proj_engine.set_camera_placement(Gaze::CameraPlacement(GodotCameraVector3(0.0, 170.0, 0.0), 0.0));

    GodotCameraVector3 origin(0.0, 0.0, -600.0);
    GodotDisplayVector2 center_pixel;
    proj_engine.project_gaze(origin, GodotCameraVector3(0.0, 0.0, 1.0), center_pixel);

    // 1. Gazing Anatomic Left (Camera Right: +X) must project to screen LEFT (pixel.x < center_pixel.x)
    GodotCameraVector3 left_dir(0.2, 0.0, 0.98);
    GodotDisplayVector2 left_pixel;
    bool left_ok = proj_engine.project_gaze(origin, left_dir, left_pixel);
    CHECK(left_ok == true);
    CHECK(left_pixel.x < center_pixel.x);

    // 2. Gazing Anatomic Right (Camera Left: -X) must project to screen RIGHT (pixel.x > center_pixel.x)
    GodotCameraVector3 right_dir(-0.2, 0.0, 0.98);
    GodotDisplayVector2 right_pixel;
    bool right_ok = proj_engine.project_gaze(origin, right_dir, right_pixel);
    CHECK(right_ok == true);
    CHECK(right_pixel.x > center_pixel.x);

    // 3. Gazing UP (Camera Up: +Y) must project towards screen TOP (pixel.y < center_pixel.y)
    GodotCameraVector3 up_dir(0.0, 0.2, 0.98);
    GodotDisplayVector2 up_pixel;
    bool up_ok = proj_engine.project_gaze(origin, up_dir, up_pixel);
    CHECK(up_ok == true);
    CHECK(up_pixel.y < center_pixel.y);

    // 4. Gazing DOWN (Camera Down: -Y) must project towards screen BOTTOM (pixel.y > center_pixel.y)
    GodotCameraVector3 down_dir(0.0, -0.2, 0.98);
    GodotDisplayVector2 down_pixel;
    bool down_ok = proj_engine.project_gaze(origin, down_dir, down_pixel);
    CHECK(down_ok == true);
    CHECK(down_pixel.y > center_pixel.y);
}

TEST_CASE("Coordinate Space Transformation Matrices Properties and Canonical Vector Mappings")
{
    // Test canonical gaze direction vector mapping:
    // Gazing subject right / display left (+x_onnx, -z_onnx forward) -> +X_cam (camera right / display left), +Z_cam (screen plane)
    GodotCameraVector3 screen_left_gaze = CoordinateConversions::to_godot_camera(OpenVINOGazeVector3(0.5, 0.0, -0.866));
    CHECK(screen_left_gaze.x > 0.0);
    CHECK(screen_left_gaze.z > 0.0); // Points towards display screen plane (+Z)

    // Gazing subject left / display right (-x_onnx, -z_onnx forward) -> -X_cam (camera left / display right), +Z_cam
    GodotCameraVector3 screen_right_gaze = CoordinateConversions::to_godot_camera(OpenVINOGazeVector3(-0.5, 0.0, -0.866));
    CHECK(screen_right_gaze.x < 0.0);
    CHECK(screen_right_gaze.z > 0.0);

    // Looking UP (+y_onnx in OpenVINO space) -> +Y_cam (screen top)
    GodotCameraVector3 up_gaze = CoordinateConversions::to_godot_camera(OpenVINOGazeVector3(0.0, 0.5, -0.866));
    CHECK(up_gaze.y > 0.0);
    CHECK(up_gaze.z > 0.0);

    // Looking DOWN (-y_onnx in OpenVINO space) -> -Y_cam (screen bottom)
    GodotCameraVector3 down_gaze = CoordinateConversions::to_godot_camera(OpenVINOGazeVector3(0.0, -0.5, -0.866));
    CHECK(down_gaze.y < 0.0);
    CHECK(down_gaze.z > 0.0);

    // Head Transform Chain Vector Mapping Test
    // For unrotated head (rvec = 0), face forward vector in Godot face space is -Z (0, 0, -1)
    OpenCVCameraVector3 translation(0.0, 0.0, 800.0);
    OpenCVCameraVector3 rotation_zero(0.0, 0.0, 0.0);
    GodotFaceTransform3D transform_zero = Gaze::CoordinateConversions::opencv_pose_to_godot_camera_transform(translation, rotation_zero);
    GodotCameraVector3 fwd_zero = -transform_zero.basis.z.normalized();
    CHECK(fwd_zero.z > 0.9); // Points towards display screen plane (+Z_cam)

    // Head turned anatomic left (rvec.y < 0 in OpenCV PnP solver, CCW rotation about +Y down) -> Display Left (+X_cam)
    OpenCVCameraVector3 rotation_left(0.0, -0.15, 0.0);
    GodotFaceTransform3D transform_left = Gaze::CoordinateConversions::opencv_pose_to_godot_camera_transform(translation, rotation_left);
    GodotCameraVector3 fwd_left = -transform_left.basis.z.normalized();
    CHECK(fwd_left.x > 0.05); // Must point towards display left (+X_cam)
    CHECK(fwd_left.z > 0.9);

    // Head turned anatomic right (rvec.y > 0 in OpenCV PnP solver, CW rotation about +Y down) -> Display Right (-X_cam)
    OpenCVCameraVector3 rotation_right(0.0, 0.15, 0.0);
    GodotFaceTransform3D transform_right = Gaze::CoordinateConversions::opencv_pose_to_godot_camera_transform(translation, rotation_right);
    GodotCameraVector3 fwd_right = -transform_right.basis.z.normalized();
    CHECK(fwd_right.x < -0.05); // Must point towards display right (-X_cam)
    CHECK(fwd_right.z > 0.9);
}

TEST_CASE("Investigating Pitch Clamping and PnP Sensitivity under Pitch Sweeps")
{
    double fx = 1000.0, cx = 320.0, cy = 240.0;

    std::vector<Gaze::OpenCVFaceVector3> model_points = Gaze::FaceModelGeometry::get_5pt_model_points();

    // Simulate real 2D landmark foreshortening of a face tilting back by +30 deg (+0.523 rad) at Z=700mm
    Gaze::OpenCVCameraVector3 true_rvec(0.523, 0.0, 0.0); // +30 deg pitch up
    Gaze::OpenCVCameraVector3 true_tvec(0.0, 0.0, 700.0);
    Gaze::SpacedBasis<Gaze::Space::OpenCVFaceModel, Gaze::Space::OpenCVCamera> R_true = Gaze::rodrigues_to_basis<Gaze::Space::OpenCVFaceModel, Gaze::Space::OpenCVCamera>(true_rvec);

    std::vector<Gaze::SpacedVector2<Gaze::Space::GodotCameraWorkingImagePixels>> img_pts(5);
    for (size_t i = 0; i < 5; ++i) {
        Gaze::OpenCVCameraVector3 P_cam = R_true.transform(model_points[i]) + true_tvec;
        img_pts[i] = Gaze::SpacedVector2<Gaze::Space::GodotCameraWorkingImagePixels>(fx * (P_cam.x / P_cam.z) + cx, fx * (P_cam.y / P_cam.z) + cy);
    }

    // Measure IPD in 2D image between Left Eye (2) and Right Eye (1)
    double eye_dist_px = std::hypot(img_pts[2].x - img_pts[1].x, img_pts[2].y - img_pts[1].y);
    double ipd_3d = 2.0 * Gaze::FaceModelGeometry::EYE_X;
    double z_ipd = (ipd_3d * fx) / eye_dist_px;

    // Run unconstrained PnP
    Gaze::OpenCVCameraVector3 unconstrained_rvec(0.0, 0.0, 0.0);
    Gaze::OpenCVCameraVector3 unconstrained_tvec(0.0, 0.0, 700.0);
    Gaze::SQPnPSolver::solve_rvec(model_points, img_pts, fx, fx, cx, cy, unconstrained_rvec, unconstrained_tvec);

    std::cout << "[PnP Foreshortening Test] True Pitch: " << true_rvec.x << " rad (" << true_rvec.x * 57.2958 << " deg)" << std::endl;
    std::cout << "  Unconstrained PnP Pitch: " << unconstrained_rvec.x << " rad (" << unconstrained_rvec.x * 57.2958 << " deg) | Z: " << unconstrained_tvec.z << " mm" << std::endl;
    std::cout << "  IPD-Calculated Z Depth: " << z_ipd << " mm" << std::endl;

    CHECK(z_ipd == doctest::Approx(741.338).epsilon(0.01));
}

TEST_CASE("Testing Closed-Form DLT Pose Initialization (solve_pnp_dlt)")
{
    double fx = 1000.0, cx = 320.0, cy = 240.0;
    std::vector<Gaze::OpenCVFaceVector3> model_points = {
        Gaze::OpenCVFaceVector3(-Gaze::FaceModelGeometry::EYE_X, Gaze::FaceModelGeometry::EYE_Y, Gaze::FaceModelGeometry::EYE_Z),
        Gaze::OpenCVFaceVector3(Gaze::FaceModelGeometry::EYE_X, Gaze::FaceModelGeometry::EYE_Y, Gaze::FaceModelGeometry::EYE_Z),
        Gaze::OpenCVFaceVector3(0.0, Gaze::FaceModelGeometry::DEFAULT_NOSE_Y, Gaze::FaceModelGeometry::DEFAULT_NOSE_Z),
        Gaze::OpenCVFaceVector3(-Gaze::FaceModelGeometry::MOUTH_X, Gaze::FaceModelGeometry::MOUTH_Y, Gaze::FaceModelGeometry::MOUTH_Z),
        Gaze::OpenCVFaceVector3(Gaze::FaceModelGeometry::MOUTH_X, Gaze::FaceModelGeometry::MOUTH_Y, Gaze::FaceModelGeometry::MOUTH_Z)
    };

    Gaze::OpenCVCameraVector3 true_rvec(0.15, -0.10, 0.05);
    Gaze::OpenCVCameraVector3 true_tvec(20.0, -10.0, 680.0);
    Gaze::SpacedBasis<Gaze::Space::OpenCVFaceModel, Gaze::Space::OpenCVCamera> R_true = Gaze::rodrigues_to_basis<Gaze::Space::OpenCVFaceModel, Gaze::Space::OpenCVCamera>(true_rvec);

    std::vector<Gaze::SpacedVector2<Gaze::Space::GodotCameraWorkingImagePixels>> img_pts(5);
    for (size_t i = 0; i < 5; ++i) {
        Gaze::OpenCVCameraVector3 P_cam = R_true.transform(model_points[i]) + true_tvec;
        img_pts[i] = Gaze::SpacedVector2<Gaze::Space::GodotCameraWorkingImagePixels>(fx * (P_cam.x / P_cam.z) + cx, fx * (P_cam.y / P_cam.z) + cy);
    }

    Gaze::OpenCVCameraVector3 pnp_rvec, pnp_tvec;
    bool pnp_ok = Gaze::SQPnPSolver::solve_rvec(model_points, img_pts, fx, fx, cx, cy, pnp_rvec, pnp_tvec);
    REQUIRE(pnp_ok == true);

    CHECK(pnp_tvec.z == doctest::Approx(680.0).epsilon(0.05));
    CHECK(pnp_rvec.z == doctest::Approx(0.05).epsilon(0.05));
}

TEST_CASE("Testing Device Calibration Window Offset and Top-Bezel Offset Invariants")
{
    Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm> physical_size_mm(300.0, 200.0);
    Gaze::GodotDisplayVector2 logical_size_px(1920.0, 1080.0);
    
    // Top-bezel camera placement: (0, 0, 0) relative to top-bezel center
    Gaze::GodotCameraVector3 default_cam_offset(0.0, 0.0, 0.0);
    CHECK(default_cam_offset.y == doctest::Approx(0.0));

    // Pixel size calculation: physical / logical
    Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm> pixel_size_mm(physical_size_mm.x / logical_size_px.x, physical_size_mm.y / logical_size_px.y);
    CHECK(pixel_size_mm.x == doctest::Approx(300.0 / 1920.0));
    CHECK(pixel_size_mm.y == doctest::Approx(200.0 / 1080.0));

    // Forward ray straight into camera from (0, 0, -500)
    Gaze::GodotCameraVector3 origin(0.0, 0.0, -500.0);
    Gaze::GodotCameraVector3 dir(0.0, 0.0, 1.0); // (0, 0, 1) in Godot Camera Space
    Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm> pos_mm;
    bool ok = Gaze::project_ray_to_screen_mm(origin, dir, default_cam_offset, 0.0, physical_size_mm, pos_mm);
    REQUIRE(ok == true);
    
    // Straight-ahead ray hits camera position on screen plane (top-center: X=150mm, Y=0mm)
    CHECK(pos_mm.x == doctest::Approx(150.0).epsilon(0.01));
    CHECK(pos_mm.y == doctest::Approx(0.0).epsilon(0.01));

    // Screen pixel position: (960, 0)
    double scale_x = logical_size_px.x / physical_size_mm.x;
    double scale_y = logical_size_px.y / physical_size_mm.y;
    Gaze::GodotDisplayVector2 screen_px(pos_mm.x * scale_x, pos_mm.y * scale_y);
    CHECK(screen_px.x == doctest::Approx(960.0).epsilon(0.01));
    CHECK(screen_px.y == doctest::Approx(0.0).epsilon(0.01));

    // When window is offset at (100, 50), local pixel position is (860, -50)
    Gaze::GodotDisplayVector2 window_pos(100.0, 50.0);
    Gaze::GodotDisplayVector2 local_px = screen_px - window_pos;
    CHECK(local_px.x == doctest::Approx(860.0).epsilon(0.01));
    CHECK(local_px.y == doctest::Approx(-50.0).epsilon(0.01));

    // Ray angled down toward screen center: origin=(0, 100, -500) pointing at (0, 0, 0) in screen coords
    Gaze::GodotCameraVector3 dir_to_center = Gaze::GodotCameraVector3(0.0, -100.0, 500.0).normalized();
    Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm> center_pos_mm;
    bool center_ok = Gaze::project_ray_to_screen_mm(origin, dir_to_center, default_cam_offset, 0.0, physical_size_mm, center_pos_mm);
    REQUIRE(center_ok == true);
    CHECK(center_pos_mm.x == doctest::Approx(150.0).epsilon(0.01));
    CHECK(center_pos_mm.y == doctest::Approx(100.0).epsilon(0.01));
}

TEST_CASE("ORTGazeModel Eye Crop Preprocessing Preserves BGR NCHW Channel Order")
{
    // Construct a synthetic 60x60 BGR image crop with distinct B, G, R patterns
    std::vector<uint8_t> bgr_crop(60 * 60 * 3);
    for (int i = 0; i < 60 * 60; ++i) {
        bgr_crop[i * 3 + 0] = 10;  // Blue
        bgr_crop[i * 3 + 1] = 50;  // Green
        bgr_crop[i * 3 + 2] = 200; // Red
    }

    std::vector<float> nchw_tensor(3 * 60 * 60, 0.0f);
    Gaze::ORTGazeModel::preprocess_eye_crop(bgr_crop.data(), nchw_tensor.data());

    constexpr int plane = 60 * 60;
    // Channel 0 must be Blue (10.0f)
    CHECK(nchw_tensor[0] == doctest::Approx(10.0f));
    CHECK(nchw_tensor[plane - 1] == doctest::Approx(10.0f));

    // Channel 1 must be Green (50.0f)
    CHECK(nchw_tensor[plane] == doctest::Approx(50.0f));
    CHECK(nchw_tensor[2 * plane - 1] == doctest::Approx(50.0f));

    // Channel 2 must be Red (200.0f)
    CHECK(nchw_tensor[2 * plane] == doctest::Approx(200.0f));
    CHECK(nchw_tensor[3 * plane - 1] == doctest::Approx(200.0f));
}

TEST_CASE("Hub-and-Spoke GodotCameraHintRolled to GodotCamera Space Transform Mapping")
{
    using TestBasis = Gaze::SpacedBasis<Gaze::Space::GodotFaceLocal, Gaze::Space::GodotCamera>;
    Gaze::GodotFaceTransform3D xform_identity(TestBasis::identity(), Gaze::GodotCameraVector3(10.0, 20.0, -500.0));
    
    // Zero roll hint returns identical transform
    Gaze::GodotFaceTransform3D unrolled_zero = Gaze::CoordinateConversions::godot_camera_hint_rolled_to_godot_camera(xform_identity, 0.0f);
    CHECK(unrolled_zero.origin.x == doctest::Approx(10.0));
    CHECK(unrolled_zero.origin.y == doctest::Approx(20.0));
    CHECK(unrolled_zero.origin.z == doctest::Approx(-500.0));

    // +90 deg roll hint (+PI/2 around +Z)
    float roll_90 = static_cast<float>(M_PI * 0.5);
    Gaze::GodotFaceTransform3D unrolled_90 = Gaze::CoordinateConversions::godot_camera_hint_rolled_to_godot_camera(xform_identity, roll_90);
    // (x, y) = (10, 20) rotated +90 deg clockwise (tilt right: +Y -> +X) -> (20, -10)
    CHECK(unrolled_90.origin.x == doctest::Approx(20.0));
    CHECK(unrolled_90.origin.y == doctest::Approx(-10.0));
    CHECK(unrolled_90.origin.z == doctest::Approx(-500.0));
}



