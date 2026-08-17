#include "doctest.h"
#include "ort_yunet_detector.hpp"
#include <fstream>
#include <vector>
#include <cmath>

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

    // Lock exact numerical ground-truth keypoint values for self_center.jpg
    CHECK(res.right_eye_px.x == doctest::Approx(684.0).epsilon(0.01));
    CHECK(res.right_eye_px.y == doctest::Approx(399.0).epsilon(0.01));

    CHECK(res.left_eye_px.x == doctest::Approx(837.0).epsilon(0.01));
    CHECK(res.left_eye_px.y == doctest::Approx(413.0).epsilon(0.01));

    CHECK(res.nose_tip_px.x == doctest::Approx(749.0).epsilon(0.01));
    CHECK(res.nose_tip_px.y == doctest::Approx(495.0).epsilon(0.01));

    CHECK(res.mouth_right_px.x == doctest::Approx(683.0).epsilon(0.01));
    CHECK(res.mouth_right_px.y == doctest::Approx(571.0).epsilon(0.01));

    CHECK(res.mouth_left_px.x == doctest::Approx(814.0).epsilon(0.01));
    CHECK(res.mouth_left_px.y == doctest::Approx(584.0).epsilon(0.01));

    // Verify 3D Head Pose translation & rotation solved by PnP for self_center.jpg
    CHECK(res.head_pose.trans_z_mm == doctest::Approx(784.25).epsilon(0.01));
    CHECK(res.head_pose.pitch_rad == doctest::Approx(-0.364).epsilon(0.01));
    CHECK(res.head_pose.yaw_rad == doctest::Approx(0.0304).epsilon(0.01));
    CHECK(res.head_pose.roll_rad == doctest::Approx(-3.059).epsilon(0.01));
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
        {"self_center.jpg", 0.0f, 684.0f, 399.0f, 837.0f, 413.0f, 749.0f, 495.0f},
        {"self_left_left.jpg", 0.0f, 774.0f, 397.0f, 925.0f, 391.0f, 877.0f, 483.0f},
        {"self_right_right.jpg", 0.0f, 615.0f, 396.0f, 772.0f, 408.0f, 650.0f, 507.0f},
        {"self_top_top.jpg", 0.0f, 701.0f, 375.0f, 854.0f, 383.0f, 776.0f, 467.0f},
        {"self_down_down.jpg", 0.0f, 707.0f, 422.0f, 864.0f, 433.0f, 779.0f, 524.0f},
        {"self_roll_left.jpg", -45.0f * (3.14159265f / 180.0f), 496.0f, 245.0f, 577.0f, 315.0f, 495.0f, 329.0f},
        {"self_roll_right.jpg", 45.0f * (3.14159265f / 180.0f), 465.0f, 202.0f, 548.0f, 120.0f, 554.0f, 215.0f}
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
