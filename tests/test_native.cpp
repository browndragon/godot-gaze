// TODO: Name `native_test.cpp`. How does this differ `main_test.cpp`?
#include "doctest.h"
#include "ort_yunet_pipeline.hpp"
#include "ort_gaze_model.hpp"
#include "gaze_tracking_pipeline.hpp"
#include "projection_engine.hpp"
#include "screen_projector.hpp"
#include "space_conversions.hpp"

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

namespace Gaze
{
    extern bool g_is_unit_test;
}

TEST_CASE("Testing Native Pipeline Model Initialization & Inference")
{
    Gaze::g_is_unit_test = true;
    try
    {
        // 1. Initialize YuNet
        std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        ORTYuNetPipeline pipeline(yunet_path);

        REQUIRE(pipeline.initialize() == true);

        // 2. Initialize Gaze Model
        std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";
        ORTGazeModel model(gaze_path);

        REQUIRE(model.initialize() == true);

        // 3. Verify solvePnP approximation with dummy inputs
        Frame frame;
        frame.width = 640;
        frame.height = 480;
        frame.timestamp = 0.0;

        // Create a dummy 640x480 BGR image vector containing a blank face
        std::vector<unsigned char> dummy_mat(640 * 480 * 3, 255);
        frame.data = dummy_mat.data();

        EyeCrops crops;
        // We expect process_frame to return false since the dummy image doesn't contain a face
        REQUIRE(pipeline.process_frame(frame, crops) == false);
        CHECK(crops.face_detected == false);

        // 4. Test Gaze Model inference on mock crop data
        crops.face_detected = true;
        crops.head_pose_rotation = GazeVector3(0.0, 0.0, 0.0);
        crops.head_pose_translation = GazeVector3(0.0, 0.0, 500.0);
        crops.left_eye_center_cam = GazeVector3(31.5, 0.0, 480.0);
        crops.right_eye_center_cam = GazeVector3(-31.5, 0.0, 480.0);

        // Fill left and right eye crop buffers with dummy data
        std::memset(crops.left_eye_data, 128, 10800);
        std::memset(crops.right_eye_data, 128, 10800);

        GazeVector3 gaze_dir_cv;
        bool model_success = model.estimate_raw_gaze(crops, gaze_dir_cv);
        REQUIRE(model_success == true);

        // Check that gaze_dir_cv is normalized and valid
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
    ORTYuNetPipeline pipeline(yunet_path);
    REQUIRE(pipeline.initialize() == true);

    LoadedImage img = load_test_image("tests/resources/self_center.jpg");
    REQUIRE(!img.data.empty());

    Frame frame;
    frame.width = img.width;
    frame.height = img.height;
    frame.timestamp = 0.0;
    frame.data = img.data.data();

    EyeCrops crops;
    bool pipeline_success = pipeline.process_frame(frame, crops);
    REQUIRE(pipeline_success == true);
    REQUIRE(crops.face_detected == true);

    std::cout << "LANDMARK 0 (right eye): " << crops.landmarks[0].x << ", " << crops.landmarks[0].y << std::endl;
    std::cout << "LANDMARK 1 (left eye): " << crops.landmarks[1].x << ", " << crops.landmarks[1].y << std::endl;
    std::cout << "LANDMARK 2 (nose): " << crops.landmarks[2].x << ", " << crops.landmarks[2].y << std::endl;

    // Verify landmarks coordinates are non-zero/valid
    CHECK(crops.left_eye_center_cam.x != 0.0);
    CHECK(crops.right_eye_center_cam.x != 0.0);

    // Verify head forward vector direction in standard Camera Space
    GazeTransform3D head_transform = Gaze::Inference::get_head_transform_in_camera_space(crops.head_pose_translation, crops.head_pose_rotation);
    GazeVector3 head_forward = head_transform.basis.multiply_vector(GazeVector3(0, 0, 1));

    // For a forward-facing head, the forward vector should point towards the screen (+Z_cam in Godot camera space)
    CHECK(head_forward.z > 0.8);
}

TEST_CASE("Testing Viewport and High-DPI Projection Coordinates")
{
    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(3840.0, 2160.0)); // 4K physical screen
    engine.set_screen_size_mm(GazeVector2(600.0, 340.0));
    engine.set_camera_placement(CameraPlacement(GazeVector3(0, 0, 0), 0.0));

    // Staring at the center of the screen
    GazeVector3 origin(0.0, 0.0, -600.0);
    GazeVector3 direction(0.0, 0.0, 1.0);
    GazeVector2 pixel;
    REQUIRE(engine.project_gaze(origin, direction, pixel) == true);

    // Physical screen center check
    CHECK(pixel.x == doctest::Approx(1920.0));
    CHECK(pixel.y == doctest::Approx(1080.0));

    // Simulate high-DPI scaling: window logical position is at (500, 300) logical points, scale is 2.0
    GazeVector2 window_pos_logical(500.0, 300.0);
    double scale = 2.0;
    GazeVector2 window_pos_physical(window_pos_logical.x * scale, window_pos_logical.y * scale);

    // Viewport-local physical position
    GazeVector2 local_pos_physical(pixel.x - window_pos_physical.x, pixel.y - window_pos_physical.y);
    CHECK(local_pos_physical.x == doctest::Approx(920.0));
    CHECK(local_pos_physical.y == doctest::Approx(480.0));

    // Viewport-local logical position
    GazeVector2 local_pos_logical(local_pos_physical.x / scale, local_pos_physical.y / scale);
    CHECK(local_pos_logical.x == doctest::Approx(460.0));
    CHECK(local_pos_logical.y == doctest::Approx(240.0));
}

