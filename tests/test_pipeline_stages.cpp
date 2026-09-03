/**
 * @file test_pipeline_stages.cpp
 * @brief Stage-by-stage golden verification tests for GazeTrackingPipeline
 */

#include "doctest.h"
#include "gaze_tracking_pipeline.hpp"
#include "projection_engine.hpp"
#include "camera_placement.hpp"
#include "test_utils.hpp"
#include <iostream>
#include <cmath>

using namespace Gaze;
using GazeTest::file_exists;
using GazeTest::load_test_image;
using GazeTest::TestImage;

static std::string get_model_path(const std::string& rel) {
    if (file_exists(rel)) return rel;
    std::string p1 = "../" + rel;
    if (file_exists(p1)) return p1;
    std::string p2 = "../../" + rel;
    if (file_exists(p2)) return p2;
    return rel;
}

TEST_CASE("Pipeline Stage-by-Stage Verification on Canonical Center Fixture") {
    std::string yunet_path = get_model_path("project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort");
    std::string gaze_path = get_model_path("project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort");
    std::string eye_state_path = get_model_path("project/addons/godot-gaze/models/open_closed_eye.ort");
    std::string lm_path = get_model_path("project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort");

    REQUIRE(file_exists(yunet_path));
    REQUIRE(file_exists(gaze_path));
    REQUIRE(file_exists(eye_state_path));
    REQUIRE(file_exists(lm_path));

    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_path, gaze_path, eye_state_path, lm_path));

    TestImage img = load_test_image("tests/resources/self_center.jpg");
    REQUIRE(img.valid());

    GazeFrameData data;
    data.camera_raw_bgr = img.data;
    data.camera_width = img.width;
    data.camera_height = img.height;
    data.timestamp = 1.0;
    data.roll_hint_rad = 0.0f;

    // Execute synchronous pipeline
    pipeline.process_frame_synchronous(&data);

    // Stage 1: Roll Un-Rotation
    CHECK(data.roll_hint_rad == 0.0f);

    // Stage 2: YuNet Face Detection
    CHECK(data.face_detected == true);
    CHECK(data.face_score >= 0.80f);
    CHECK(data.face_bbox.width > 150.0f);
    CHECK(data.face_bbox.height > 150.0f);
    CHECK(data.face_bbox.x > 100.0f);
    CHECK(data.face_bbox.y > 50.0f);

    // Stage 3: 35 Facial Landmarks
    CHECK(data.has_landmarks_2d == true);
    CHECK(data.internal_landmarks_working_px.size() == 35);
    // Landmarks inside bounding box
    for (const auto& lm : data.internal_landmarks_working_px) {
        CHECK(lm.x >= data.face_bbox.x - 20.0f);
        CHECK(lm.x <= data.face_bbox.x + data.face_bbox.width + 20.0f);
        CHECK(lm.y >= data.face_bbox.y - 20.0f);
        CHECK(lm.y <= data.face_bbox.y + data.face_bbox.height + 20.0f);
    }

    // Stage 4: SQPnP 3D Head Pose
    CHECK(data.head_translation.z < -300.0); // Facing camera ~350mm away in Godot camera space
    CHECK(data.head_translation.z > -500.0);
    CHECK(std::abs(data.head_rotation.z) < 10.0 * DEG_TO_RAD); // Upright roll < 10 deg

    // Stage 5: Dense Eye Crops
    CHECK(data.eye_crops.face_detected == true);
    CHECK(data.eye_box_sz >= 24.0f);
    // Non-trivial pixel buffer
    uint32_t l_sum = 0, r_sum = 0;
    for (size_t i = 0; i < EYE_CROP_BYTES; ++i) {
        l_sum += data.eye_crops.left_eye_data[i];
        r_sum += data.eye_crops.right_eye_data[i];
    }
    CHECK(l_sum > 10000);
    CHECK(r_sum > 10000);

    // Stage 7 & 8: GazeNet 3D Vector & Godot Camera Unroll
    CHECK(data.gaze_success == true);
    CHECK(data.gaze_direction.length() == doctest::Approx(1.0).epsilon(1e-3));
    // Gaze pointing forward from user towards screen (+Z in Godot Camera space)
    CHECK(data.gaze_direction.z > 0.5);

    // Stage 9: Screen Projection Integration
    ProjectionEngine proj;
    proj.set_screen_size_pixels(GodotDisplayVector2(1440, 900));
    proj.set_screen_size_mm(SpacedVector2<Space::GodotDisplayMm>(304.1, 212.4));
    proj.set_camera_placement(CameraPlacement(GodotCameraVector3(0, 106.2, 0), 0.0));

    GodotDisplayVector2 screen_px;
    bool proj_ok = proj.project_gaze(data.gaze_origin, data.gaze_direction, screen_px);
    CHECK(proj_ok == true);
    CHECK(screen_px.x >= 0.0);
    CHECK(screen_px.x <= 1440.0);
    CHECK(screen_px.y >= 0.0);
    CHECK(screen_px.y <= 900.0);
}

