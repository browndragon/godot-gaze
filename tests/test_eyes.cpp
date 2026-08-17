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

#include "stb_image.h"

inline ImageBuffer load_test_bgr(const std::string& filepath) {
    ImageBuffer res;
    int w = 0, h = 0, c = 0;
    unsigned char* data = stbi_load(filepath.c_str(), &w, &h, &c, 3);
    if (!data) {
        std::string fallback = "../" + filepath;
        data = stbi_load(fallback.c_str(), &w, &h, &c, 3);
    }
    if (!data) {
        std::string fallback = "../../" + filepath;
        data = stbi_load(fallback.c_str(), &w, &h, &c, 3);
    }
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

TEST_CASE("MediaPipe Face Mesh - Both Eyes Open Fixture")
{
    auto pipeline = create_test_pipeline();
    REQUIRE(pipeline != nullptr);

    ImageBuffer img = load_test_bgr("tests/resources/self_center.jpg");
    REQUIRE(img.bgr_data.empty() == false);

    Frame frame{img.width, img.height, img.bgr_data.data(), 0};
    MediaPipeFaceMeshResult res;
    bool detected = pipeline->process_frame(frame, res);

    std::cout << "[TestEyes] self_center.jpg -> Left Openness: " << res.left_eye_openness << " | Right Openness: " << res.right_eye_openness << "\n";
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
    std::cout << "[TestEyes] eyes_both_wink.jpg -> Detected: " << (detected ? "true" : "false") << " | Left Openness: " << res.left_eye_openness << " | Right Openness: " << res.right_eye_openness << "\n";
    CHECK(detected == true);
    CHECK(res.face_detected == true);
    CHECK(res.right_eye_openness <= 0.50f);
    CHECK(res.left_eye_openness <= 0.50f);
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
    std::cout << "[TestEyes] eyes_anatomical_left_wink.jpg -> Detected: " << (detected ? "true" : "false") << " | Left Openness: " << res.left_eye_openness << " | Right Openness: " << res.right_eye_openness << "\n";
    CHECK(detected == true);
    CHECK(res.face_detected == true);
    CHECK(res.left_eye_openness <= 0.80f);
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
    std::cout << "[TestEyes] eyes_anatomical_right_wink.jpg -> Detected: " << (detected ? "true" : "false") << " | Right Openness: " << res.right_eye_openness << " | Left Openness: " << res.left_eye_openness << "\n";
    CHECK(detected == true);
    CHECK(res.face_detected == true);
    CHECK(res.right_eye_openness <= res.left_eye_openness);
}

TEST_CASE("MediaPipe Face Mesh - Tilted Head Anatomical Right Wink")
{
    auto pipeline = create_test_pipeline();
    REQUIRE(pipeline != nullptr);

    ImageBuffer img = load_test_bgr("tests/resources/eyes_tilted_anatomical_right_wink.jpg");
    REQUIRE(img.bgr_data.empty() == false);

    Frame frame{img.width, img.height, img.bgr_data.data(), 0};
    MediaPipeFaceMeshResult res;
    bool detected = pipeline->process_frame(frame, res);

    std::cout << "[TestEyes] eyes_tilted_anatomical_right_wink.jpg -> Right Openness: " << res.right_eye_openness << " | Left Openness: " << res.left_eye_openness << "\n";
    CHECK(detected == true);
    CHECK(res.face_detected == true);
    CHECK(res.right_eye_openness < res.left_eye_openness);
}

TEST_CASE("MediaPipe Face Mesh - Eye Crop Feature Intensity Variance and Pupil Content Verification")
{
    auto pipeline = create_test_pipeline();
    REQUIRE(pipeline != nullptr);

    ImageBuffer img = load_test_bgr("tests/resources/self_center.jpg");
    REQUIRE(img.bgr_data.empty() == false);

    Frame frame{img.width, img.height, img.bgr_data.data(), 0};
    MediaPipeFaceMeshResult res;
    bool detected = pipeline->process_frame(frame, res);
    REQUIRE(detected == true);

    auto calc_crop_stats = [](const uint8_t* crop, double& out_mean, double& out_stddev, double& out_center_lum, double& out_outer_lum) {
        double sum = 0.0;
        double sum_sq = 0.0;
        double center_sum = 0.0;
        int center_count = 0;
        double outer_sum = 0.0;
        int outer_count = 0;

        for (int y = 0; y < 60; y++) {
            for (int x = 0; x < 60; x++) {
                int idx = (y * 60 + x) * 3;
                double lum = 0.299 * crop[idx + 2] + 0.587 * crop[idx + 1] + 0.114 * crop[idx + 0];
                sum += lum;
                sum_sq += lum * lum;

                if (x >= 20 && x < 40 && y >= 20 && y < 40) {
                    center_sum += lum;
                    center_count++;
                } else {
                    outer_sum += lum;
                    outer_count++;
                }
            }
        }
        int total_pixels = 60 * 60;
        out_mean = sum / total_pixels;
        double var = (sum_sq / total_pixels) - (out_mean * out_mean);
        out_stddev = std::sqrt(std::max(0.0, var));
        out_center_lum = center_sum / center_count;
        out_outer_lum = outer_sum / outer_count;
    };

    double l_mean = 0, l_std = 0, l_center = 0, l_outer = 0;
    double r_mean = 0, r_std = 0, r_center = 0, r_outer = 0;
    calc_crop_stats(res.left_eye_crop, l_mean, l_std, l_center, l_outer);
    calc_crop_stats(res.right_eye_crop, r_mean, r_std, r_center, r_outer);

    std::cout << "[TestEyes] Left Crop -> Mean: " << l_mean << " | StdDev: " << l_std << " | Center: " << l_center << " | Outer: " << l_outer << "\n";
    std::cout << "[TestEyes] Right Crop -> Mean: " << r_mean << " | StdDev: " << r_std << " | Center: " << r_center << " | Outer: " << r_outer << "\n";

    // 1. High-contrast facial feature verification (stddev >= 5.0)
    CHECK_MESSAGE(l_std >= 5.0, "Left eye crop lacks facial/iris feature contrast (StdDev: " << l_std << ")");
    CHECK_MESSAGE(r_std >= 5.0, "Right eye crop lacks facial/iris feature contrast (StdDev: " << r_std << ")");

    // 2. Pupil/Iris darkness verification (pupil region is darker than outer sclera)
    CHECK_MESSAGE(r_center < r_outer, "Right eye crop center (pupil) is not darker than outer sclera region");
}

