#include "doctest.h"
#include "ort_yunet_detector.hpp"
#include "ort_landmark_model.hpp"
#include "ort_eye_state_model.hpp"
#include "gaze_frame_data.hpp"
#include "math_defs.hpp"
#include <fstream>
#include <vector>
#include <iostream>
#include <memory>
#include <cmath>

using namespace Gaze;

struct ImageBuffer {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> bgr_data;
};

#include "stb_image.h"

inline bool eye_state_file_exists(const std::string &filename) {
    std::ifstream f(filename.c_str());
    return f.good();
}

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

static void extract_dense_eye_crops_60x60(
    const uint8_t *frame_bgr, int width, int height,
    const std::vector<GazeVector2> &landmarks_35,
    uint8_t *out_right_crop_bgr, uint8_t *out_left_crop_bgr,
    float scale_factor = 2.2f)
{
    // Landmark 0: Right Eye Inner Canthus, 1: Right Eye Outer Canthus
    // Landmark 2: Left Eye Inner Canthus,  3: Left Eye Outer Canthus
    float r_cx = (landmarks_35[0].x + landmarks_35[1].x) * 0.5f;
    float r_cy = (landmarks_35[0].y + landmarks_35[1].y) * 0.5f;
    float r_dx = landmarks_35[0].x - landmarks_35[1].x;
    float r_dy = landmarks_35[0].y - landmarks_35[1].y;
    float r_w = std::sqrt(r_dx * r_dx + r_dy * r_dy);

    float l_cx = (landmarks_35[2].x + landmarks_35[3].x) * 0.5f;
    float l_cy = (landmarks_35[2].y + landmarks_35[3].y) * 0.5f;
    float l_dx = landmarks_35[2].x - landmarks_35[3].x;
    float l_dy = landmarks_35[2].y - landmarks_35[3].y;
    float l_w = std::sqrt(l_dx * l_dx + l_dy * l_dy);

    float r_box_s = std::max(20.0f, r_w * scale_factor);
    float l_box_s = std::max(20.0f, l_w * scale_factor);

    crop_and_resize_bgr(
        frame_bgr, width, height,
        r_cx - r_box_s * 0.5f, r_cy - r_box_s * 0.5f, r_box_s, r_box_s,
        out_right_crop_bgr, 60, 60
    );

    crop_and_resize_bgr(
        frame_bgr, width, height,
        l_cx - l_box_s * 0.5f, l_cy - l_box_s * 0.5f, l_box_s, l_box_s,
        out_left_crop_bgr, 60, 60
    );
}

TEST_CASE("Phase 4: Dynamic Eye Cropping & Eye State Classification on Baseline")
{
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string lm_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    std::string eye_state_path = "project/addons/godot-gaze/models/open_closed_eye.ort";
    if (!eye_state_file_exists(eye_state_path)) {
        eye_state_path = "../project/addons/godot-gaze/models/open_closed_eye.ort";
    }

    ORTYuNetDetector detector(yunet_path);
    REQUIRE(detector.initialize() == true);

    ORTLandmarkModel lm_model(lm_path);
    REQUIRE(lm_model.initialize() == true);

    ORTEyeStateModel eye_state(eye_state_path);
    REQUIRE(eye_state.initialize() == true);

    ImageBuffer img = load_test_bgr("tests/resources/self_center.jpg");
    REQUIRE(img.bgr_data.empty() == false);

    Frame frame{img.width, img.height, img.bgr_data.data(), 0};
    YuNetResult det_res;
    bool det_ok = detector.process_frame(frame, det_res, 0.0f);
    REQUIRE(det_ok);
    REQUIRE(det_res.face_detected);

    GazeRect bbox(det_res.roi_x, det_res.roi_y, det_res.roi_w, det_res.roi_h);
    std::vector<GazeVector2> landmarks;
    bool lm_ok = lm_model.extract_landmarks(frame.data, frame.width, frame.height, bbox, landmarks, 0.0f);
    REQUIRE(lm_ok);
    REQUIRE(landmarks.size() == 35);

    uint8_t r_crop[60 * 60 * 3];
    uint8_t l_crop[60 * 60 * 3];
    extract_dense_eye_crops_60x60(frame.data, frame.width, frame.height, landmarks, r_crop, l_crop, 1.8f);

    float r_open = 0.0f, l_open = 0.0f;
    bool ok_r = eye_state.estimate_openness(r_crop, r_open);
    bool ok_l = eye_state.estimate_openness(l_crop, l_open);

    REQUIRE(ok_r);
    REQUIRE(ok_l);

    std::cout << "[Phase 4 Test] self_center.jpg -> Right Openness: " << r_open << " | Left Openness: " << l_open << "\n";

    // Strict Domain Assertions for Open Eyes
    CHECK(l_open >= 0.70f);
}

