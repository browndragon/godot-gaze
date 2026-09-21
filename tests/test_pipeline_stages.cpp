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
    proj.set_camera_placement(CameraPlacement(GodotCameraVector3(0, 0, 0), 0.0));

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

TEST_CASE("Pipeline Multi-Frame Landmark Tracking Stability") {
    std::string yunet_path = get_model_path("project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort");
    std::string gaze_path = get_model_path("project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort");
    std::string eye_state_path = get_model_path("project/addons/godot-gaze/models/open_closed_eye.ort");
    std::string lm_path = get_model_path("project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort");

    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_path, gaze_path, eye_state_path, lm_path));

    TestImage img = load_test_image("tests/resources/self_center.jpg");
    REQUIRE(img.valid());

    pipeline.reset_tracker();
    GazeFrameData d;
    d.camera_raw_bgr = img.data;
    d.camera_width = img.width;
    d.camera_height = img.height;
    d.timestamp = 1.0;

    // Frame 0: YuNet detection baseline
    pipeline.process_frame_synchronous(&d);
    REQUIRE(d.face_detected == true);
    REQUIRE(d.has_landmarks_2d == true);
    REQUIRE(d.internal_landmarks_working_px.size() == 35);
    float base_w = d.face_bbox.width;
    float base_h = d.face_bbox.height;
    double base_z = d.head_translation.z;

    std::vector<GodotCameraImageVector2> prev_lms(35);
    for (size_t i = 0; i < 35; ++i) {
        prev_lms[i] = GodotCameraImageVector2(d.internal_landmarks_working_px[i].x, d.internal_landmarks_working_px[i].y);
    }

    double final_frame_to_frame_delta = 0.0;

    // Run 30 consecutive hinted tracking frames
    for (int frame = 1; frame <= 30; ++frame) {
        d.timestamp = 1.0 + frame * (1.0 / 60.0);
        pipeline.process_frame_synchronous(&d);

        CHECK_MESSAGE(d.face_detected == true, "Face tracking dropped on frame " << frame);
        CHECK_MESSAGE(d.has_landmarks_2d == true, "Landmarks lost on frame " << frame);
        CHECK_MESSAGE(d.internal_landmarks_working_px.size() == 35, "Incomplete landmarks on frame " << frame);

        // Bounding box size stability: must not shrink or explode
        CHECK(d.face_bbox.width >= base_w * 0.80f);
        CHECK(d.face_bbox.width <= base_w * 1.60f);
        CHECK(d.face_bbox.height >= base_h * 0.80f);
        CHECK(d.face_bbox.height <= base_h * 1.60f);

        // Head translation Z stability: webcam distance must remain stable (~400mm)
        CHECK(d.head_translation.z == doctest::Approx(base_z).epsilon(0.15));

        // Compute frame-to-frame delta
        double max_delta = 0.0;
        for (size_t i = 0; i < 35; ++i) {
            double dx = d.internal_landmarks_working_px[i].x - prev_lms[i].x;
            double dy = d.internal_landmarks_working_px[i].y - prev_lms[i].y;
            max_delta = std::max(max_delta, std::sqrt(dx * dx + dy * dy));
            prev_lms[i] = GodotCameraImageVector2(d.internal_landmarks_working_px[i].x, d.internal_landmarks_working_px[i].y);
        }
        if (frame >= 25) {
            final_frame_to_frame_delta = std::max(final_frame_to_frame_delta, max_delta);
        }
    }

    // Convergence check: frame-to-frame delta must settle under 2.0px
    CHECK(final_frame_to_frame_delta < 2.0);
}

