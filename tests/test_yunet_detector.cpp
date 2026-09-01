#include "doctest.h"
#include "ort_yunet_detector.hpp"
#include <fstream>
#include <vector>
#include <cmath>
#include <iostream>
#include <iomanip>

#ifndef STB_IMAGE_IMPLEMENTATION_INCLUDED
#include "stb_image.h"
#endif

inline bool yunet_file_exists(const std::string &filename) {
    std::ifstream f(filename.c_str());
    return f.good();
}

struct TestImage
{
    int width = 0;
    int height = 0;
    std::vector<unsigned char> data; // BGR format
};

inline TestImage load_yunet_test_image(const std::string &filepath)
{
    TestImage result;
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

TEST_CASE("ORT YuNet Detector Boundary Invariants and Head Local Space Model Points")
{
    std::string model_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    if (!yunet_file_exists(model_path)) {
        model_path = "../project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    }

    if (!yunet_file_exists(model_path)) {
        MESSAGE("Skipping ORT YuNet Detector test: Model file not found");
        return;
    }

    Gaze::ORTYuNetDetector detector(model_path);
    REQUIRE(detector.initialize() == true);

    // Verify 5 keypoint model point layout in Head Local Space:
    // Origin (0,0,0) = Nose Tip
    // +X = Subject's Anatomical Right (+31.5mm)
    // -X = Subject's Anatomical Left (-31.5mm)
    auto godot_pts = detector.get_canonical_godot_model_points();
    REQUIRE(godot_pts.size() == 5);

    // Nose tip (0) is at origin in local space
    CHECK(godot_pts[0].x == doctest::Approx(0.0));
    CHECK(godot_pts[0].y == doctest::Approx(0.0));

    // Right Eye (1) in Head Local Space has positive X (+31.5mm)
    CHECK(godot_pts[1].x > 0.0);
    CHECK(godot_pts[1].y > 0.0); // +Y is UP in Head Local Space

    // Left Eye (2) in Head Local Space has negative X (-31.5mm)
    CHECK(godot_pts[2].x < 0.0);
    CHECK(godot_pts[2].y > 0.0); // +Y is UP in Head Local Space
}

TEST_CASE("ORT YuNet 5-Keypoint Extraction on Real Image self_center.jpg")
{
    std::string model_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    if (!yunet_file_exists(model_path)) {
        model_path = "../project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    }

    if (!yunet_file_exists(model_path)) {
        MESSAGE("Skipping test: Model file not found");
        return;
    }

    std::string img_path = "tests/resources/self_center.jpg";
    if (!yunet_file_exists(img_path)) {
        img_path = "../tests/resources/self_center.jpg";
    }

    TestImage img = load_yunet_test_image(img_path);
    REQUIRE_MESSAGE(!img.data.empty(), "Failed to load test image self_center.jpg");

    Gaze::ORTYuNetDetector detector(model_path);
    REQUIRE(detector.initialize() == true);

    Gaze::Frame frame;
    frame.width = img.width;
    frame.height = img.height;
    frame.data = img.data.data();

    Gaze::YuNetResult res;
    bool ok = detector.process_frame(frame, res);
    REQUIRE(ok == true);
    REQUIRE(res.face_detected == true);

    // Lock exact numerical ground-truth keypoint values for self_center.jpg under Central 1:1 Square Crop
    CHECK(res.right_eye_px.x == doctest::Approx(682.8).epsilon(0.01));
    CHECK(res.right_eye_px.y == doctest::Approx(389.9).epsilon(0.01));

    CHECK(res.left_eye_px.x == doctest::Approx(834.0).epsilon(0.01));
    CHECK(res.left_eye_px.y == doctest::Approx(394.2).epsilon(0.01));

    CHECK(res.nose_tip_px.x == doctest::Approx(757.0).epsilon(0.01));
    CHECK(res.nose_tip_px.y == doctest::Approx(489.1).epsilon(0.01));

    CHECK(res.mouth_right_px.x == doctest::Approx(692.1).epsilon(0.01));
    CHECK(res.mouth_right_px.y == doctest::Approx(563.8).epsilon(0.01));

    CHECK(res.mouth_left_px.x == doctest::Approx(814.0).epsilon(0.01));
    CHECK(res.mouth_left_px.y == doctest::Approx(567.5).epsilon(0.01));

    // Verify 3D Head Pose translation & rotation solved by OpenCV SQPnP for self_center.jpg
    CHECK(res.head_pose.trans_z_mm == doctest::Approx(785.45).epsilon(0.05));
    CHECK(res.head_pose.pitch_rad == doctest::Approx(0.113).epsilon(0.05));
    CHECK(res.head_pose.yaw_rad == doctest::Approx(0.0).epsilon(0.05));
    CHECK(res.head_pose.roll_rad == doctest::Approx(0.031).epsilon(0.05));
}

TEST_CASE("ORT YuNet Full Benchmark Image Suite Keypoint Invariants")
{
    std::string model_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    if (!yunet_file_exists(model_path)) {
        model_path = "../project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    }

    if (!yunet_file_exists(model_path)) {
        MESSAGE("Skipping test: Model file not found");
        return;
    }

    Gaze::ORTYuNetDetector detector(model_path);
    REQUIRE(detector.initialize() == true);

    struct BenchmarkExpectation {
        std::string filename;
        float roll_hint_rad;
        float expected_r_eye_x;
        float expected_r_eye_y;
        float expected_l_eye_x;
        float expected_l_eye_y;
        float expected_nose_x;
        float expected_nose_y;
    };

    std::vector<BenchmarkExpectation> suite = {
        {"self_center.jpg", 0.0f, 682.8f, 389.9f, 834.0f, 394.2f, 757.0f, 489.1f},
        {"self_left_left.jpg", 0.0f, 773.2f, 387.5f, 924.5f, 382.1f, 876.5f, 474.0f},
        {"self_right_right.jpg", 0.0f, 618.9f, 398.9f, 778.6f, 396.3f, 674.4f, 497.1f},
        {"self_top_top.jpg", 0.0f, 700.5f, 365.1f, 853.5f, 356.4f, 775.5f, 444.6f},
        {"self_down_down.jpg", 0.0f, 706.5f, 425.2f, 863.5f, 423.5f, 778.5f, 514.5f},
        {"self_roll_left.jpg", 45.0f * (3.14159265f / 180.0f), 453.5f, 286.6f, 516.0f, 314.2f, 451.2f, 328.7f},
        {"self_roll_right.jpg", -45.0f * (3.14159265f / 180.0f), 615.7f, 348.0f, 686.0f, 273.6f, 693.8f, 358.5f}
    };

    for (const auto &item : suite) {
        std::string img_path = "tests/resources/" + item.filename;
        if (!yunet_file_exists(img_path)) {
            img_path = "../tests/resources/" + item.filename;
        }

        TestImage img = load_yunet_test_image(img_path);
        if (img.data.empty()) continue;

        Gaze::Frame frame;
        frame.width = img.width;
        frame.height = img.height;
        frame.data = img.data.data();

        Gaze::YuNetResult res;
        bool ok = detector.process_frame(frame, res, item.roll_hint_rad);
        CHECK_MESSAGE(ok == true, "Failed on image: ", item.filename);
        CHECK_MESSAGE(res.face_detected == true, "No face detected in: ", item.filename);

        CHECK(res.right_eye_px.x == doctest::Approx(item.expected_r_eye_x).epsilon(0.02));
        CHECK(res.right_eye_px.y == doctest::Approx(item.expected_r_eye_y).epsilon(0.02));
        CHECK(res.left_eye_px.x == doctest::Approx(item.expected_l_eye_x).epsilon(0.02));
        CHECK(res.left_eye_px.y == doctest::Approx(item.expected_l_eye_y).epsilon(0.02));
        CHECK(res.nose_tip_px.x == doctest::Approx(item.expected_nose_x).epsilon(0.02));
        CHECK(res.nose_tip_px.y == doctest::Approx(item.expected_nose_y).epsilon(0.02));
    }
}

TEST_CASE("ORT YuNet Detection Robustness on Chin-Clipped Frames")
{
    std::string model_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    if (!yunet_file_exists(model_path)) {
        model_path = "../project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    }
    if (!yunet_file_exists(model_path)) {
        MESSAGE("Skipping test: Model file not found");
        return;
    }

    Gaze::ORTYuNetDetector detector(model_path);
    REQUIRE(detector.initialize() == true);

    std::string img_path = "tests/resources/self_center.jpg";
    if (!yunet_file_exists(img_path)) {
        img_path = "../tests/resources/self_center.jpg";
    }

    TestImage base_img = load_yunet_test_image(img_path);
    REQUIRE(!base_img.data.empty());

    // Test truncated frames where chin is clipped at bottom edge
    // self_center.jpg baseline chin apex is at y ~ 679px
    std::vector<int> test_heights = {650, 620, 590};
    for (int h : test_heights) {
        Gaze::Frame clipped_frame;
        clipped_frame.width = base_img.width;
        clipped_frame.height = h;
        clipped_frame.data = base_img.data.data();

        Gaze::YuNetResult res;
        bool ok = detector.process_frame(clipped_frame, res, 0.0f);
        CHECK_MESSAGE(ok == true, "Processing failed on clipped height: ", h);
        CHECK_MESSAGE(res.face_detected == true, "Face detection lost on chin-clipped height: ", h);
        CHECK_MESSAGE(res.score >= 0.30f, "Detection score too low on chin-clipped height: ", h);
    }
}

TEST_CASE("Empirical Test: YuNet Face Detection Across Roll Angles with Roll Hint")
{
    std::string model_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    if (!yunet_file_exists(model_path)) {
        model_path = "../project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    }
    REQUIRE(yunet_file_exists(model_path));

    Gaze::ORTYuNetDetector detector(model_path);
    REQUIRE(detector.initialize() == true);

    std::string img_path = "tests/resources/self_center.jpg";
    if (!yunet_file_exists(img_path)) {
        img_path = "../tests/resources/self_center.jpg";
    }
    TestImage base_img = load_yunet_test_image(img_path);
    REQUIRE(!base_img.data.empty());

    int w = base_img.width;
    int h = base_img.height;

    // Verify that with roll hint, YuNet detects face across entire -90 to +90 degree range
    for (int deg = -90; deg <= 90; deg += 15) {
        float rad = deg * (3.14159265f / 180.0f);
        std::vector<unsigned char> rot_data(w * h * 3);
        Gaze::rotate_image(base_img.data.data(), w, h, rot_data.data(), rad);

        Gaze::Frame frame;
        frame.width = w;
        frame.height = h;
        frame.data = rot_data.data();

        Gaze::YuNetResult res_withhint;
        bool ok = detector.process_frame(frame, res_withhint, rad);
        CHECK_MESSAGE(ok == true, "Processing failed for angle: ", deg);
        CHECK_MESSAGE(res_withhint.face_detected == true, "Face detection lost with hint at angle: ", deg);
        CHECK_MESSAGE(res_withhint.score >= 0.70f, "Detection score degraded at angle: ", deg);
    }
}


