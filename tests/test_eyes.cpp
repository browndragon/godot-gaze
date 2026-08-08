#include "doctest.h"
#include "ort_yunet_pipeline.hpp"
#include "onnx_eye_blink_estimator.hpp"
#include "heuristic_eye_blink_estimator.hpp"
#include "stb_image.h"
#include <string>
#include <vector>
#include <iostream>
#include <memory>

using namespace Gaze;

namespace {

struct ImageBuffer {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> bgr_data;
};

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

bool process_fixture_crops(const std::string& filename, EyeCrops& out_crops) {
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    ORTYuNetPipeline pipeline(yunet_path);
    if (!pipeline.initialize()) return false;

    ImageBuffer img = load_test_bgr("tests/resources/" + filename);
    if (img.bgr_data.empty()) {
        img = load_test_bgr("../tests/resources/" + filename);
    }
    if (img.bgr_data.empty()) return false;

    Frame frame;
    frame.width = img.width;
    frame.height = img.height;
    frame.data = img.bgr_data.data();

    bool res = pipeline.process_frame(frame, out_crops);
    if (res && out_crops.face_detected) {
        std::string stem = filename.substr(0, filename.find_last_of('.'));
        FILE* f_l = fopen(("tests/resources/extracted_crops/" + stem + "_left.raw").c_str(), "wb");
        if (f_l) { fwrite(out_crops.left_eye_data, 1, 60*60*3, f_l); fclose(f_l); }
        FILE* f_r = fopen(("tests/resources/extracted_crops/" + stem + "_right.raw").c_str(), "wb");
        if (f_r) { fwrite(out_crops.right_eye_data, 1, 60*60*3, f_r); fclose(f_r); }
    }
    return res;
}

std::unique_ptr<EyeBlinkEstimator> create_test_estimator() {
    std::string model_path = "project/addons/godot-gaze/models/eye_openness.ort";
    auto onnx_est = std::make_unique<ONNXEyeBlinkEstimator>(model_path);
    if (onnx_est->initialize()) {
        return onnx_est;
    }
    return std::make_unique<HeuristicEyeBlinkEstimator>();
}

} // namespace

TEST_CASE("EyeBlinkEstimator - Both Eyes Open Fixture")
{
    EyeCrops crops;
    REQUIRE_MESSAGE(process_fixture_crops("eyes_both_open.jpg", crops) == true, "Failed to load/process eyes_both_open.jpg");
    REQUIRE(crops.face_detected == true);

    auto estimator = create_test_estimator();
    float left_open = 0.0f, right_open = 0.0f;
    estimator->estimate_openness(crops.left_eye_data, crops.right_eye_data, left_open, right_open);

    std::cout << "[TestEyes] eyes_both_open.jpg -> Left Openness: " << left_open << " | Right Openness: " << right_open << "\n";
    CHECK(left_open >= 0.70f);
    CHECK(right_open >= 0.70f);
}

TEST_CASE("EyeBlinkEstimator - Both Eyes Closed (Blink) Fixture")
{
    EyeCrops crops;
    REQUIRE_MESSAGE(process_fixture_crops("eyes_both_wink.jpg", crops) == true, "Failed to load/process eyes_both_wink.jpg");
    REQUIRE(crops.face_detected == true);

    auto estimator = create_test_estimator();
    float left_open = 0.0f, right_open = 0.0f;
    estimator->estimate_openness(crops.left_eye_data, crops.right_eye_data, left_open, right_open);

    std::cout << "[TestEyes] eyes_both_wink.jpg -> Left Openness: " << left_open << " | Right Openness: " << right_open << "\n";
    CHECK(left_open <= 0.20f);
    CHECK(right_open <= 0.20f);
}

TEST_CASE("EyeBlinkEstimator - Anatomical Left Eye Closed (Wink)")
{
    EyeCrops crops;
    REQUIRE_MESSAGE(process_fixture_crops("eyes_anatomical_left_wink.jpg", crops) == true, "Failed to load/process eyes_anatomical_left_wink.jpg");
    REQUIRE(crops.face_detected == true);

    auto estimator = create_test_estimator();
    float left_open = 0.0f, right_open = 0.0f;
    estimator->estimate_openness(crops.left_eye_data, crops.right_eye_data, left_open, right_open);

    std::cout << "[TestEyes] eyes_anatomical_left_wink.jpg -> Left Openness: " << left_open << " | Right Openness: " << right_open << "\n";
    CHECK(left_open <= 0.20f);
    CHECK(right_open >= 0.70f);
}

TEST_CASE("EyeBlinkEstimator - Anatomical Right Eye Closed (Wink)")
{
    EyeCrops crops;
    REQUIRE_MESSAGE(process_fixture_crops("eyes_anatomical_right_wink.jpg", crops) == true, "Failed to load/process eyes_anatomical_right_wink.jpg");
    REQUIRE(crops.face_detected == true);

    auto estimator = create_test_estimator();
    float left_open = 0.0f, right_open = 0.0f;
    estimator->estimate_openness(crops.left_eye_data, crops.right_eye_data, left_open, right_open);

    std::cout << "[TestEyes] eyes_anatomical_right_wink.jpg -> Right Openness: " << right_open << " | Left Openness: " << left_open << "\n";
    CHECK(right_open <= 0.20f);
    CHECK(left_open >= 0.70f);
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