TEST_CASE("Phase 4: Dynamic Eye Cropping Secondary Baseline (self_center2.jpg)")
{
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string lm_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    std::string eye_state_path = "project/addons/godot-gaze/models/open_closed_eye.ort";
    if (!eye_state_file_exists(eye_state_path)) {
        eye_state_path = "../project/addons/godot-gaze/models/open_closed_eye.ort";
    }

    ORTYuNetDetector detector(yunet_path);
    REQUIRE(detector.initialize() == true);

    ORTLandmarkModel lm_model(lm_path);
    REQUIRE(lm_model.initialize() == true);

    ORTEyeStateModel eye_state(eye_state_path);
    REQUIRE(eye_state.initialize() == true);

    ImageBuffer img = load_test_bgr("tests/resources/self_center2.jpg");
    REQUIRE(img.bgr_data.empty() == false);

    Frame frame{img.width, img.height, img.bgr_data.data(), 0};
    YuNetResult det_res;
    bool det_ok = detector.process_frame(frame, det_res, 0.0f);
    REQUIRE(det_ok);

    GazeRect bbox(det_res.roi_x, det_res.roi_y, det_res.roi_w, det_res.roi_h);
    std::vector<GazeVector2> landmarks;
    bool lm_ok = lm_model.extract_landmarks(frame.data, frame.width, frame.height, bbox, landmarks, 0.0f);
    REQUIRE(lm_ok);

    uint8_t r_crop[60 * 60 * 3];
    uint8_t l_crop[60 * 60 * 3];
    extract_dense_eye_crops_60x60(frame.data, frame.width, frame.height, landmarks, r_crop, l_crop, 1.8f);

    float r_open = 0.0f, l_open = 0.0f;
    eye_state.estimate_openness(r_crop, r_open);
    eye_state.estimate_openness(l_crop, l_open);

    std::cout << "[Phase 4 Test] self_center2.jpg -> Right Openness: " << r_open << " | Left Openness: " << l_open << "\n";

    CHECK(r_open >= 0.70f);
    CHECK(l_open >= 0.70f);
}