TEST_CASE("Pipeline Stage-by-Stage Verification on Wink Fixtures with Signal Separation") {
    std::string yunet_path = get_model_path("project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort");
    std::string gaze_path = get_model_path("project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort");
    std::string eye_state_path = get_model_path("project/addons/godot-gaze/models/open_closed_eye.ort");
    std::string lm_path = get_model_path("project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort");

    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_path, gaze_path, eye_state_path, lm_path));

    // 1. Both Open
    {
        pipeline.reset_tracker();
        TestImage img = load_test_image("tests/resources/eyes_both_open.jpg");
        REQUIRE(img.valid());
        GazeFrameData data;
        data.camera_raw_bgr = img.data;
        data.camera_width = img.width;
        data.camera_height = img.height;
        pipeline.process_frame_synchronous(&data);

        CHECK(data.face_detected == true);
        CHECK(data.left_eye_openness >= 0.70f);
        CHECK(data.right_eye_openness >= 0.70f);
    }

    // 2. Both Wink / Closed
    {
        pipeline.reset_tracker();
        TestImage img = load_test_image("tests/resources/eyes_both_wink.jpg");
        REQUIRE(img.valid());
        GazeFrameData data;
        data.camera_raw_bgr = img.data;
        data.camera_width = img.width;
        data.camera_height = img.height;
        pipeline.process_frame_synchronous(&data);

        CHECK(data.face_detected == true);
        CHECK(data.left_eye_openness <= 0.20f);
        CHECK(data.right_eye_openness <= 0.20f);
    }

    // 3. Left Wink (Anatomical Left Closed, Anatomical Right Open)
    {
        pipeline.reset_tracker();
        TestImage img = load_test_image("tests/resources/eyes_anatomical_left_wink.jpg");
        REQUIRE(img.valid());
        GazeFrameData data;
        data.camera_raw_bgr = img.data;
        data.camera_width = img.width;
        data.camera_height = img.height;
        pipeline.process_frame_synchronous(&data);

        CHECK(data.face_detected == true);
        CHECK(data.left_eye_openness <= 0.20f);
        CHECK(data.right_eye_openness >= 0.70f);
        // Signal Separation Delta >= 0.50
        CHECK((data.right_eye_openness - data.left_eye_openness) >= 0.50f);
    }

    // 4. Right Wink (Anatomical Right Closed, Anatomical Left Open)
    {
        pipeline.reset_tracker();
        TestImage img = load_test_image("tests/resources/eyes_anatomical_right_wink.jpg");
        REQUIRE(img.valid());
        GazeFrameData data;
        data.camera_raw_bgr = img.data;
        data.camera_width = img.width;
        data.camera_height = img.height;
        data.auto_roll_enabled = true;
        data.timestamp = 1.0;
        pipeline.process_frame_synchronous(&data);
        data.timestamp = 1.033;
        pipeline.process_frame_synchronous(&data);

        CHECK(data.face_detected == true);
        CHECK(data.right_eye_openness <= 0.35f);
        CHECK(data.left_eye_openness >= 0.70f);
        // Signal Separation Delta >= 0.50
        CHECK((data.left_eye_openness - data.right_eye_openness) >= 0.50f);
    }
}

TEST_CASE("Pipeline Obscured Face Graceful Handling") {
    std::string yunet_path = get_model_path("project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort");
    std::string gaze_path = get_model_path("project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort");
    std::string eye_state_path = get_model_path("project/addons/godot-gaze/models/open_closed_eye.ort");
    std::string lm_path = get_model_path("project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort");

    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_path, gaze_path, eye_state_path, lm_path));

    TestImage img = load_test_image("tests/resources/eyes_anatomical_left_obscured.jpg");
    REQUIRE(img.valid());

    GazeFrameData data;
    data.camera_raw_bgr = img.data;
    data.camera_width = img.width;
    data.camera_height = img.height;
    pipeline.process_frame_synchronous(&data);

    // Stage 2 detection fails gracefully
    CHECK(data.face_detected == false);
    CHECK(data.gaze_success == false);
}

