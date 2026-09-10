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

TEST_CASE("Phase 3: 35-Point Landmark Canonical Geometry Validation")
{
    auto model_35 = Gaze::FaceModelGeometry::get_canonical_35pt_model_points();
    auto godot_35 = Gaze::FaceModelGeometry::get_canonical_godot_model_points();

    // 1. In OpenCV Model Space: Pts 0..1 (Image Left Eye) is at -X, Pts 2..3 (Image Right Eye) is at +X
    CHECK(model_35[0].x < 0.0f); // Image Left Eye Inner (Anatomical Right)
    CHECK(model_35[1].x < 0.0f); // Image Left Eye Outer
    CHECK(model_35[2].x > 0.0f); // Image Right Eye Inner (Anatomical Left)
    CHECK(model_35[3].x > 0.0f); // Image Right Eye Outer

    // 2. In Godot Face Space: Pts 0..1 (Anatomical Right Eye) is at +X, Pts 2..3 (Anatomical Left Eye) is at -X
    CHECK(godot_35[0].x > 0.0f);
    CHECK(godot_35[1].x > 0.0f);
    CHECK(godot_35[2].x < 0.0f);
    CHECK(godot_35[3].x < 0.0f);

    // 3. Eyebrows: Pts 12..14 (Image Left) at -X in OpenCV, Pts 15..17 (Image Right) at +X in OpenCV
    CHECK(model_35[12].x < 0.0f);
    CHECK(model_35[15].x > 0.0f);
}

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

    auto process_image = [&](const std::string &filename, Gaze::GodotCameraVector3 &out_gaze_dir) -> bool {
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
        std::vector<Gaze::GodotCameraImageVector2> landmarks;
        bool lm_ok = lm_model.extract_landmarks(frame.data, frame.width, frame.height, bbox, landmarks, 0.0f);
        if (!lm_ok || landmarks.size() != 35) return false;

        double focal = Gaze::calculate_default_focal_length(static_cast<double>(frame.width));
        double cx = frame.width * 0.5;
        double cy = frame.height * 0.5;
        Gaze::OpenCVCameraVector3 rvec(0.0, 0.0, 0.0);
        Gaze::OpenCVCameraVector3 tvec(0.0, 0.0, 600.0);
        bool pnp_ok = Gaze::SQPnPSolver::solve_rvec(model_35pt, landmarks, focal, focal, cx, cy, rvec, tvec);
        if (!pnp_ok) return false;

        Gaze::EyeCrops crops;
        crops.face_detected = true;
        crops.head_pose_translation = tvec;
        crops.head_pose_rotation = rvec;

        extract_dense_eye_crops_60x60(frame.data, frame.width, frame.height, landmarks, crops.right_eye_data, crops.left_eye_data, 1.0f);

        Gaze::OpenVINOGazeVector3 raw_gaze;
        if (!gaze_model.estimate_raw_gaze(crops, raw_gaze)) return false;
        std::cout << "[RAW OPENVINO] " << filename << " -> raw_gaze: (" << raw_gaze.x << ", " << raw_gaze.y << ", " << raw_gaze.z << ")\n";
        out_gaze_dir = Gaze::CoordinateConversions::to_godot_camera(raw_gaze);
        return true;
    };

    Gaze::GodotCameraVector3 gaze_center, gaze_left, gaze_right, gaze_noseleft_eyesright;

    bool ok_center = process_image("self_center.jpg", gaze_center);
    REQUIRE(ok_center);
    std::cout << "[Gaze Suite] self_center.jpg -> Gaze Vector: (" << gaze_center.x << ", " << gaze_center.y << ", " << gaze_center.z << ")\n";

    // 1. Dominant camera forward (+Z) gaze invariant in Godot space
    double norm_c = std::sqrt(gaze_center.x * gaze_center.x + gaze_center.y * gaze_center.y + gaze_center.z * gaze_center.z);
    CHECK(norm_c == doctest::Approx(1.0).epsilon(1e-3));
    CHECK(gaze_center.z > 0.0); // points towards screen plane

    bool ok_left = process_image("self_left_left.jpg", gaze_left);
    REQUIRE(ok_left);
    std::cout << "[Gaze Suite] self_left_left.jpg -> Gaze Vector: (" << gaze_left.x << ", " << gaze_left.y << ", " << gaze_left.z << ")\n";

    bool ok_right = process_image("self_right_right.jpg", gaze_right);
    REQUIRE(ok_right);
    std::cout << "[Gaze Suite] self_right_right.jpg -> Gaze Vector: (" << gaze_right.x << ", " << gaze_right.y << ", " << gaze_right.z << ")\n";

    bool ok_nl_er = process_image("self_noseleft_eyesright.jpg", gaze_noseleft_eyesright);
    REQUIRE(ok_nl_er);
    std::cout << "[Gaze Suite] self_noseleft_eyesright.jpg -> Gaze Vector: (" << gaze_noseleft_eyesright.x << ", " << gaze_noseleft_eyesright.y << ", " << gaze_noseleft_eyesright.z << ")\n";

    // Godot Camera Space: +x = viewer left (camera right / display left), -x = viewer right (camera left / display right)
    // Decoupled: Eyes looking display right must produce negative X component in Godot Camera Space
    CHECK(gaze_noseleft_eyesright.x < 0.0);
}

