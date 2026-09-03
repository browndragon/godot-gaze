#include "doctest.h"
#include "ort_landmark_model.hpp"
#include "ort_yunet_detector.hpp"
#include <fstream>
#include <vector>
#include <cmath>

#ifndef STB_IMAGE_IMPLEMENTATION_INCLUDED
#include "stb_image.h"
#endif

inline bool landmark_test_file_exists(const std::string &filename) {
    std::ifstream f(filename.c_str());
    return f.good();
}

struct LandmarkTestImage
{
    int width = 0;
    int height = 0;
    std::vector<unsigned char> data; // BGR format
};

inline LandmarkTestImage load_landmark_test_image(const std::string &filepath)
{
    LandmarkTestImage result;
    int width = 0, height = 0, channels = 0;
    unsigned char *data = stbi_load(filepath.c_str(), &width, &height, &channels, 3);
    if (!data)
    {
        return result;
    }
    result.width = width;
    result.height = height;
    result.data.resize(width * height * 3);

    for (int i = 0; i < width * height; ++i)
    {
        result.data[i * 3 + 0] = data[i * 3 + 2]; // B
        result.data[i * 3 + 1] = data[i * 3 + 1]; // G
        result.data[i * 3 + 2] = data[i * 3 + 0]; // R
    }
    stbi_image_free(data);
    return result;
}