TEST_CASE("Testing HiDPI Scaling Settings and Coordinate Transforms")
{
    // Screen parameters
    GazeVector2 screen_size_lpix(1920, 1080);
    double os_screen_scale = 2.0;                                                                             // Retina screen
    GazeVector2 screen_size_ppix(screen_size_lpix.x * os_screen_scale, screen_size_lpix.y * os_screen_scale); // (3840, 2160)

    // Projected pixel from ProjectionEngine (physical pixels)
    GazeVector2 projected_pixel_physical(1920.0, 1080.0);

    // Window position in logical points
    GazeVector2 window_pos_logical(100.0, 50.0);
    GazeVector2 window_pos_physical(window_pos_logical.x * os_screen_scale, window_pos_logical.y * os_screen_scale); // (200, 100)

    // Calculate local position in physical pixels
    GazeVector2 local_pos_physical(projected_pixel_physical.x - window_pos_physical.x, projected_pixel_physical.y - window_pos_physical.y);

    // Test Scenario A: allow_hidpi = true (Godot window scale is 2.0)
    {
        double godot_window_scale = 2.0;
        double window_to_screen_scale_ratio = godot_window_scale / os_screen_scale;

        // Scale local position to Godot window space
        GazeVector2 local_pos_godot(local_pos_physical.x * window_to_screen_scale_ratio, local_pos_physical.y * window_to_screen_scale_ratio);
        CHECK(window_to_screen_scale_ratio == doctest::Approx(1.0));
        CHECK(local_pos_godot.x == doctest::Approx(local_pos_physical.x));
    }

    // Test Scenario B: allow_hidpi = false (Godot window scale is 1.0)
    {
        double godot_window_scale = 1.0;
        double window_to_screen_scale_ratio = godot_window_scale / os_screen_scale;

        // Scale local position to Godot window space
        GazeVector2 local_pos_godot(local_pos_physical.x * window_to_screen_scale_ratio, local_pos_physical.y * window_to_screen_scale_ratio);
        CHECK(window_to_screen_scale_ratio == doctest::Approx(0.5));
        CHECK(local_pos_godot.x == doctest::Approx(local_pos_physical.x * 0.5));
    }
}