TEST_CASE("Eye Crop Routing Empirical Experiment: Standard vs Swapped")
{
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    std::string lm_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";

    if (!file_exists(yunet_path)) yunet_path = "../" + yunet_path;
    if (!file_exists(lm_path)) lm_path = "../" + lm_path;
    if (!file_exists(gaze_path)) gaze_path = "../" + gaze_path;

    Gaze::ORTYuNetDetector detector(yunet_path);
    REQUIRE(detector.initialize());
    Gaze::ORTLandmarkModel lm_model(lm_path);
    REQUIRE(lm_model.initialize());
    Gaze::ORTGazeModel gaze_model(gaze_path);
    REQUIRE(gaze_model.initialize());

    auto model_35 = Gaze::FaceModelGeometry::get_canonical_35pt_model_points();

    struct TestCase {
        std::string filename;
        std::string description;
    };

    std::vector<TestCase> test_cases = {
        {"self_center.jpg", "Center"},
        {"self_left_left.jpg", "Head Left, Eyes Left (Screen Left)"},
        {"self_right_right.jpg", "Head Right, Eyes Right (Screen Right)"},
        {"self_noseleft_eyesright.jpg", "Head Left, Eyes Right (Screen Right)"},
        {"self_noseright_eyesleft.jpg", "Head Right, Eyes Left (Screen Left)"},
    };

    std::cout << "\n================================================================================\n";
    std::cout << "           EYE CROP ROUTING EXPERIMENT (STANDARD vs SWAPPED)\n";
    std::cout << "================================================================================\n";

    for (const auto& tc : test_cases) {
        LoadedImage img = load_image("tests/resources/" + tc.filename);
        if (img.data.empty()) continue;

        Gaze::Frame frame{img.width, img.height, img.data.data(), 0};
        Gaze::YuNetResult det_res;
        if (!detector.process_frame(frame, det_res, 0.0f) || !det_res.face_detected) continue;

        Gaze::GazeRect bbox(det_res.roi_x, det_res.roi_y, det_res.roi_w, det_res.roi_h);
        std::vector<Gaze::GodotCameraImageVector2> landmarks;
        if (!lm_model.extract_landmarks(frame.data, frame.width, frame.height, bbox, landmarks, 0.0f) || landmarks.size() != 35) continue;

        double focal = Gaze::calculate_default_focal_length(static_cast<double>(frame.width));
        double cx = frame.width * 0.5;
        double cy = frame.height * 0.5;
        Gaze::OpenCVCameraVector3 rvec(0.0, 0.0, 0.0), tvec(0.0, 0.0, 600.0);
        if (!Gaze::SQPnPSolver::solve_rvec(model_35, landmarks, focal, focal, cx, cy, rvec, tvec)) continue;

        // Extract crops: out_right_crop (pts 0..1, image-left), out_left_crop (pts 2..3, image-right)
        std::vector<uint8_t> crop_img_left(60 * 60 * 3);  // anatomical right
        std::vector<uint8_t> crop_img_right(60 * 60 * 3); // anatomical left
        extract_dense_eye_crops_60x60(frame.data, frame.width, frame.height, landmarks, crop_img_left.data(), crop_img_right.data(), 1.0f);

        // 1. Standard Routing:
        // crops.left_eye_data = crop_img_right (anatomical left)
        // crops.right_eye_data = crop_img_left (anatomical right)
        Gaze::EyeCrops crops_std;
        crops_std.face_detected = true;
        crops_std.head_pose_translation = tvec;
        crops_std.head_pose_rotation = rvec;
        std::memcpy(crops_std.left_eye_data, crop_img_right.data(), 60*60*3);
        std::memcpy(crops_std.right_eye_data, crop_img_left.data(), 60*60*3);

        Gaze::OpenVINOGazeVector3 raw_gaze_std;
        gaze_model.estimate_raw_gaze(crops_std, raw_gaze_std);
        Gaze::GodotCameraVector3 gaze_godot_std = Gaze::CoordinateConversions::to_godot_camera(raw_gaze_std);

        // 2. Swapped Routing (left_eye_image = image-left crop, right_eye_image = image-right crop)
        Gaze::EyeCrops crops_swapped;
        crops_swapped.face_detected = true;
        crops_swapped.head_pose_translation = tvec;
        crops_swapped.head_pose_rotation = rvec;
        std::memcpy(crops_swapped.left_eye_data, crop_img_left.data(), 60*60*3);
        std::memcpy(crops_swapped.right_eye_data, crop_img_right.data(), 60*60*3);

        Gaze::OpenVINOGazeVector3 raw_gaze_swapped;
        gaze_model.estimate_raw_gaze(crops_swapped, raw_gaze_swapped);
        Gaze::GodotCameraVector3 gaze_godot_swapped = Gaze::CoordinateConversions::to_godot_camera(raw_gaze_swapped);

        Gaze::SpacedVector3<Gaze::Space::OpenCVCamera> ov_angles = Gaze::CoordinateConversions::opencv_head_pose_to_openvino_angles_deg(rvec);

        std::cout << "Fixture: " << tc.filename << " (" << tc.description << ")\n";
        std::cout << "  Head Pose [Yaw, Pitch, Roll]: (" << ov_angles.x << ", " << ov_angles.y << ", " << ov_angles.z << ") deg\n";
        std::cout << "  Standard (left=anatomical_left, right=anatomical_right) -> Raw: (" 
                  << std::fixed << std::setprecision(3) << raw_gaze_std.x << ", " << raw_gaze_std.y << ", " << raw_gaze_std.z 
                  << ") | Godot Dir: (" << gaze_godot_std.x << ", " << gaze_godot_std.y << ", " << gaze_godot_std.z << ")\n";
        std::cout << "  Swapped  (left=anatomical_right, right=anatomical_left) -> Raw: (" 
                  << std::fixed << std::setprecision(3) << raw_gaze_swapped.x << ", " << raw_gaze_swapped.y << ", " << raw_gaze_swapped.z 
                  << ") | Godot Dir: (" << gaze_godot_swapped.x << ", " << gaze_godot_swapped.y << ", " << gaze_godot_swapped.z << ")\n\n";
    }
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

    // Standard Display: 1440x960 px, 300x200 mm, camera at top center (offset 0, 0 mm, 0)
    Gaze::ProjectionEngine engine;
    engine.set_screen_size_pixels(Gaze::GodotDisplayVector2(1440.0, 960.0));
    engine.set_screen_size_mm(Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(300.0, 200.0));
    engine.set_camera_placement(Gaze::CameraPlacement(Gaze::GodotCameraVector3(0.0, 0.0, 0.0), 0.0));

    auto model_35 = Gaze::FaceModelGeometry::get_canonical_35pt_model_points();

    auto process_fixture = [&](const std::string& fixture_name,
                               Gaze::GodotDisplayVector2& out_nose_px,
                               Gaze::GodotDisplayVector2& out_gaze_px,
                               Gaze::SpacedVector3<Gaze::Space::GodotCameraEuler>& out_head_euler,
                               Gaze::GodotCameraVector3& out_gaze_dir) -> bool {
        LoadedImage img = load_image("tests/resources/" + fixture_name);
        if (img.data.empty()) return false;

        Gaze::Frame frame{img.width, img.height, img.data.data(), 0};
        Gaze::YuNetResult yunet_res;
        if (!detector.process_frame(frame, yunet_res, 0.0f) || !yunet_res.face_detected) {
            std::cout << "[DEBUG process_fixture] Face detection failed for " << fixture_name << "\n";
            return false;
        }

        Gaze::GazeRect bbox(yunet_res.roi_x, yunet_res.roi_y, yunet_res.roi_w, yunet_res.roi_h);
        std::vector<Gaze::GodotCameraImageVector2> landmarks_35;
        if (!lm_model.extract_landmarks(frame.data, frame.width, frame.height, bbox, landmarks_35, 0.0f) || landmarks_35.size() != 35) {
            std::cout << "[DEBUG process_fixture] LM model failed for " << fixture_name << "\n";
            return false;
        }

        // Solve head pose
        double focal = Gaze::calculate_default_focal_length(static_cast<double>(frame.width));
        double cx = frame.width * 0.5;
        double cy = frame.height * 0.5;
        Gaze::OpenCVCameraVector3 rvec(0.0, 0.0, 0.0), tvec(0.0, 0.0, 600.0);
        if (!Gaze::SQPnPSolver::solve_rvec(model_35, landmarks_35, focal, focal, cx, cy, rvec, tvec)) {
            std::cout << "[DEBUG process_fixture] SQPnPSolver::solve_rvec failed for " << fixture_name << "\n";
            return false;
        }

        // Compute Head Transform in Godot Camera Space
        Gaze::GodotFaceTransform3D head_xform = Gaze::CoordinateConversions::opencv_pose_to_godot_camera_transform(tvec, rvec);
        Gaze::GodotCameraVector3 head_fwd = -head_xform.basis.z.normalized();
        out_head_euler = head_xform.basis.get_euler_rad() * Gaze::RAD_TO_DEG;

        // Project Nose Gaze
        Gaze::GodotDisplayVector2 nose_px_spaced;
        if (!engine.project_gaze(head_xform.origin, head_fwd, nose_px_spaced)) {
            std::cout << "[DEBUG process_fixture] project_gaze nose failed for " << fixture_name << "\n";
            return false;
        }
        out_nose_px = nose_px_spaced;

        // Extract eye crops and run gaze model
        Gaze::EyeCrops crops;
        crops.face_detected = true;
        crops.head_pose_translation = tvec;
        crops.head_pose_rotation = rvec;
        extract_dense_eye_crops_60x60(frame.data, frame.width, frame.height, landmarks_35, crops.right_eye_data, crops.left_eye_data, 1.5f);

        Gaze::OpenVINOGazeVector3 raw_gaze_dir_cam;
        if (!gaze_model.estimate_raw_gaze(crops, raw_gaze_dir_cam)) {
            std::cout << "[DEBUG process_fixture] estimate_raw_gaze failed for " << fixture_name << "\n";
            return false;
        }

        // Convert raw ONNX gaze vector to Godot camera space
        Gaze::GodotCameraVector3 gaze_dir_godot = Gaze::CoordinateConversions::to_godot_camera(raw_gaze_dir_cam);
        out_gaze_dir = gaze_dir_godot;

        // Project Eye Gaze from canonical anatomical eye midpoint
        Gaze::SpacedBasis<Gaze::Space::OpenCVFaceModel, Gaze::Space::OpenCVCamera> head_rot_cv = Gaze::rodrigues_to_basis<Gaze::Space::OpenCVFaceModel, Gaze::Space::OpenCVCamera>(rvec);
        Gaze::OpenCVCameraVector3 eye_mid_cv = head_rot_cv.transform(Gaze::OpenCVFaceVector3(0.0, -32.0, 0.0)) + tvec;
        Gaze::GodotCameraVector3 eye_orig_godot = Gaze::CoordinateConversions::to_godot_camera(eye_mid_cv);

        Gaze::GodotDisplayVector2 gaze_px_spaced;
        if (!engine.project_gaze(eye_orig_godot, gaze_dir_godot, gaze_px_spaced)) {
            std::cout << "[DEBUG process_fixture] project_gaze eye failed for " << fixture_name 
                      << " | origin=(" << eye_orig_godot.x << ", " << eye_orig_godot.y << ", " << eye_orig_godot.z << ")"
                      << " | dir=(" << gaze_dir_godot.x << ", " << gaze_dir_godot.y << ", " << gaze_dir_godot.z << ")\n";
            return false;
        }
        out_gaze_px = gaze_px_spaced;

        return true;
    };

    Gaze::GodotDisplayVector2 nose_center, gaze_center, nose_top_down, gaze_top_down, nose_top, gaze_top, nose_down, gaze_down, nose_left, gaze_left, nose_right, gaze_right, nose_nl_er, gaze_nl_er, nose_yr_rl, gaze_yr_rl;
    Gaze::SpacedVector3<Gaze::Space::GodotCameraEuler> head_rot_center, head_rot_top_down, head_rot_top, head_rot_down, head_rot_left, head_rot_right, head_rot_nl_er, head_rot_yr_rl;
    Gaze::GodotCameraVector3 gaze_dir_center, gaze_dir_top_down, gaze_dir_top, gaze_dir_down, gaze_dir_left, gaze_dir_right, gaze_dir_nl_er, gaze_dir_yr_rl;

    REQUIRE(process_fixture("self_center.jpg", nose_center, gaze_center, head_rot_center, gaze_dir_center));
    REQUIRE(process_fixture("self_top_top.jpg", nose_top, gaze_top, head_rot_top, gaze_dir_top));
    REQUIRE(process_fixture("self_down_down.jpg", nose_down, gaze_down, head_rot_down, gaze_dir_down));
    REQUIRE(process_fixture("self_nosetop_eyesdown.jpg", nose_top_down, gaze_top_down, head_rot_top_down, gaze_dir_top_down));
    REQUIRE(process_fixture("self_left_left.jpg", nose_left, gaze_left, head_rot_left, gaze_dir_left));
    REQUIRE(process_fixture("self_right_right.jpg", nose_right, gaze_right, head_rot_right, gaze_dir_right));
    REQUIRE(process_fixture("self_noseleft_eyesright.jpg", nose_nl_er, gaze_nl_er, head_rot_nl_er, gaze_dir_nl_er));
    REQUIRE(process_fixture("self_yaw_right_roll_left.jpg", nose_yr_rl, gaze_yr_rl, head_rot_yr_rl, gaze_dir_yr_rl));

    std::cout << "\n=================== END-TO-END INVARIANT MEASUREMENTS ===================\n";
    std::cout << "self_center.jpg             -> Nose: (" << nose_center.x << ", " << nose_center.y << ") | Gaze: (" << gaze_center.x << ", " << gaze_center.y << ") | GazeDir: (" << gaze_dir_center.x << ", " << gaze_dir_center.y << ", " << gaze_dir_center.z << ")\n";
    std::cout << "self_top_top.jpg            -> Nose: (" << nose_top.x << ", " << nose_top.y << ") | Gaze: (" << gaze_top.x << ", " << gaze_top.y << ") | GazeDir: (" << gaze_dir_top.x << ", " << gaze_dir_top.y << ", " << gaze_dir_top.z << ")\n";
    std::cout << "self_down_down.jpg          -> Nose: (" << nose_down.x << ", " << nose_down.y << ") | Gaze: (" << gaze_down.x << ", " << gaze_down.y << ") | GazeDir: (" << gaze_dir_down.x << ", " << gaze_dir_down.y << ", " << gaze_dir_down.z << ")\n";
    std::cout << "self_nosetop_eyesdown.jpg   -> Nose: (" << nose_top_down.x << ", " << nose_top_down.y << ") | Gaze: (" << gaze_top_down.x << ", " << gaze_top_down.y << ") | GazeDir: (" << gaze_dir_top_down.x << ", " << gaze_dir_top_down.y << ", " << gaze_dir_top_down.z << ")\n";
    std::cout << "self_left_left.jpg          -> Nose: (" << nose_left.x << ", " << nose_left.y << ") | Gaze: (" << gaze_left.x << ", " << gaze_left.y << ") | GazeDir: (" << gaze_dir_left.x << ", " << gaze_dir_left.y << ", " << gaze_dir_left.z << ")\n";
    std::cout << "self_right_right.jpg        -> Nose: (" << nose_right.x << ", " << nose_right.y << ") | Gaze: (" << gaze_right.x << ", " << gaze_right.y << ") | GazeDir: (" << gaze_dir_right.x << ", " << gaze_dir_right.y << ", " << gaze_dir_right.z << ")\n";
    std::cout << "self_noseleft_eyesright.jpg -> Nose: (" << nose_nl_er.x << ", " << nose_nl_er.y << ") | Gaze: (" << gaze_nl_er.x << ", " << gaze_nl_er.y << ") | GazeDir: (" << gaze_dir_nl_er.x << ", " << gaze_dir_nl_er.y << ", " << gaze_dir_nl_er.z << ")\n";
    std::cout << "self_yaw_right_roll_left.jpg-> Nose: (" << nose_yr_rl.x << ", " << nose_yr_rl.y << ") | Gaze: (" << gaze_yr_rl.x << ", " << gaze_yr_rl.y << ") | GazeDir: (" << gaze_dir_yr_rl.x << ", " << gaze_dir_yr_rl.y << ", " << gaze_dir_yr_rl.z << ")\n";
    std::cout << "=========================================================================\n\n";

    // 1. INDEPENDENT FRAME DOMAIN BOUNDS (Screen-Center Virtual Anchor: Y = 540 px center of 1080p screen):
    // self_center.jpg: Centered gaze & nose in central display region around (960, 540)
    CHECK(nose_center.x > 600.0f);
    CHECK(nose_center.x < 1000.0f);
    CHECK(nose_center.y > 500.0f);
    CHECK(nose_center.y < 850.0f);

    // self_top_top.jpg: Pitch near zero, projects above center (nose y in [300, 600], gaze y in [100, 400])
    CHECK(head_rot_top.x >= -2.0f);
    CHECK(head_rot_top.x <= 2.0f);
    CHECK(nose_top.y > 300.0f);
    CHECK(nose_top.y < 600.0f);
    CHECK(gaze_top.y > 100.0f);
    CHECK(gaze_top.y < 400.0f);

    // self_down_down.jpg: Pitch <= -3 deg, projects downward (nose y in [650, 1050])
    CHECK(head_rot_down.x <= -3.0f);
    CHECK(nose_down.y > 650.0f);
    CHECK(nose_down.y < 1050.0f);

    // self_nosetop_eyesdown.jpg: Head pitched up (pitch >= 10 deg), nose projected near or above screen top (y < 100)
    CHECK(head_rot_top_down.x >= 10.0f);
    CHECK(nose_top_down.y < 100.0f);

    // self_left_left.jpg: Yaw <= -10 deg (facing viewer left), nose projected left (x < 600)
    CHECK(head_rot_left.y <= -10.0f);
    CHECK(nose_left.x < 600.0f);

    // self_right_right.jpg: Yaw >= 10 deg (facing viewer right), nose projected right (x > 800)
    CHECK(head_rot_right.y >= 10.0f);
    CHECK(nose_right.x > 800.0f);

    // self_noseleft_eyesright.jpg: Head facing left (yaw <= -3 deg), eyes looking screen right (gaze x > 800)
    CHECK(head_rot_nl_er.y <= -3.0f);
    CHECK(nose_nl_er.x < 700.0f);
    CHECK(gaze_nl_er.x > 800.0f);

    // 2. DIRECTIONAL INVARIANTS:
    // Tilting head UP (top_top) must project higher on screen (smaller Y) than tilting head DOWN (down_down).
    CHECK(nose_top.y < nose_down.y - 200.0f);

    // Turning head to user's left (screen left, smaller X) must project left of center (< center.x);
    // Turning head to user's right (screen right, larger X) must project right of center (> center.x).
    CHECK(nose_left.x < nose_center.x - 50.0f);
    CHECK(nose_right.x > nose_center.x + 50.0f);

    // Gazing UP (top_top) must project higher on screen (smaller Y) than gazing DOWN (down_down).
    CHECK(gaze_top.y < gaze_down.y - 20.0f);

    // On self_noseleft_eyesright.jpg:
    // - Head/Nose is facing left (screen left): nose_nl_er.x < nose_center.x
    // - Eyes are looking right (screen right): gaze_nl_er.x > nose_nl_er.x + 50px
    CHECK(nose_nl_er.x < nose_center.x);
    CHECK(gaze_nl_er.x > nose_nl_er.x + 50.0f);

    // On self_nosetop_eyesdown.jpg:
    // - Eyes looking down must project below the upward-pointing nose
    CHECK(gaze_top_down.y > nose_top_down.y + 50.0f);
}

