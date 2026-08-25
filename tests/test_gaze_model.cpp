#include "doctest.h"
#include "ort_gaze_model.hpp"
#include "ort_landmark_model.hpp"
#include "ort_yunet_detector.hpp"
#include "ort_eye_state_model.hpp"
#include "projection_engine.hpp"
#include "camera_placement.hpp"
#include "opencv_space_conversions.hpp"
#include "face_model_geometry.hpp"
#include "pnp_solver.hpp"
#include "math_defs.hpp"
#include "test_utils.hpp"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>

using GazeTest::file_exists;
using GazeTest::load_test_image;
using GazeTest::extract_dense_eye_crops_60x60;
using LoadedImage = GazeTest::TestImage;
#define load_image load_test_image

TEST_CASE("Phase 5: OpenVINO Gaze Estimation on Benchmark Suite")
{
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string lm_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";

    if (!file_exists(yunet_path)) yunet_path = "../" + yunet_path;
    if (!file_exists(lm_path)) lm_path = "../" + lm_path;
    if (!file_exists(gaze_path)) gaze_path = "../" + gaze_path;

    REQUIRE(file_exists(yunet_path));
    REQUIRE(file_exists(lm_path));
    REQUIRE(file_exists(gaze_path));

    Gaze::ORTYuNetDetector detector(yunet_path);
    REQUIRE(detector.initialize() == true);

    Gaze::ORTLandmarkModel lm_model(lm_path);
    REQUIRE(lm_model.initialize() == true);

    Gaze::ORTGazeModel gaze_model(gaze_path);
    REQUIRE(gaze_model.initialize() == true);

    auto model_35pt = Gaze::FaceModelGeometry::get_canonical_35pt_model_points();

    auto process_image = [&](const std::string &filename, Gaze::GazeVector3 &out_gaze_dir) -> bool {
        std::string path = "tests/resources/" + filename;
        if (!file_exists(path)) path = "../tests/resources/" + filename;

        LoadedImage img = load_image(path);
        if (img.data.empty()) return false;

        Gaze::Frame frame;
        frame.width = img.width;
        frame.height = img.height;
        frame.data = img.data.data();

        Gaze::YuNetResult det_res;
        bool det_ok = detector.process_frame(frame, det_res, 0.0f);
        if (!det_ok || !det_res.face_detected) return false;

        Gaze::GazeRect bbox(det_res.roi_x, det_res.roi_y, det_res.roi_w, det_res.roi_h);
        std::vector<Gaze::GazeVector2> landmarks;
        bool lm_ok = lm_model.extract_landmarks(frame.data, frame.width, frame.height, bbox, landmarks, 0.0f);
        if (!lm_ok || landmarks.size() != 35) return false;

        double focal = static_cast<double>(frame.width);
        double cx = frame.width * 0.5;
        double cy = frame.height * 0.5;
        Gaze::GazeVector3 rvec(0.0f, 0.0f, 0.0f);
        Gaze::GazeVector3 tvec(0.0f, 0.0f, 600.0f);
        bool pnp_ok = Gaze::solve_pnp_lm(model_35pt, landmarks, focal, focal, cx, cy, rvec, tvec, false);
        if (!pnp_ok) return false;

        Gaze::EyeCrops crops;
        crops.face_detected = true;
        crops.head_pose_translation = tvec;
        crops.head_pose_rotation = rvec;

        extract_dense_eye_crops_60x60(frame.data, frame.width, frame.height, landmarks, crops.right_eye_data, crops.left_eye_data, 1.5f);

        Gaze::GazeVector3 raw_gaze;
        if (!gaze_model.estimate_raw_gaze(crops, raw_gaze)) return false;
        out_gaze_dir = Gaze::CoordinateConversions::ONNX_GAZE_TO_GODOT_CAM.multiply_vector(raw_gaze);
        return true;
    };

    Gaze::GazeVector3 gaze_center, gaze_left, gaze_right, gaze_noseleft_eyesright;

    bool ok_center = process_image("self_center.jpg", gaze_center);
    REQUIRE(ok_center);
    std::cout << "[Gaze Suite] self_center.jpg -> Gaze Vector: (" << gaze_center.x << ", " << gaze_center.y << ", " << gaze_center.z << ")\n";

    // 1. Dominant camera forward (+Z) gaze invariant in Godot space
    double norm_c = std::sqrt(gaze_center.x * gaze_center.x + gaze_center.y * gaze_center.y + gaze_center.z * gaze_center.z);
    CHECK(norm_c == doctest::Approx(1.0).epsilon(1e-3));
    CHECK(gaze_center.z > 0.0f); // points towards screen plane

    bool ok_left = process_image("self_left_left.jpg", gaze_left);
    REQUIRE(ok_left);
    std::cout << "[Gaze Suite] self_left_left.jpg -> Gaze Vector: (" << gaze_left.x << ", " << gaze_left.y << ", " << gaze_left.z << ")\n";

    bool ok_right = process_image("self_right_right.jpg", gaze_right);
    REQUIRE(ok_right);
    std::cout << "[Gaze Suite] self_right_right.jpg -> Gaze Vector: (" << gaze_right.x << ", " << gaze_right.y << ", " << gaze_right.z << ")\n";

    bool ok_nl_er = process_image("self_noseleft_eyesright.jpg", gaze_noseleft_eyesright);
    REQUIRE(ok_nl_er);
    std::cout << "[Gaze Suite] self_noseleft_eyesright.jpg -> Gaze Vector: (" << gaze_noseleft_eyesright.x << ", " << gaze_noseleft_eyesright.y << ", " << gaze_noseleft_eyesright.z << ")\n";

    // Raw OpenVINO ADAS-0002 Output: +x = subject left (screen left), -x = subject right (screen right)
    CHECK(gaze_left.x > gaze_right.x);
    // Decoupled: Eyes looking subject right must produce negative X component in model frame
    CHECK(gaze_noseleft_eyesright.x < 0.0f);
}