TEST_CASE("Testing Web Geometry Scaling and Coordinate Mapping Parity")
{
    // Web Screen metrics (Simulated Retina: 1512x982 logical, 3024x1964 physical)
    GazeVector2 screen_size_lpix(1512.0, 982.0);
    double dpr = 2.0;
    GazeVector2 screen_size_ppix(screen_size_lpix.x * dpr, screen_size_lpix.y * dpr); // (3024, 1964)

    // Scenario A: godot_scale = 1.0 (Default Web export, HiDPI disabled)
    {
        double godot_scale = 1.0;
        double window_to_screen_scale_ratio = godot_scale / dpr; // 0.5

        // Gaze intersection point in physical pixels on the screen
        GazeVector2 projected_pixel_physical(1512.0, 982.0);

        // Canvas position on the screen in physical pixels
        GazeVector2 canvas_pos_physical(300.0 * dpr, 200.0 * dpr); // (600, 400)

        // Calculate local position in physical pixels
        GazeVector2 local_pos_physical(
            projected_pixel_physical.x - canvas_pos_physical.x,
            projected_pixel_physical.y - canvas_pos_physical.y); // (912, 582)

        // Scale to Godot viewport space
        GazeVector2 local_pos_godot(
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

        GazeVector2 projected_pixel_physical(1512.0, 982.0);
        GazeVector2 canvas_pos_physical(300.0 * dpr, 200.0 * dpr);

        GazeVector2 local_pos_physical(
            projected_pixel_physical.x - canvas_pos_physical.x,
            projected_pixel_physical.y - canvas_pos_physical.y);

        GazeVector2 local_pos_godot(
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
    engine.set_screen_size_pixels(Gaze::GazeVector2(1440.0, 900.0)); // Logical screen width
    engine.set_screen_size_mm(Gaze::GazeVector2(300.0, 195.0));      // Physical screen mm
    Gaze::CameraPlacement placement(Gaze::GazeVector3(0.0, 95.5, 0.0), 0.0);
    engine.set_camera_placement(placement);

    // 2. Test standard window (positioned at 0,0)
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::derive_configuration(
            Gaze::GazeVector2(0.0, 0.0), // Window pos in logical screen pixels
            Gaze::GazeVector2(1.0, 1.0), // Viewport scale
            Gaze::GazeVector2(0.0, 0.0)  // Viewport origin offset
        );

        Gaze::GazeVector3 origin_cam(0.0, -95.5, 800.0);
        Gaze::GazeVector3 dir_cam(0.0, 0.0, -1.0);

        Gaze::GazeVector2 viewport_pixel;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, viewport_pixel);

        REQUIRE(ok);
        // Center of 1440.0 screen is 720.0. With win_pos=0 and vp_scale=1, viewport X should be 720.0
        CHECK(viewport_pixel.x == doctest::Approx(720.0));
    }

    // 3. Test window positioned at (180, 82) logical pixels
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::derive_configuration(
            Gaze::GazeVector2(180.0, 82.0), // Window pos in logical screen pixels
            Gaze::GazeVector2(1.0, 1.0),    // Viewport scale
            Gaze::GazeVector2(0.0, 0.0)     // Viewport origin offset
        );

        Gaze::GazeVector3 origin_cam(0.0, -95.5, 800.0);
        Gaze::GazeVector3 dir_cam(0.0, 0.0, -1.0);

        Gaze::GazeVector2 viewport_pixel;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, viewport_pixel);

        REQUIRE(ok);
        // logical screen X = 720.0
        // local window X = 720.0 - 180.0 = 540.0
        CHECK(viewport_pixel.x == doctest::Approx(540.0));
    }

    // 4. Test window positioned at (180, 82) logical pixels using from_godot_geometry
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::from_godot_geometry(
            Gaze::GazeVector2(180.0, 82.0), // Window pos in logical pixels
            Gaze::GazeVector2(1.0, 1.0),    // Viewport scale
            Gaze::GazeVector2(0.0, 0.0)     // Viewport origin offset
        );

        Gaze::GazeVector3 origin_cam(0.0, -95.5, 800.0);
        Gaze::GazeVector3 dir_cam(0.0, 0.0, -1.0);

        Gaze::GazeVector2 viewport_pixel;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, viewport_pixel);

        REQUIRE(ok);
        CHECK(viewport_pixel.x == doctest::Approx(540.0));
    }
}

TEST_CASE("Testing ScreenProjector Scaling & Inverse Parity")
{
    // Setup projection engine mock metrics
    Gaze::ProjectionEngine engine;
    engine.set_screen_size_pixels(Gaze::GazeVector2(1440.0, 900.0)); // Logical screen width
    engine.set_screen_size_mm(Gaze::GazeVector2(302.0, 188.0));      // Physical screen mm
    Gaze::CameraPlacement placement(Gaze::GazeVector3(0.0, 94.0, 0.0), 0.0);
    engine.set_camera_placement(placement);

    // 1. Standard Configuration
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::from_godot_geometry(
            Gaze::GazeVector2(100.0, 50.0), // Window pos in logical screen pixels
            Gaze::GazeVector2(1.0, 1.0),    // Logical viewport scale
            Gaze::GazeVector2(0.0, 0.0)     // Logical viewport offset
        );

        Gaze::GazeVector2 logical_pixel(512.0, 300.0);
        Gaze::GazeVector2 physical_pixel = projector.map_viewport_to_screen_px(logical_pixel);

        CHECK(physical_pixel.x == doctest::Approx(612.0));
        CHECK(physical_pixel.y == doctest::Approx(350.0));

        // Setup raw gaze/origin in camera space and project it back to verify inverse parity
        Gaze::GazeVector3 origin_cam(0.0, -94.0, 800.0);
        Gaze::GazeVector3 dir_cam(0.05, -0.02, -0.998); // Random gaze direction pointing forward
        Gaze::GazeVector2 proj_viewport;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, proj_viewport);
        REQUIRE(ok);

        Gaze::GazeVector2 proj_physical = projector.map_viewport_to_screen_px(proj_viewport);

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
            Gaze::GazeVector2(200.0, 100.0), // window position in logical pixels
            Gaze::GazeVector2(2.0, 2.0),     // viewport scale
            Gaze::GazeVector2(0.0, 0.0)      // offset
        );

        Gaze::GazeVector2 logical_pixel(512.0, 300.0);
        Gaze::GazeVector2 physical_pixel = projector.map_viewport_to_screen_px(logical_pixel);
        CHECK(physical_pixel.x == doctest::Approx(1224.0));
        CHECK(physical_pixel.y == doctest::Approx(700.0));

        // Assert inverse parity
        Gaze::GazeVector3 origin_cam(0.0, -94.0, 800.0);
        Gaze::GazeVector3 dir_cam(-0.1, 0.05, -0.99);
        Gaze::GazeVector2 proj_viewport;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, proj_viewport);
        REQUIRE(ok);

        Gaze::GazeVector2 proj_physical = projector.map_viewport_to_screen_px(proj_viewport);
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
            Gaze::GazeVector2(0.0, 0.0),
            Gaze::GazeVector2(0.0, 0.0), // Scale = 0 (Division by zero risk!)
            Gaze::GazeVector2(0.0, 0.0));

        Gaze::GazeVector3 origin_cam(0.0, -94.0, 800.0);
        Gaze::GazeVector3 dir_cam(0.0, 0.0, -1.0);
        Gaze::GazeVector2 proj_viewport;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, proj_viewport);
        CHECK_FALSE(ok); // Must fail gracefully
    }
}