TEST_CASE("ORT Landmark Model 35-Point Boundary Invariants and Face Landmarking")
{
    std::string lm_model_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    if (!landmark_test_file_exists(lm_model_path)) {
        lm_model_path = "../project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    }

    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    if (!landmark_test_file_exists(yunet_path)) {
        yunet_path = "../project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    }

    if (!landmark_test_file_exists(lm_model_path) || !landmark_test_file_exists(yunet_path)) {
        MESSAGE("Skipping Landmark Model test: Model files not found");
        return;
    }

    Gaze::ORTLandmarkModel lm_model(lm_model_path);
    REQUIRE(lm_model.initialize() == true);

    Gaze::ORTYuNetDetector detector(yunet_path);
    REQUIRE(detector.initialize() == true);

    // 1. Test synthetic 60x60 face crop normalized output invariants
    std::vector<uint8_t> dummy_crop(60 * 60 * 3, 128);
    std::vector<Gaze::GodotCameraImageVector2> norm_landmarks;
    bool ok_dummy = lm_model.extract_landmarks_norm(dummy_crop.data(), norm_landmarks);
    REQUIRE(ok_dummy == true);
    REQUIRE(norm_landmarks.size() == 35);

    for (size_t i = 0; i < norm_landmarks.size(); ++i) {
        CHECK(norm_landmarks[i].x >= 0.0f);
        CHECK(norm_landmarks[i].x <= 1.0f);
        CHECK(norm_landmarks[i].y >= 0.0f);
        CHECK(norm_landmarks[i].y <= 1.0f);
    }

    // 2. Test Real Face Image: self_center.jpg
    std::string img_path = "tests/resources/self_center.jpg";
    if (!landmark_test_file_exists(img_path)) {
        img_path = "../tests/resources/self_center.jpg";
    }

    LandmarkTestImage img = load_landmark_test_image(img_path);
    REQUIRE(img.data.size() > 0);

    Gaze::Frame frame;
    frame.width = img.width;
    frame.height = img.height;
    frame.data = img.data.data();

    Gaze::YuNetResult yunet_res;
    bool det_ok = detector.process_frame(frame, yunet_res);
    REQUIRE(det_ok == true);
    REQUIRE(yunet_res.face_detected == true);

    Gaze::GazeRect face_bbox(yunet_res.roi_x, yunet_res.roi_y, yunet_res.roi_w, yunet_res.roi_h);
    std::vector<Gaze::GodotCameraImageVector2> landmarks_35_px;
    bool lm_ok = lm_model.extract_landmarks(frame.data, frame.width, frame.height, face_bbox, landmarks_35_px);
    REQUIRE(lm_ok == true);
    REQUIRE(landmarks_35_px.size() == 35);

    // Compute Eye Centers from 35-point OpenVINO ADAS Landmark Topology:
    // Pt 0: Right Eye Inner Corner, Pt 1: Right Eye Outer Corner (Viewer's Left)
    // Pt 2: Left Eye Inner Corner,  Pt 3: Left Eye Outer Corner  (Viewer's Right)
    // Pt 5: Nose Tip
    // Pt 8: Mouth Right Corner, Pt 9: Mouth Left Corner
    // Pt 26: Chin Center
    float r_eye_x = (landmarks_35_px[0].x + landmarks_35_px[1].x) * 0.5f;
    float r_eye_y = (landmarks_35_px[0].y + landmarks_35_px[1].y) * 0.5f;

    float l_eye_x = (landmarks_35_px[2].x + landmarks_35_px[3].x) * 0.5f;
    float l_eye_y = (landmarks_35_px[2].y + landmarks_35_px[3].y) * 0.5f;

    Gaze::GodotCameraImageVector2 nose_tip = landmarks_35_px[5];
    Gaze::GodotCameraImageVector2 chin = landmarks_35_px[26];

    // Assert that Right Eye (Viewer Left) is horizontally to the left of Left Eye (Viewer Right)
    CHECK(r_eye_x < l_eye_x);
    // Assert eye centers are within expected pixel bounds for self_center.jpg
    CHECK(r_eye_x == doctest::Approx(675.0f).epsilon(0.08));
    CHECK(l_eye_x == doctest::Approx(830.0f).epsilon(0.08));
    // Assert nose tip is below the eye level vertically
    CHECK(nose_tip.y > r_eye_y);
    CHECK(nose_tip.y > l_eye_y);
    // Assert chin is below the nose tip vertically
    CHECK(chin.y > nose_tip.y);
}

TEST_CASE("ORT Landmark Model Full Benchmark Image Suite Invariants")
{
    std::string lm_model_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    if (!landmark_test_file_exists(lm_model_path)) {
        lm_model_path = "../project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    }

    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    if (!landmark_test_file_exists(yunet_path)) {
        yunet_path = "../project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    }

    if (!landmark_test_file_exists(lm_model_path) || !landmark_test_file_exists(yunet_path)) {
        MESSAGE("Skipping Landmark Benchmark Suite: Model files not found");
        return;
    }

    Gaze::ORTLandmarkModel lm_model(lm_model_path);
    REQUIRE(lm_model.initialize() == true);

    Gaze::ORTYuNetDetector detector(yunet_path);
    REQUIRE(detector.initialize() == true);

    struct SuiteItem {
        std::string filename;
        float roll_hint_rad;
        float exp_rx, exp_ry;
        float exp_lx, exp_ly;
        float exp_nose_x, exp_nose_y;
        float exp_chin_x, exp_chin_y;
    };

    std::vector<SuiteItem> suite = {
        {"self_center.jpg", 0.0f, 668.6f, 400.0f, 826.1f, 396.9f, 748.0f, 517.8f, 747.9f, 678.6f},
        {"self_center2.jpg", 0.0f, 670.2f, 485.4f, 838.1f, 484.5f, 756.2f, 610.4f, 755.7f, 787.1f},
        {"self_left_left.jpg", 0.0f, 775.3f, 384.7f, 928.7f, 382.6f, 872.3f, 498.2f, 855.4f, 670.5f},
        {"self_right_right.jpg", 0.0f, 599.4f, 396.4f, 759.0f, 401.9f, 656.4f, 525.4f, 674.4f, 704.5f},
        {"self_top_top.jpg", 0.0f, 688.7f, 371.4f, 847.1f, 366.9f, 773.2f, 483.2f, 778.5f, 652.8f},
        {"self_down_down.jpg", 0.0f, 698.7f, 435.2f, 860.3f, 431.7f, 783.9f, 554.1f, 785.3f, 708.6f},
    };

    for (const auto &item : suite) {
        std::string img_path = "tests/resources/" + item.filename;
        if (!landmark_test_file_exists(img_path)) {
            img_path = "../tests/resources/" + item.filename;
        }

        LandmarkTestImage img = load_landmark_test_image(img_path);
        if (img.data.empty()) continue;

        Gaze::Frame frame;
        frame.width = img.width;
        frame.height = img.height;
        frame.data = img.data.data();

        Gaze::YuNetResult yunet_res;
        bool det_ok = detector.process_frame(frame, yunet_res, item.roll_hint_rad);
        CHECK_MESSAGE(det_ok == true, "Face detection failed on: ", item.filename);
        CHECK_MESSAGE(yunet_res.face_detected == true, "No face found in: ", item.filename);

        Gaze::GazeRect face_bbox(yunet_res.roi_x, yunet_res.roi_y, yunet_res.roi_w, yunet_res.roi_h);
        std::vector<Gaze::GodotCameraImageVector2> landmarks_35;
        bool lm_ok = lm_model.extract_landmarks(frame.data, frame.width, frame.height, face_bbox, landmarks_35, item.roll_hint_rad);
        CHECK_MESSAGE(lm_ok == true, "Landmark extraction failed on: ", item.filename);
        CHECK_MESSAGE(landmarks_35.size() == 35, "Expected 35 landmarks for: ", item.filename);

        if (landmarks_35.size() == 35) {
            float r_x = (landmarks_35[0].x + landmarks_35[1].x) * 0.5f;
            float r_y = (landmarks_35[0].y + landmarks_35[1].y) * 0.5f;
            float l_x = (landmarks_35[2].x + landmarks_35[3].x) * 0.5f;
            float l_y = (landmarks_35[2].y + landmarks_35[3].y) * 0.5f;

            float dx = l_x - r_x;
            float dy = l_y - r_y;
            float ipd_px = std::sqrt(dx * dx + dy * dy);

            // Physiological IPD invariant: Eye distance must be positive and within reasonable bounds
            CHECK_MESSAGE(ipd_px > 50.0f, "IPD too small in: ", item.filename);
            CHECK_MESSAGE(ipd_px < 350.0f, "IPD too large in: ", item.filename);

            // Assert exact empirical coordinates (within 4% tolerance) to prevent regressions
            CHECK_MESSAGE(r_x == doctest::Approx(item.exp_rx).epsilon(0.04), "Right Eye X regression in: ", item.filename);
            CHECK_MESSAGE(r_y == doctest::Approx(item.exp_ry).epsilon(0.04), "Right Eye Y regression in: ", item.filename);
            CHECK_MESSAGE(l_x == doctest::Approx(item.exp_lx).epsilon(0.04), "Left Eye X regression in: ", item.filename);
            CHECK_MESSAGE(l_y == doctest::Approx(item.exp_ly).epsilon(0.04), "Left Eye Y regression in: ", item.filename);
            CHECK_MESSAGE(landmarks_35[5].x == doctest::Approx(item.exp_nose_x).epsilon(0.04), "Nose Tip X regression in: ", item.filename);
            CHECK_MESSAGE(landmarks_35[5].y == doctest::Approx(item.exp_nose_y).epsilon(0.04), "Nose Tip Y regression in: ", item.filename);
            CHECK_MESSAGE(landmarks_35[26].x == doctest::Approx(item.exp_chin_x).epsilon(0.04), "Chin X regression in: ", item.filename);
            CHECK_MESSAGE(landmarks_35[26].y == doctest::Approx(item.exp_chin_y).epsilon(0.04), "Chin Y regression in: ", item.filename);

            // All 35 points must lie inside the camera frame bounds
            for (size_t i = 0; i < 35; ++i) {
                CHECK_MESSAGE(landmarks_35[i].x >= 0.0f, "Landmark X out of frame in: ", item.filename);
                CHECK_MESSAGE(landmarks_35[i].x <= static_cast<float>(img.width), "Landmark X out of frame in: ", item.filename);
                CHECK_MESSAGE(landmarks_35[i].y >= 0.0f, "Landmark Y out of frame in: ", item.filename);
                CHECK_MESSAGE(landmarks_35[i].y <= static_cast<float>(img.height), "Landmark Y out of frame in: ", item.filename);
            }
        }
    }
}