TEST_CASE("Physical End-to-End Screen Gaze Directional Invariants")
{
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string lm_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    std::string eye_state_path = "project/addons/godot-gaze/models/open_closed_eye.ort";
    std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";

    Gaze::ORTYuNetDetector detector(yunet_path);
    REQUIRE(detector.initialize());

    Gaze::ORTLandmarkModel lm_model(lm_path);
    REQUIRE(lm_model.initialize());

    Gaze::ORTEyeStateModel eye_model(eye_state_path);
    REQUIRE(eye_model.initialize());

    Gaze::ORTGazeModel gaze_model(gaze_path);
    REQUIRE(gaze_model.initialize());

    // Standard Display: 1440x960 px, 300x200 mm, camera at top center (offset 0, 0, 0)
    Gaze::ProjectionEngine engine;
    engine.set_screen_size_pixels(Gaze::GazeVector2(1440.0, 960.0));
    engine.set_screen_size_mm(Gaze::GazeVector2(300.0, 200.0));
    engine.set_camera_placement(Gaze::CameraPlacement(Gaze::GazeVector3(0.0, 0.0, 0.0), 0.0));

    auto model_35 = Gaze::get_canonical_35pt_face_model();

    auto process_fixture = [&](const std::string& fixture_name,
                               Gaze::GazeVector2& out_nose_px,
                               Gaze::GazeVector2& out_gaze_px,
                               Gaze::GazeVector3& out_head_euler,
                               Gaze::GazeVector3& out_gaze_dir) -> bool {
        LoadedImage img = load_image("tests/resources/" + fixture_name);
        if (img.data.empty()) return false;

        Gaze::Frame frame{img.width, img.height, img.data.data(), 0};
        Gaze::YuNetResult yunet_res;
        if (!detector.process_frame(frame, yunet_res, 0.0f) || !yunet_res.face_detected) {
            std::cout << "[DEBUG process_fixture] Face detection failed for " << fixture_name << "\n";
            return false;
        }

        Gaze::GazeRect bbox(yunet_res.roi_x, yunet_res.roi_y, yunet_res.roi_w, yunet_res.roi_h);
        std::vector<Gaze::GazeVector2> landmarks_35;
        if (!lm_model.extract_landmarks(frame.data, frame.width, frame.height, bbox, landmarks_35, 0.0f) || landmarks_35.size() != 35) {
            std::cout << "[DEBUG process_fixture] LM model failed for " << fixture_name << "\n";
            return false;
        }

        // Solve head pose
        double focal = static_cast<double>(frame.width);
        double cx = frame.width * 0.5;
        double cy = frame.height * 0.5;
        Gaze::GazeVector3 rvec(0.0f, 0.0f, 0.0f), tvec(0.0f, 0.0f, 600.0f);
        if (!Gaze::solve_pnp_lm(model_35, landmarks_35, focal, focal, cx, cy, rvec, tvec, false)) {
            return false;
        }

        // Compute Head Transform in Godot Camera Space
        Gaze::GazeTransform3D head_xform = Gaze::CoordinateConversions::opencv_pose_to_godot_camera_transform(tvec, rvec);
        Gaze::GazeVector3 head_fwd = -head_xform.basis.z.normalized();
        out_head_euler = head_xform.basis.get_euler_deg();

        // Project Nose Gaze
        if (!engine.project_gaze(head_xform.origin, head_fwd, out_nose_px)) {
            return false;
        }

        // Extract eye crops and run gaze model
        Gaze::EyeCrops crops;
        crops.face_detected = true;
        crops.head_pose_translation = tvec;
        crops.head_pose_rotation = rvec;
        extract_dense_eye_crops_60x60(frame.data, frame.width, frame.height, landmarks_35, crops.right_eye_data, crops.left_eye_data, 1.5f);

        Gaze::GazeVector3 raw_gaze_dir_cam;
        if (!gaze_model.estimate_raw_gaze(crops, raw_gaze_dir_cam)) {
            return false;
        }

        // Apply ONNX_GAZE_TO_GODOT_CAM basis to convert raw ONNX gaze vector to Godot camera space
        Gaze::GazeVector3 gaze_dir_godot = Gaze::CoordinateConversions::ONNX_GAZE_TO_GODOT_CAM.multiply_vector(raw_gaze_dir_cam);
        out_gaze_dir = gaze_dir_godot;

        // Project Eye Gaze
        if (!engine.project_gaze(head_xform.origin, gaze_dir_godot, out_gaze_px)) {
            return false;
        }

        return true;
    };

    Gaze::GazeVector2 nose_center, gaze_center, nose_top_down, gaze_top_down, nose_top, gaze_top, nose_down, gaze_down, nose_left, gaze_left, nose_right, gaze_right, nose_nl_er, gaze_nl_er;
    Gaze::GazeVector3 head_rot_center, head_rot_top_down, head_rot_top, head_rot_down, head_rot_left, head_rot_right, head_rot_nl_er;
    Gaze::GazeVector3 gaze_dir_center, gaze_dir_top_down, gaze_dir_top, gaze_dir_down, gaze_dir_left, gaze_dir_right, gaze_dir_nl_er;

    REQUIRE(process_fixture("self_center.jpg", nose_center, gaze_center, head_rot_center, gaze_dir_center));
    REQUIRE(process_fixture("self_top_top.jpg", nose_top, gaze_top, head_rot_top, gaze_dir_top));
    REQUIRE(process_fixture("self_down_down.jpg", nose_down, gaze_down, head_rot_down, gaze_dir_down));
    REQUIRE(process_fixture("self_nosetop_eyesdown.jpg", nose_top_down, gaze_top_down, head_rot_top_down, gaze_dir_top_down));
    REQUIRE(process_fixture("self_left_left.jpg", nose_left, gaze_left, head_rot_left, gaze_dir_left));
    REQUIRE(process_fixture("self_right_right.jpg", nose_right, gaze_right, head_rot_right, gaze_dir_right));
    REQUIRE(process_fixture("self_noseleft_eyesright.jpg", nose_nl_er, gaze_nl_er, head_rot_nl_er, gaze_dir_nl_er));

    std::cout << "\n=================== END-TO-END INVARIANT MEASUREMENTS ===================\n";
    std::cout << "self_center.jpg             -> Nose: (" << nose_center.x << ", " << nose_center.y << ") | Gaze: (" << gaze_center.x << ", " << gaze_center.y << ")\n";
    std::cout << "self_top_top.jpg            -> Nose: (" << nose_top.x << ", " << nose_top.y << ") | Gaze: (" << gaze_top.x << ", " << gaze_top.y << ")\n";
    std::cout << "self_down_down.jpg          -> Nose: (" << nose_down.x << ", " << nose_down.y << ") | Gaze: (" << gaze_down.x << ", " << gaze_down.y << ")\n";
    std::cout << "self_nosetop_eyesdown.jpg   -> Nose: (" << nose_top_down.x << ", " << nose_top_down.y << ") | Gaze: (" << gaze_top_down.x << ", " << gaze_top_down.y << ")\n";
    std::cout << "self_left_left.jpg          -> Nose: (" << nose_left.x << ", " << nose_left.y << ") | Gaze: (" << gaze_left.x << ", " << gaze_left.y << ")\n";
    std::cout << "self_right_right.jpg        -> Nose: (" << nose_right.x << ", " << nose_right.y << ") | Gaze: (" << gaze_right.x << ", " << gaze_right.y << ")\n";
    std::cout << "self_noseleft_eyesright.jpg -> Nose: (" << nose_nl_er.x << ", " << nose_nl_er.y << ") | Gaze: (" << gaze_nl_er.x << ", " << gaze_nl_er.y << ")\n";
    std::cout << "=========================================================================\n\n";

    // 1. INDEPENDENT FRAME DOMAIN BOUNDS:
    // self_center.jpg: Centered gaze & nose
    CHECK(nose_center.x > 400.0f);
    CHECK(nose_center.x < 1100.0f);
    CHECK(nose_center.y > 0.0f);
    CHECK(nose_center.y < 800.0f);

    // self_top_top.jpg: Pitch >= -2 deg, projects high on screen (nose y < 450, gaze y < 400)
    CHECK(head_rot_top.x >= -2.0f);
    CHECK(nose_top.y < 450.0f);
    CHECK(gaze_top.y < 400.0f);

    // self_down_down.jpg: Pitch <= -3 deg, projects lower on screen (nose y > 650)
    CHECK(head_rot_down.x <= -3.0f);
    CHECK(nose_down.y > 650.0f);

    // self_nosetop_eyesdown.jpg: Head pitched up (pitch >= 10 deg), nose projected very high (y < 0)
    CHECK(head_rot_top_down.x >= 10.0f);
    CHECK(nose_top_down.y < 0.0f);

    // self_left_left.jpg: Yaw <= -10 deg (facing viewer left), nose projected left (x < 600)
    CHECK(head_rot_left.y <= -10.0f);
    CHECK(nose_left.x < 600.0f);
    CHECK(gaze_left.x < 600.0f);

    // self_right_right.jpg: Yaw >= 10 deg (facing viewer right), nose projected right (x > 800)
    CHECK(head_rot_right.y >= 10.0f);
    CHECK(nose_right.x > 800.0f);
    CHECK(gaze_right.x > 800.0f);

    // self_noseleft_eyesright.jpg: Head facing left (yaw <= -5 deg), gaze looking right (gaze x > 800)
    CHECK(head_rot_nl_er.y <= -5.0f);
    CHECK(nose_nl_er.x < 700.0f);
    CHECK(gaze_nl_er.x > 800.0f);

    // 2. DIRECTIONAL INVARIANTS:
    // Tilting head UP (top_top) must project higher on screen (smaller Y) than tilting head DOWN (down_down).
    CHECK(nose_top.y < nose_down.y - 30.0f);

    // Turning head to user's left (screen left, smaller X) must project left of center (< center.x);
    // Turning head to user's right (screen right, larger X) must project right of center (> center.x).
    CHECK(nose_left.x < nose_center.x - 50.0f);
    CHECK(nose_right.x > nose_center.x + 50.0f);

    // Gazing to user's left (screen left, smaller X) must project left of center (< center.x);
    // Gazing to user's right (screen right, larger X) must project right of center (> center.x).
    CHECK(gaze_left.x < gaze_center.x - 30.0f);
    CHECK(gaze_right.x > gaze_center.x + 30.0f);

    // Gazing UP (top_top) must project higher on screen (smaller Y) than gazing DOWN (down_down).
    CHECK(gaze_top.y < gaze_down.y - 20.0f);

    // On self_noseleft_eyesright.jpg:
    // - Head/Nose is facing user's left (screen left): nose_nl_er.x < nose_center.x
    // - Eyes are looking to user's right (screen right): gaze_nl_er.x > nose_nl_er.x + 50px
    CHECK(nose_nl_er.x < nose_center.x);
    CHECK(gaze_nl_er.x > nose_nl_er.x + 50.0f);

    // On self_nosetop_eyesdown.jpg:
    // - Eyes looking down must project below the upward-pointing nose
    CHECK(gaze_top_down.y > nose_top_down.y + 50.0f);
}