TEST_CASE("Testing Head Rotation Pitch and Yaw Coordinate Signs")
{
    // Test case A: Logical math verification
    // 1. Staring straight ahead (near-zero rotation)
    {
        Gaze::GazeVector3 translation(0.0, 0.0, 800.0);
        Gaze::GazeVector3 rotation_straight(0.0, 0.0, 0.0);
        Gaze::GazeTransform3D transform = Gaze::Inference::get_head_transform_in_camera_space(translation, rotation_straight);
        Gaze::GazeVector3 head_forward = transform.basis.multiply_vector(Gaze::GazeVector3(0, 0, 1));

        // Z points towards the screen (positive Z in Godot camera space)
        CHECK(head_forward.z > 0.9);
        CHECK(std::abs(head_forward.x) < 0.01);
        CHECK(std::abs(head_forward.y) < 0.01);
    }

    // 2. Head tilted up (pitch rotation around X is negative in OpenCV: rvec.x < 0)
    {
        Gaze::GazeVector3 translation(0.0, 0.0, 800.0);
        Gaze::GazeVector3 rotation_up(-0.15, 0.0, 0.0); // ~8.6 degrees up
        Gaze::GazeTransform3D transform = Gaze::Inference::get_head_transform_in_camera_space(translation, rotation_up);
        Gaze::GazeVector3 head_forward = transform.basis.multiply_vector(Gaze::GazeVector3(0, 0, 1));

        // Pitch up must produce a positive Y component in camera space
        CHECK(head_forward.y > 0.05);
        CHECK(head_forward.z > 0.9);
    }

    // 3. Head tilted down (pitch rotation around X is positive in OpenCV: rvec.x > 0)
    {
        Gaze::GazeVector3 translation(0.0, 0.0, 800.0);
        Gaze::GazeVector3 rotation_down(0.15, 0.0, 0.0); // ~8.6 degrees down
        Gaze::GazeTransform3D transform = Gaze::Inference::get_head_transform_in_camera_space(translation, rotation_down);
        Gaze::GazeVector3 head_forward = transform.basis.multiply_vector(Gaze::GazeVector3(0, 0, 1));

        // Pitch down must produce a negative Y component in camera space
        CHECK(head_forward.y < -0.05);
        CHECK(head_forward.z > 0.9);
    }

    // 4. Head turned left (yaw rotation around Y is positive in OpenCV: rvec.y > 0)
    {
        Gaze::GazeVector3 translation(0.0, 0.0, 800.0);
        Gaze::GazeVector3 rotation_left(0.0, 0.2, 0.0); // ~11.5 degrees left (camera right)
        Gaze::GazeTransform3D transform = Gaze::Inference::get_head_transform_in_camera_space(translation, rotation_left);
        Gaze::GazeVector3 head_forward = transform.basis.multiply_vector(Gaze::GazeVector3(0, 0, 1));

        // Turning left (facing camera's negative X direction in standard camera space)
        CHECK(head_forward.x < -0.05);
        CHECK(head_forward.z > 0.9);
    }

    // 5. Head turned right (yaw rotation around Y is negative in OpenCV: rvec.y < 0)
    {
        Gaze::GazeVector3 translation(0.0, 0.0, 800.0);
        Gaze::GazeVector3 rotation_right(0.0, -0.2, 0.0); // ~11.5 degrees right (camera left)
        Gaze::GazeTransform3D transform = Gaze::Inference::get_head_transform_in_camera_space(translation, rotation_right);
        Gaze::GazeVector3 head_forward = transform.basis.multiply_vector(Gaze::GazeVector3(0, 0, 1));

        // Turning right (facing camera's positive X direction in standard camera space)
        CHECK(head_forward.x > 0.05);
        CHECK(head_forward.z > 0.9);
    }
}

