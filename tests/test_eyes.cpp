#include "doctest.h"
#include "ort_mediapipe_face_mesh.hpp"
#include "gaze_frame_data.hpp"
#include <fstream>
#include <vector>
#include <iostream>
#include <memory>

using namespace Gaze;

struct ImageBuffer {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> bgr_data;
};

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

inline ImageBuffer load_test_bgr(const std::string& filepath) {
    ImageBuffer res;
    int w = 0, h = 0, c = 0;
    unsigned char* data = stbi_load(filepath.c_str(), &w, &h, &c, 3);
    if (!data) return res;

    res.width = w;
    res.height = h;
    res.bgr_data.resize(w * h * 3);
    for (int i = 0; i < w * h; ++i) {
        res.bgr_data[i * 3 + 0] = data[i * 3 + 2]; // B
        res.bgr_data[i * 3 + 1] = data[i * 3 + 1]; // G
        res.bgr_data[i * 3 + 2] = data[i * 3 + 0]; // R
    }
    stbi_image_free(data);
    return res;
}

std::unique_ptr<MediaPipeFaceMeshPipeline> create_test_pipeline() {
    std::string model_path = "project/addons/godot-gaze/models/mediapipe_face_mesh.ort";
    auto mp = std::make_unique<MediaPipeFaceMeshPipeline>(model_path);
    if (mp->initialize()) {
        return mp;
    }
    return nullptr;
}

} // namespace

TEST_CASE("MediaPipe Face Mesh - Both Eyes Open Fixture")
{
    auto pipeline = create_test_pipeline();
    REQUIRE(pipeline != nullptr);

    ImageBuffer img = load_test_bgr("tests/resources/eyes_both_open.jpg");
    REQUIRE(img.bgr_data.empty() == false);

    Frame frame{img.width, img.height, img.bgr_data.data(), 0};
    MediaPipeFaceMeshResult res;
    bool detected = pipeline->process_frame(frame, res);

    std::cout << "[TestEyes] eyes_both_open.jpg -> Left Openness: " << res.left_eye_openness << " | Right Openness: " << res.right_eye_openness << "\n";
    CHECK(detected == true);
    CHECK(res.face_detected == true);
    CHECK(res.left_eye_openness >= 0.70f);
    CHECK(res.right_eye_openness >= 0.70f);
}

TEST_CASE("MediaPipe Face Mesh - Both Eyes Closed (Blink) Fixture")
{
    auto pipeline = create_test_pipeline();
    REQUIRE(pipeline != nullptr);

    ImageBuffer img = load_test_bgr("tests/resources/eyes_both_wink.jpg");
    REQUIRE(img.bgr_data.empty() == false);

    Frame frame{img.width, img.height, img.bgr_data.data(), 0};
    MediaPipeFaceMeshResult res;
    bool detected = pipeline->process_frame(frame, res);

    std::cout << "[TestEyes] eyes_both_wink.jpg -> Left Openness: " << res.left_eye_openness << " | Right Openness: " << res.right_eye_openness << "\n";
    CHECK(detected == true);
    CHECK(res.face_detected == true);
    CHECK(res.left_eye_openness <= 0.20f);
    CHECK(res.right_eye_openness <= 0.20f);
}

TEST_CASE("MediaPipe Face Mesh - Anatomical Left Wink Fixture")
{
    auto pipeline = create_test_pipeline();
    REQUIRE(pipeline != nullptr);

    ImageBuffer img = load_test_bgr("tests/resources/eyes_anatomical_left_wink.jpg");
    REQUIRE(img.bgr_data.empty() == false);

    Frame frame{img.width, img.height, img.bgr_data.data(), 0};
    MediaPipeFaceMeshResult res;
    bool detected = pipeline->process_frame(frame, res);

    std::cout << "[TestEyes] eyes_anatomical_left_wink.jpg -> Left Openness: " << res.left_eye_openness << " | Right Openness: " << res.right_eye_openness << "\n";
    CHECK(detected == true);
    CHECK(res.face_detected == true);
    CHECK(res.left_eye_openness <= 0.20f);
    CHECK(res.right_eye_openness >= 0.70f);
}

TEST_CASE("MediaPipe Face Mesh - Anatomical Right Wink Fixture")
{
    auto pipeline = create_test_pipeline();
    REQUIRE(pipeline != nullptr);

    ImageBuffer img = load_test_bgr("tests/resources/eyes_anatomical_right_wink.jpg");
    REQUIRE(img.bgr_data.empty() == false);

    Frame frame{img.width, img.height, img.bgr_data.data(), 0};
    MediaPipeFaceMeshResult res;
    bool detected = pipeline->process_frame(frame, res);

    std::cout << "[TestEyes] eyes_anatomical_right_wink.jpg -> Right Openness: " << res.right_eye_openness << " | Left Openness: " << res.left_eye_openness << "\n";
    CHECK(detected == true);
    CHECK(res.face_detected == true);
    CHECK(res.right_eye_openness <= 0.20f);
    CHECK(res.left_eye_openness >= 0.70f);
}

TEST_CASE("EyeBlinkEstimator - Tilted Head Anatomical Right Wink")
{
    EyeCrops crops;
    REQUIRE_MESSAGE(process_fixture_crops("eyes_tilted_anatomical_right_wink.jpg", crops) == true, "Failed to load/process eyes_tilted_anatomical_right_wink.jpg");
    REQUIRE(crops.face_detected == true);

    auto estimator = create_test_estimator();
    float left_open = 0.0f, right_open = 0.0f;
    estimator->estimate_openness(crops.left_eye_data, crops.right_eye_data, left_open, right_open);

    std::cout << "[TestEyes] eyes_tilted_anatomical_right_wink.jpg -> Right Openness: " << right_open << " | Left Openness: " << left_open << "\n";
    CHECK(left_open >= 0.70f);
}

TEST_CASE("EyeBlinkEstimator - Anatomical Left Obscured Edge Case")
{
    EyeCrops crops;
    bool face_found = process_fixture_crops("eyes_anatomical_left_obscured.jpg", crops);
    std::cout << "[TestEyes] eyes_anatomical_left_obscured.jpg -> Face Detected: " << (face_found ? "true" : "false") << "\n";
    if (face_found) {
        auto estimator = create_test_estimator();
        float left_open = 0.0f, right_open = 0.0f;
        estimator->estimate_openness(crops.left_eye_data, crops.right_eye_data, left_open, right_open);
        std::cout << "[TestEyes] eyes_anatomical_left_obscured.jpg -> Left Openness: " << left_open << " | Right Openness: " << right_open << "\n";
        CHECK(left_open <= 0.20f);
    }
}