TEST_CASE("Pipeline Stage 8: Eye Centers Invariant Under Head Roll") {
    std::string yunet_path = get_model_path("project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort");
    std::string gaze_path = get_model_path("project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort");
    std::string eye_state_path = get_model_path("project/addons/godot-gaze/models/open_closed_eye.ort");
    std::string lm_path = get_model_path("project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort");

    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_path, gaze_path, eye_state_path, lm_path));

    const std::vector<std::pair<std::string, float>> roll_fixtures = {
        {"tests/resources/self_roll_left.jpg", 0.785f},
        {"tests/resources/self_roll_right.jpg", -0.785f},
        {"tests/resources/self_yaw_left_roll_left.jpg", 0.785f}
    };

    for (const auto& fix : roll_fixtures) {
        pipeline.reset_tracker();
        TestImage img = load_test_image(fix.first);
        REQUIRE(img.valid());

        GazeFrameData data;
        data.camera_raw_bgr = img.data;
        data.camera_width = img.width;
        data.camera_height = img.height;
        data.roll_hint_rad = fix.second;

        pipeline.process_frame_synchronous(&data);

        REQUIRE(data.face_detected == true);
        REQUIRE(data.gaze_success == true);

        // Invariant: Unrolled eye midpoint must match unrolled gaze origin in canonical GodotCamera space
        GodotCameraVector3 eye_mid = (data.eye_crops.left_eye_center_cam + data.eye_crops.right_eye_center_cam) * 0.5;
        CHECK(data.gaze_origin.x == doctest::Approx(eye_mid.x).epsilon(1e-3));
        CHECK(data.gaze_origin.y == doctest::Approx(eye_mid.y).epsilon(1e-3));
        CHECK(data.gaze_origin.z == doctest::Approx(eye_mid.z).epsilon(1e-3));
    }
}

TEST_CASE("Pipeline Auto-Roll Tracking with Sigmoidal Dropout Hold") {
    std::string yunet_path = get_model_path("project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort");
    std::string gaze_path = get_model_path("project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort");
    std::string eye_state_path = get_model_path("project/addons/godot-gaze/models/open_closed_eye.ort");
    std::string lm_path = get_model_path("project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort");

    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_path, gaze_path, eye_state_path, lm_path));

    TestImage roll_img = load_test_image("tests/resources/self_roll_left.jpg");
    REQUIRE(roll_img.valid());

    TestImage obs_img = load_test_image("tests/resources/eyes_anatomical_left_obscured.jpg");
    REQUIRE(obs_img.valid());

    // 1. Process rolled frame sequence with initial roll hint = 0.785 rad (~45 deg)
    double t = 1.0;
    for (int i = 0; i < 5; ++i) {
        GazeFrameData d;
        d.camera_raw_bgr = roll_img.data;
        d.camera_width = roll_img.width;
        d.camera_height = roll_img.height;
        d.timestamp = t;
        d.auto_roll_enabled = true;
        d.roll_hint_rad = 0.785f;
        pipeline.process_frame_synchronous(&d);
        REQUIRE(d.face_detected == true);
        t += (1.0 / 60.0);
    }

    // 2. Feed an obscured frame at t = +16.6ms into dropout
    GazeFrameData d2;
    d2.camera_raw_bgr = obs_img.data;
    d2.camera_width = obs_img.width;
    d2.camera_height = obs_img.height;
    d2.timestamp = t;
    d2.auto_roll_enabled = true;

    pipeline.process_frame_synchronous(&d2);
    CHECK(d2.face_detected == false);
    // Roll hint applied in Stage 1 should be held by grace period (>= 0.70 rad)
    CHECK(d2.roll_hint_rad >= 0.70f);

    // 3. Reset pipeline tracker
    pipeline.reset_tracker();
    GazeFrameData d3;
    d3.camera_raw_bgr = obs_img.data;
    d3.camera_width = obs_img.width;
    d3.camera_height = obs_img.height;
    d3.timestamp = 2.0;
    d3.auto_roll_enabled = true;

    pipeline.process_frame_synchronous(&d3);
    CHECK(d3.roll_hint_rad == doctest::Approx(0.0f));
}