TEST_CASE("Phase 4: Eye Crop Feature Intensity Contrast & Pupil Content")
{
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string lm_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";

    ORTYuNetDetector detector(yunet_path);
    REQUIRE(detector.initialize() == true);

    ORTLandmarkModel lm_model(lm_path);
    REQUIRE(lm_model.initialize() == true);

    ImageBuffer img = load_test_bgr("tests/resources/self_center.jpg");
    Frame frame{img.width, img.height, img.bgr_data.data(), 0};
    YuNetResult det_res;
    detector.process_frame(frame, det_res, 0.0f);

    GazeRect bbox(det_res.roi_x, det_res.roi_y, det_res.roi_w, det_res.roi_h);
    std::vector<GazeVector2> landmarks;
    lm_model.extract_landmarks(frame.data, frame.width, frame.height, bbox, landmarks, 0.0f);

    uint8_t r_crop[60 * 60 * 3];
    uint8_t l_crop[60 * 60 * 3];
    extract_dense_eye_crops_60x60(frame.data, frame.width, frame.height, landmarks, r_crop, l_crop, 1.5f);

    auto calc_crop_stats = [](const uint8_t* crop, double& out_mean, double& out_stddev, double& out_center_lum, double& out_outer_lum) {
        double sum = 0.0, sum_sq = 0.0;
        double center_sum = 0.0, outer_sum = 0.0;
        int center_count = 0, outer_count = 0;

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
    calc_crop_stats(l_crop, l_mean, l_std, l_center, l_outer);
    calc_crop_stats(r_crop, r_mean, r_std, r_center, r_outer);

    CHECK_MESSAGE(l_std >= 5.0, "Left eye crop lacks feature contrast");
    CHECK_MESSAGE(r_std >= 5.0, "Right eye crop lacks feature contrast");
}

TEST_CASE("Phase 4: Wink and Blink Strict Signal Separation Fixtures")
{
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string lm_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    std::string eye_state_path = "project/addons/godot-gaze/models/open_closed_eye.ort";
    if (!eye_state_file_exists(eye_state_path)) {
        eye_state_path = "project/addons/godot-gaze/models/mediapipe_eye_openness.ort";
    }

    ORTYuNetDetector detector(yunet_path);
    REQUIRE(detector.initialize() == true);

    ORTLandmarkModel lm_model(lm_path);
    REQUIRE(lm_model.initialize() == true);

    ORTEyeStateModel eye_state(eye_state_path);
    REQUIRE(eye_state.initialize() == true);

    auto eval_sample = [&](const std::string &filename, float &out_r, float &out_l) {
        ImageBuffer img = load_test_bgr("tests/resources/" + filename);
        REQUIRE(img.bgr_data.empty() == false);

        Frame frame{img.width, img.height, img.bgr_data.data(), 0};
        YuNetResult det_res;
        bool det_ok = detector.process_frame(frame, det_res, 0.0f);
        REQUIRE(det_ok);
        REQUIRE(det_res.face_detected);

        GazeRect bbox(det_res.roi_x, det_res.roi_y, det_res.roi_w, det_res.roi_h);
        std::vector<GazeVector2> landmarks;
        bool lm_ok = lm_model.extract_landmarks(frame.data, frame.width, frame.height, bbox, landmarks, 0.0f);
        REQUIRE(lm_ok);
        REQUIRE(landmarks.size() == 35);

        uint8_t r_crop[60 * 60 * 3];
        uint8_t l_crop[60 * 60 * 3];
        extract_dense_eye_crops_60x60(frame.data, frame.width, frame.height, landmarks, r_crop, l_crop, 2.2f);

        eye_state.estimate_openness(r_crop, out_r);
        eye_state.estimate_openness(l_crop, out_l);
    };

    float r_both_open = 0.0f, l_both_open = 0.0f;
    eval_sample("eyes_both_open.jpg", r_both_open, l_both_open);
    std::cout << "[TDD Signal Separation] eyes_both_open.jpg -> Right: " << r_both_open << " | Left: " << l_both_open << "\n";

    float r_both_wink = 0.0f, l_both_wink = 0.0f;
    eval_sample("eyes_both_wink.jpg", r_both_wink, l_both_wink);
    std::cout << "[TDD Signal Separation] eyes_both_wink.jpg -> Right: " << r_both_wink << " | Left: " << l_both_wink << "\n";

    float r_lwink = 0.0f, l_lwink = 0.0f;
    eval_sample("eyes_anatomical_left_wink.jpg", r_lwink, l_lwink);
    std::cout << "[TDD Signal Separation] eyes_anatomical_left_wink.jpg -> Right: " << r_lwink << " | Left: " << l_lwink << "\n";

    float r_rwink = 0.0f, l_rwink = 0.0f;
    eval_sample("eyes_anatomical_right_wink.jpg", r_rwink, l_rwink);
    std::cout << "[TDD Signal Separation] eyes_anatomical_right_wink.jpg -> Right: " << r_rwink << " | Left: " << l_rwink << "\n";

    // Strict Domain & Separation Assertions
    CHECK(r_both_open >= 0.70f);
    CHECK(l_both_open >= 0.70f);

    CHECK(r_both_wink <= 0.20f);
    CHECK(l_both_wink <= 0.20f);

    CHECK(r_lwink >= 0.70f);
    CHECK(l_lwink <= 0.20f);

    CHECK(r_rwink <= 0.20f);
    CHECK(l_rwink >= 0.70f);

    CHECK((r_both_open - r_both_wink) >= 0.50f);
    CHECK((l_both_open - l_both_wink) >= 0.50f);
}