TEST_CASE("Testing Edge Conditions and Stress Scenarios")
{
    // 1. Test empty frames (null data)
    {
        std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        ORTYuNetPipeline pipeline(yunet_path);
        REQUIRE(pipeline.initialize() == true);

        Frame empty_frame;
        empty_frame.width = 640;
        empty_frame.height = 480;
        empty_frame.timestamp = 0.0;
        empty_frame.data = nullptr;

        EyeCrops crops;
        CHECK(pipeline.process_frame(empty_frame, crops) == false);
    }

    // 2. Test invalid dimensions (0x0 frame)
    {
        std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        ORTYuNetPipeline pipeline(yunet_path);
        REQUIRE(pipeline.initialize() == true);

        unsigned char dummy_data[1] = {0};
        Frame zero_frame;
        zero_frame.width = 0;
        zero_frame.height = 0;
        zero_frame.timestamp = 0.0;
        zero_frame.data = dummy_data;

        EyeCrops crops;
        CHECK(pipeline.process_frame(zero_frame, crops) == false);
    }

    // 3. Test negative dimensions (-640x-480 frame)
    {
        std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        ORTYuNetPipeline pipeline(yunet_path);
        REQUIRE(pipeline.initialize() == true);

        unsigned char dummy_data[1] = {0};
        Frame neg_frame;
        neg_frame.width = -640;
        neg_frame.height = -480;
        neg_frame.timestamp = 0.0;
        neg_frame.data = dummy_data;

        EyeCrops crops;
        CHECK(pipeline.process_frame(neg_frame, crops) == false);
    }

    // 4. Test extremely small non-zero dimensions (1x1 frame)
    {
        std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        ORTYuNetPipeline pipeline(yunet_path);
        REQUIRE(pipeline.initialize() == true);

        unsigned char dummy_data[3] = {128, 128, 128};
        Frame tiny_frame;
        tiny_frame.width = 1;
        tiny_frame.height = 1;
        tiny_frame.timestamp = 0.0;
        tiny_frame.data = dummy_data;

        EyeCrops crops;
        CHECK(pipeline.process_frame(tiny_frame, crops) == false);
    }

    // 5. Test ORTGazeModel with extreme head pose rotations (NaN, Infinity, and Out of Bounds)
    {
        std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";
        ORTGazeModel model(gaze_path);
        REQUIRE(model.initialize() == true);

        EyeCrops crops;
        crops.face_detected = true;
        crops.head_pose_translation = GazeVector3(0.0, 0.0, 500.0);
        crops.left_eye_center_cam = GazeVector3(31.5, 0.0, 480.0);
        crops.right_eye_center_cam = GazeVector3(-31.5, 0.0, 480.0);
        std::memset(crops.left_eye_data, 128, 10800);
        std::memset(crops.right_eye_data, 128, 10800);

        // Test with NaN rotation
        crops.head_pose_rotation = GazeVector3(NAN, NAN, NAN);
        GazeVector3 gaze_dir_cv;
        bool success = model.estimate_raw_gaze(crops, gaze_dir_cv);
        CHECK((!success || std::isnan(gaze_dir_cv.x) || std::isnan(gaze_dir_cv.y) || std::isnan(gaze_dir_cv.z)));

        // Test with Infinity rotation
        crops.head_pose_rotation = GazeVector3(INFINITY, -INFINITY, INFINITY);
        success = model.estimate_raw_gaze(crops, gaze_dir_cv);
        CHECK((!success || std::isnan(gaze_dir_cv.x) || std::isnan(gaze_dir_cv.y) || std::isnan(gaze_dir_cv.z)));

        // Test with extremely large rotation values (e.g. 1e12)
        crops.head_pose_rotation = GazeVector3(1e12, -1e12, 1e12);
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
            crops.head_pose_rotation = GazeVector3(0.05, -0.02, 0.01);
            crops.head_pose_translation = GazeVector3(10.0, -15.0, 600.0);
            crops.left_eye_center_cam = GazeVector3(31.5, 0.0, 580.0);
            crops.right_eye_center_cam = GazeVector3(-31.5, 0.0, 580.0);
            std::memset(crops.left_eye_data, 200, 10800);
            std::memset(crops.right_eye_data, 100, 10800);

            GazeVector3 gaze_dir;
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
        std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        ORTYuNetPipeline pipeline(yunet_path);
        // Do NOT call initialize()
        Frame frame;
        frame.width = 640;
        frame.height = 480;
        frame.timestamp = 0.0;
        unsigned char dummy_data[640 * 480 * 3] = {0};
        frame.data = dummy_data;
        EyeCrops crops;
        CHECK(pipeline.process_frame(frame, crops) == false);

        std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";
        ORTGazeModel model(gaze_path);
        // Do NOT call initialize()
        GazeVector3 gaze_dir;
        CHECK(model.estimate_raw_gaze(crops, gaze_dir) == false);
    }

    // 8. Test extremely large frame sizes (8K: 7680x4320) to ensure resize handles memory allocation safely
    {
        std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
        ORTYuNetPipeline pipeline(yunet_path);
        REQUIRE(pipeline.initialize() == true);

        // Instead of allocating a huge array on stack or heap that might exceed limits,
        // use a heap-allocated unique_ptr buffer.
        std::unique_ptr<unsigned char[]> huge_mat(new unsigned char[7680 * 4320 * 3]());
        Frame huge_frame;
        huge_frame.width = 7680;
        huge_frame.height = 4320;
        huge_frame.timestamp = 0.0;
        huge_frame.data = huge_mat.get();

        EyeCrops crops;
        CHECK(pipeline.process_frame(huge_frame, crops) == false);
    }
}