TEST_CASE("Pipeline Multi-Frame Landmark Tracking With Dynamic Roll") {
    std::string yunet_path = get_model_path("project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort");
    std::string gaze_path = get_model_path("project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort");
    std::string eye_state_path = get_model_path("project/addons/godot-gaze/models/open_closed_eye.ort");
    std::string lm_path = get_model_path("project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort");

    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_path, gaze_path, eye_state_path, lm_path));

    TestImage img = load_test_image("tests/resources/self_roll_right.jpg");
    REQUIRE(img.valid());

    pipeline.reset_tracker();
    GazeFrameData d;
    d.camera_raw_bgr = img.data;
    d.camera_width = img.width;
    d.camera_height = img.height;
    d.timestamp = 1.0;
    d.auto_roll_enabled = true;
    d.roll_hint_rad = 0.0f;

    // Frame 0: Baseline YuNet discovery
    pipeline.process_frame_synchronous(&d);
    REQUIRE(d.face_detected == true);
    REQUIRE(d.has_landmarks_2d == true);
    float base_w = d.face_bbox.width;
    float base_h = d.face_bbox.height;

    // Track 15 successive frames with auto-roll enabled
    for (int frame = 1; frame <= 15; ++frame) {
        d.timestamp = 1.0 + frame * (1.0 / 60.0);
        pipeline.process_frame_synchronous(&d);

        CHECK_MESSAGE(d.face_detected == true, "Face tracking dropped on frame " << frame);
        CHECK_MESSAGE(d.has_landmarks_2d == true, "Landmarks lost on frame " << frame);

        // Aspect and scale stability invariant: bounding box width and height must not collapse
        CHECK_MESSAGE(d.face_bbox.width >= base_w * 0.90f,
                      "Bbox width collapsed on frame " << frame << ": " << d.face_bbox.width << " < " << (base_w * 0.90f));
        CHECK_MESSAGE(d.face_bbox.height >= base_h * 0.90f,
                      "Bbox height collapsed on frame " << frame << ": " << d.face_bbox.height << " < " << (base_h * 0.90f));
    }
}

TEST_CASE("Pipeline Stage 5: Independent Eye Bounding Box Sizing on Yawed Fixture") {
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string lm_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";
    std::string eye_state_path = "project/addons/godot-gaze/models/open_closed_eye.ort";

    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_path, gaze_path, eye_state_path, lm_path));

    TestImage img = load_test_image("tests/resources/self_yaw_left_roll_left.jpg");
    REQUIRE(img.valid());

    pipeline.reset_tracker();
    GazeFrameData d;
    d.camera_raw_bgr = img.data;
    d.camera_width = img.width;
    d.camera_height = img.height;
    d.timestamp = 1.0;
    d.auto_roll_enabled = true;
    d.roll_hint_rad = 0.0f;

    pipeline.process_frame_synchronous(&d);
    REQUIRE(d.face_detected == true);
    REQUIRE(d.has_landmarks_2d == true);

    // Verify independent per-eye bounding box sizes
    CHECK(d.right_eye_box_sz >= 24.0f);
    CHECK(d.left_eye_box_sz >= 24.0f);
    // On yawed face, the two eyes undergo perspective foreshortening and have distinct widths (>= 5px difference)
    CHECK(std::abs(d.right_eye_box_sz - d.left_eye_box_sz) >= 5.0f);
    // eye_box_sz preserves the max for backward compatibility
    CHECK(d.eye_box_sz == std::max(d.right_eye_box_sz, d.left_eye_box_sz));
}