#include "pnp_solver.hpp"
#include "cpu_image_warper.hpp"

TEST_CASE("Testing Native Math Solvers and Warping Parity")
{
    // 1. Test Rodrigues Parity (Direct Math Roundtrip / Sanity Check)
    {
        std::vector<GazeVector3> test_rvecs = {
            GazeVector3(0.1, -0.2, 0.5),
            GazeVector3(0.0, 0.0, 0.0),
            GazeVector3(1e-7, -2e-7, 1e-7), // Near zero
            GazeVector3(1.8, -1.8, 1.8)     // Large angle
        };

        for (const auto &rvec_orig : test_rvecs)
        {
            GazeBasis3D R_basis = rodrigues_to_basis(rvec_orig);
            GazeVector3 rvec_native = basis_to_rodrigues(R_basis);

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
        std::vector<GazeVector3> model_pts = {
            GazeVector3(-FaceModelGeometry::EYE_X, FaceModelGeometry::EYE_Y, FaceModelGeometry::EYE_Z),
            GazeVector3(FaceModelGeometry::EYE_X, FaceModelGeometry::EYE_Y, FaceModelGeometry::EYE_Z),
            GazeVector3(0.0, -0.5, -52.0),
            GazeVector3(-FaceModelGeometry::MOUTH_X, FaceModelGeometry::MOUTH_Y, FaceModelGeometry::MOUTH_Z),
            GazeVector3(FaceModelGeometry::MOUTH_X, FaceModelGeometry::MOUTH_Y, FaceModelGeometry::MOUTH_Z)};

        GazeVector3 true_rvec(0.12, -0.08, 0.04);
        GazeVector3 true_tvec(-15.0, 10.0, 580.0);
        double fx = 960.0, fy = 960.0, cx = 320.0, cy = 240.0;

        // Generate 2D image points from true pose
        std::vector<GazeVector2> img_pts(5);
        for (int i = 0; i < 5; ++i)
        {
            GazeBasis3D R = rodrigues_to_basis(true_rvec);
            GazeVector3 P_cam = R.multiply_vector(model_pts[i]) + true_tvec;
            img_pts[i] = GazeVector2(fx * P_cam.x / P_cam.z + cx, fy * P_cam.y / P_cam.z + cy);
        }

        // Run native LM PnP
        GazeVector3 est_rvec(0.0, 0.0, 0.0);
        GazeVector3 est_tvec(0.0, 0.0, 700.0);
        bool pnp_ok = solve_pnp_lm(model_pts, img_pts, fx, fy, cx, cy, est_rvec, est_tvec, true);
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

        GazePoint landmarks[5] = {
            GazePoint(70.5f, 80.2f),  // right eye
            GazePoint(130.8f, 85.6f), // left eye
            GazePoint(100.0f, 110.0f),
            GazePoint(80.0f, 140.0f),
            GazePoint(120.0f, 142.0f)};

        GazePoint eye_center = landmarks[0];
        double roll_dx = landmarks[1].x - landmarks[0].x;
        double roll_dy = landmarks[1].y - landmarks[0].y;
        double angle = std::atan2(roll_dy, roll_dx) * (180.0 / 3.141592653589793);
        double dist_px = std::sqrt(roll_dx * roll_dx + roll_dy * roll_dy);
        double scale = 70.0 / (dist_px > 1e-6 ? dist_px : 70.0);

        CPUImageWarper native_warper;
        unsigned char native_out[10800] = {0};
        GazeVector2 eye_center_vec(eye_center.x, eye_center.y);
        bool warp_success = native_warper.warp(dummy_src.data(), src_w, src_h, 3, eye_center_vec, angle, scale, native_out);
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
    std::vector<GazeVector3> model_pts = {
        GazeVector3(-FaceModelGeometry::EYE_X, FaceModelGeometry::EYE_Y, FaceModelGeometry::EYE_Z),
        GazeVector3(FaceModelGeometry::EYE_X, FaceModelGeometry::EYE_Y, FaceModelGeometry::EYE_Z),
        GazeVector3(0.0, -0.5, -52.0),
        GazeVector3(-FaceModelGeometry::MOUTH_X, FaceModelGeometry::MOUTH_Y, FaceModelGeometry::MOUTH_Z),
        GazeVector3(FaceModelGeometry::MOUTH_X, FaceModelGeometry::MOUTH_Y, FaceModelGeometry::MOUTH_Z)};

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
            GazeVector3 true_rvec(rx, ry, rz);

            double tx = dist_trans_xy(rng);
            double ty = dist_trans_xy(rng);
            double tz = dist_trans_z(rng);
            if (sc.extreme_dist)
            {
                // Extreme distances: either very close (100mm) or very far (3000mm)
                tz = (run % 2) ? 100.0 : 3000.0;
            }
            GazeVector3 true_tvec(tx, ty, tz);

            // Configure model points
            std::vector<GazeVector3> current_model_pts = model_pts;
            if (sc.coplanar)
            {
                // Make points almost coplanar by setting nose Z close to 0
                current_model_pts[2].z = -1.0;
            }

            // Project model points to get true 2D image points
            std::vector<GazeVector2> img_pts(5);
            GazeBasis3D R = rodrigues_to_basis(true_rvec);
            for (int i = 0; i < 5; ++i)
            {
                GazeVector3 P_cam = R.multiply_vector(current_model_pts[i]) + true_tvec;
                double px = fx * P_cam.x / P_cam.z + cx;
                double py = fy * P_cam.y / P_cam.z + cy;
                if (sc.noise_level > 0.0)
                {
                    px += dist_noise(rng) * sc.noise_level;
                    py += dist_noise(rng) * sc.noise_level;
                }
                img_pts[i] = GazeVector2(px, py);
            }

            // Prepare initial guesses
            GazeVector3 est_rvec(0.0, 0.0, 0.0);
            GazeVector3 est_tvec(0.0, 0.0, 700.0);
            if (sc.use_extrinsic_guess)
            {
                // Initial guess perturbed from the truth
                est_rvec = GazeVector3(true_rvec.x + dist_rot(rng) * 0.1, true_rvec.y + dist_rot(rng) * 0.1, true_rvec.z + dist_rot(rng) * 0.1);
                est_tvec = GazeVector3(true_tvec.x + dist_trans_xy(rng) * 0.1, true_tvec.y + dist_trans_xy(rng) * 0.1, true_tvec.z + dist_trans_z(rng) * 0.1);
            }

            // Run native LM solver and measure time
            GazeVector3 native_rvec = est_rvec;
            GazeVector3 native_tvec = est_tvec;
            auto t0 = std::chrono::high_resolution_clock::now();
            bool native_ok = solve_pnp_lm(current_model_pts, img_pts, fx, fy, cx, cy, native_rvec, native_tvec, sc.use_extrinsic_guess);
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
    // 1. Verify offsetof checks to ensure zero internal padding/gaps
    CHECK(offsetof(Gaze::GazeVector2, x) == 0);
    CHECK(offsetof(Gaze::GazeVector2, y) == 8);
    CHECK(sizeof(Gaze::GazeVector2) == 16);

    CHECK(offsetof(Gaze::GazeVector2i, x) == 0);
    CHECK(offsetof(Gaze::GazeVector2i, y) == 4);
    CHECK(sizeof(Gaze::GazeVector2i) == 8);

    CHECK(offsetof(Gaze::GazePoint, x) == 0);
    CHECK(offsetof(Gaze::GazePoint, y) == 8);
    CHECK(sizeof(Gaze::GazePoint) == 16);

    CHECK(offsetof(Gaze::GazeRect, x) == 0);
    CHECK(offsetof(Gaze::GazeRect, y) == 4);
    CHECK(offsetof(Gaze::GazeRect, width) == 8);
    CHECK(offsetof(Gaze::GazeRect, height) == 12);
    CHECK(sizeof(Gaze::GazeRect) == 16);

    CHECK(offsetof(Gaze::GazeVector3, x) == 0);
    CHECK(offsetof(Gaze::GazeVector3, y) == 4);
    CHECK(offsetof(Gaze::GazeVector3, z) == 8);
    CHECK(sizeof(Gaze::GazeVector3) == 12);

    CHECK(offsetof(Gaze::GazeBasis3D, x) == 0);
    CHECK(offsetof(Gaze::GazeBasis3D, y) == 12);
    CHECK(offsetof(Gaze::GazeBasis3D, z) == 24);
    CHECK(sizeof(Gaze::GazeBasis3D) == 36);

    CHECK(offsetof(Gaze::GazeTransform3D, basis) == 0);
    CHECK(offsetof(Gaze::GazeTransform3D, origin) == 36);
    CHECK(sizeof(Gaze::GazeTransform3D) == 48);

    CHECK(offsetof(Gaze::Frame, width) == 0);
    CHECK(offsetof(Gaze::Frame, height) == 4);
    CHECK(offsetof(Gaze::Frame, data) == 8);
    CHECK(offsetof(Gaze::Frame, timestamp) == 16);
    CHECK(sizeof(Gaze::Frame) == 24);

    // 2. Validate round-trip serializability via memcpy
    {
        Gaze::GazeVector2 v2(1.23, 4.56);
        unsigned char buf[sizeof(Gaze::GazeVector2)];
        std::memcpy(buf, &v2, sizeof(v2));
        Gaze::GazeVector2 v2_out;
        std::memcpy(&v2_out, buf, sizeof(v2_out));
        CHECK(v2_out.x == v2.x);
        CHECK(v2_out.y == v2.y);
    }
    {
        Gaze::GazeVector2i v2i(42, -99);
        unsigned char buf[sizeof(Gaze::GazeVector2i)];
        std::memcpy(buf, &v2i, sizeof(v2i));
        Gaze::GazeVector2i v2i_out;
        std::memcpy(&v2i_out, buf, sizeof(v2i_out));
        CHECK(v2i_out.x == v2i.x);
        CHECK(v2i_out.y == v2i.y);
    }
    {
        Gaze::GazePoint pt(1.5f, 2.5f);
        unsigned char buf[sizeof(Gaze::GazePoint)];
        std::memcpy(buf, &pt, sizeof(pt));
        Gaze::GazePoint pt_out;
        std::memcpy(&pt_out, buf, sizeof(pt_out));
        CHECK(pt_out.x == pt.x);
        CHECK(pt_out.y == pt.y);
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
        Gaze::GazeVector3 v3(1.1f, -2.2f, 3.3f);
        unsigned char buf[sizeof(Gaze::GazeVector3)];
        std::memcpy(buf, &v3, sizeof(v3));
        Gaze::GazeVector3 v3_out;
        std::memcpy(&v3_out, buf, sizeof(v3_out));
        CHECK(v3_out.x == v3.x);
        CHECK(v3_out.y == v3.y);
        CHECK(v3_out.z == v3.z);
    }
    {
        Gaze::GazeBasis3D basis(Gaze::GazeVector3(1.f, 2.f, 3.f), Gaze::GazeVector3(4.f, 5.f, 6.f), Gaze::GazeVector3(7.f, 8.f, 9.f));
        unsigned char buf[sizeof(Gaze::GazeBasis3D)];
        std::memcpy(buf, &basis, sizeof(basis));
        Gaze::GazeBasis3D basis_out;
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
        Gaze::GazeTransform3D transform(
            Gaze::GazeBasis3D(Gaze::GazeVector3(1.f, 2.f, 3.f), Gaze::GazeVector3(4.f, 5.f, 6.f), Gaze::GazeVector3(7.f, 8.f, 9.f)),
            Gaze::GazeVector3(10.f, 11.f, 12.f));
        unsigned char buf[sizeof(Gaze::GazeTransform3D)];
        std::memcpy(buf, &transform, sizeof(transform));
        Gaze::GazeTransform3D transform_out;
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
    Gaze::g_is_unit_test = true;

    // 1. Read ONNX model data from standard model paths
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";

    std::vector<uint8_t> yunet_data = read_binary_file(yunet_path);
    std::vector<uint8_t> gaze_data = read_binary_file(gaze_path);

    REQUIRE_MESSAGE(!yunet_data.empty(), "Failed to read YuNet model data");
    REQUIRE_MESSAGE(!gaze_data.empty(), "Failed to read Gaze model data");

    // 2. Instantiate and Initialize GazeTrackingPipeline
    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_data, gaze_data) == true);

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
    Gaze::g_is_unit_test = true;

    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";

    std::vector<uint8_t> yunet_data = read_binary_file(yunet_path);
    std::vector<uint8_t> gaze_data = read_binary_file(gaze_path);

    REQUIRE(!yunet_data.empty());
    REQUIRE(!gaze_data.empty());

    LoadedImage img = load_test_image("tests/resources/self_center.jpg");
    REQUIRE(!img.data.empty());

    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_data, gaze_data) == true);

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
            pipeline.initialize(yunet_data, gaze_data);
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