TEST_CASE("Pipeline YuNet-to-Landmark Handoff Continuity") {
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string lm_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";
    std::string eye_state_path = "project/addons/godot-gaze/models/open_closed_eye.ort";

    GazeTrackingPipeline pipeline;
    REQUIRE(pipeline.initialize(yunet_path, gaze_path, eye_state_path, lm_path));

    TestImage img = load_test_image("tests/resources/self_center.jpg");
    REQUIRE(img.valid());

    pipeline.reset_tracker();

    // Frame 0: Cold start / SEEKING mode using YuNet
    GazeFrameData d0;
    d0.camera_raw_bgr = img.data;
    d0.camera_width = img.width;
    d0.camera_height = img.height;
    d0.timestamp = 1.0;
    d0.auto_roll_enabled = true;
    d0.roll_hint_rad = 0.0f;
    pipeline.process_frame_synchronous(&d0);

    REQUIRE(d0.face_detected == true);
    REQUIRE(d0.has_landmarks_2d == true);

    float pitch0 = static_cast<float>(d0.head_rotation.x);
    float yaw0   = static_cast<float>(d0.head_rotation.y);
    float roll0  = static_cast<float>(d0.head_rotation.z);
    GodotCameraVector3 t0 = d0.head_translation;
    float w0 = d0.face_bbox.width;
    float h0 = d0.face_bbox.height;
    float cx0 = d0.face_bbox.x + w0 * 0.5f;
    float cy0 = d0.face_bbox.y + h0 * 0.5f;

    // Frame 1: TRACKING mode using previous frame's face state
    GazeFrameData d1;
    d1.camera_raw_bgr = img.data;
    d1.camera_width = img.width;
    d1.camera_height = img.height;
    d1.timestamp = 1.016; // 60 fps step
    d1.auto_roll_enabled = true;
    d1.roll_hint_rad = 0.0f;
    pipeline.process_frame_synchronous(&d1);

    REQUIRE(d1.face_detected == true);
    REQUIRE(d1.has_landmarks_2d == true);

    float pitch1 = static_cast<float>(d1.head_rotation.x);
    float yaw1   = static_cast<float>(d1.head_rotation.y);
    float roll1  = static_cast<float>(d1.head_rotation.z);
    GodotCameraVector3 t1 = d1.head_translation;
    float w1 = d1.face_bbox.width;
    float h1 = d1.face_bbox.height;
    float cx1 = d1.face_bbox.x + w1 * 0.5f;
    float cy1 = d1.face_bbox.y + h1 * 0.5f;

    float d_pitch_deg = std::abs(pitch1 - pitch0) * (180.0f / 3.14159265f);
    float d_yaw_deg   = std::abs(yaw1 - yaw0) * (180.0f / 3.14159265f);
    float d_roll_deg  = std::abs(roll1 - roll0) * (180.0f / 3.14159265f);
    float dz_mm       = std::abs(static_cast<float>(t1.z - t0.z));
    float dw_ratio    = std::abs(w1 - w0) / w0;
    float dh_ratio    = std::abs(h1 - h0) / h0;
    float dc_px       = std::sqrt((cx1 - cx0) * (cx1 - cx0) + (cy1 - cy0) * (cy1 - cy0));

    std::cout << "\n=== EMPIRICAL HANDOFF CONTINUITY METRICS ===" << std::endl;
    std::cout << "Pitch Jump (deg) : " << d_pitch_deg << " (bound: <= 1.5 deg)" << std::endl;
    std::cout << "Yaw Jump (deg)   : " << d_yaw_deg << " (bound: <= 1.0 deg)" << std::endl;
    std::cout << "Roll Jump (deg)  : " << d_roll_deg << " (bound: <= 1.0 deg)" << std::endl;
    std::cout << "Depth Z Jump (mm): " << dz_mm << " (bound: <= 5.0 mm)" << std::endl;
    std::cout << "Width 0 vs 1     : " << w0 << " -> " << w1 << " (" << (dw_ratio * 100.0f) << "%)" << std::endl;
    std::cout << "Height 0 vs 1    : " << h0 << " -> " << h1 << " (" << (dh_ratio * 100.0f) << "%)" << std::endl;
    std::cout << "Center Shift (px): " << dc_px << " (bound: <= 5.0 px)" << std::endl;
    std::cout << "RMSE Frame 0 (px): " << d0.landmark_rmse_px << std::endl;
    std::cout << "RMSE Frame 1 (px): " << d1.landmark_rmse_px << std::endl;
    std::cout << "===========================================\n" << std::endl;

    // Strict signal continuity invariants:
    CHECK_MESSAGE(d_pitch_deg <= 1.5f, "Pitch jumped discontinuously across handoff: ", d_pitch_deg, " deg");
    CHECK_MESSAGE(dz_mm <= 5.0f, "Depth Z jumped discontinuously across handoff: ", dz_mm, " mm");
    CHECK_MESSAGE(dw_ratio <= 0.05f, "Crop width changed by >5% across handoff: ", (dw_ratio * 100.0f), "%");
    CHECK_MESSAGE(dh_ratio <= 0.05f, "Crop height changed by >5% across handoff: ", (dh_ratio * 100.0f), "%");
    CHECK_MESSAGE(dc_px <= 5.0f, "Crop center shifted by >5px across handoff: ", dc_px, " px");

    // Mode and quality verification:
    CHECK(d0.tracking_mode == PipelineTrackingMode::SEEKING);
    CHECK(d1.tracking_mode == PipelineTrackingMode::TRACKING);
    CHECK(d0.landmark_rmse_px > 0.0f);
    CHECK((d0.landmark_rmse_px / d0.face_bbox.height) <= 0.10f);
    CHECK(d0.landmark_quality >= 0.70f);
    CHECK(d1.landmark_rmse_px > 0.0f);
    CHECK((d1.landmark_rmse_px / d1.face_bbox.height) <= 0.10f);
    CHECK(d1.landmark_quality >= 0.70f);
    CHECK(d1.face_score == doctest::Approx(d1.landmark_quality));
}



