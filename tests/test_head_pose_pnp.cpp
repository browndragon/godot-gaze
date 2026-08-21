#include "doctest.h"
#include "ort_landmark_model.hpp"
#include "ort_yunet_detector.hpp"
#include "pnp_solver.hpp"
#include "math_defs.hpp"
#include "../src/core/space_conversions.hpp"
#include "../src/core/face_model_geometry.hpp"

#include "stb_image.h"

#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <sys/stat.h>

namespace
{

    bool file_exists(const std::string &path)
    {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    }

    struct LoadedImage
    {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> data;
    };

    LoadedImage load_image(const std::string &path)
    {
        LoadedImage img;
        int channels = 0;
        unsigned char *raw = stbi_load(path.c_str(), &img.width, &img.height, &channels, 3);
        if (!raw) return img;

        img.data.resize(img.width * img.height * 3);
        // Convert RGB to BGR
        for (int i = 0; i < img.width * img.height; ++i)
        {
            img.data[i * 3 + 0] = raw[i * 3 + 2]; // B
            img.data[i * 3 + 1] = raw[i * 3 + 1]; // G
            img.data[i * 3 + 2] = raw[i * 3 + 0]; // R
        }
        stbi_image_free(raw);
        return img;
    }

    // Canonical 35-point anthropometric 3D face model defined directly in camera rest frame
    // (+X right, +Y down, +Z away from camera into scene, face facing camera at rvec = 0)
    std::vector<Gaze::GazeVector3> get_canonical_35pt_model()
    {
        std::vector<Gaze::GazeVector3> pts(35);
        // Eyes (IPD approx 63mm)
        pts[0] = Gaze::GazeVector3(-15.0, -32.0, -18.0); // Right Eye Inner Canthus
        pts[1] = Gaze::GazeVector3(-46.0, -32.0,  -8.0); // Right Eye Outer Canthus
        pts[2] = Gaze::GazeVector3( 15.0, -32.0, -18.0); // Left Eye Inner Canthus
        pts[3] = Gaze::GazeVector3( 46.0, -32.0,  -8.0); // Left Eye Outer Canthus

        // Nose
        pts[4] = Gaze::GazeVector3(  0.0, -22.0, -25.0); // Nose Bridge Top
        pts[5] = Gaze::GazeVector3(  0.0,   0.0, -35.0); // Nose Tip (furthest forward towards camera)
        pts[6] = Gaze::GazeVector3(-16.0,   6.0, -20.0); // Nose Right Wing
        pts[7] = Gaze::GazeVector3( 16.0,   6.0, -20.0); // Nose Left Wing

        // Mouth
        pts[8]  = Gaze::GazeVector3(-25.0,  32.0, -12.0); // Mouth Right Corner
        pts[9]  = Gaze::GazeVector3( 25.0,  32.0, -12.0); // Mouth Left Corner
        pts[10] = Gaze::GazeVector3(  0.0,  26.0, -20.0); // Upper Lip Center
        pts[11] = Gaze::GazeVector3(  0.0,  40.0, -18.0); // Lower Lip Center

        // Eyebrows
        pts[12] = Gaze::GazeVector3(-50.0, -48.0,  -5.0); // Right Eyebrow Outer
        pts[13] = Gaze::GazeVector3(-32.0, -52.0, -12.0); // Right Eyebrow Mid
        pts[14] = Gaze::GazeVector3(-12.0, -48.0, -18.0); // Right Eyebrow Inner
        pts[15] = Gaze::GazeVector3( 12.0, -48.0, -18.0); // Left Eyebrow Inner
        pts[16] = Gaze::GazeVector3( 32.0, -52.0, -12.0); // Left Eyebrow Mid
        pts[17] = Gaze::GazeVector3( 50.0, -48.0,  -5.0); // Left Eyebrow Outer

        // 17-point Jawline Contour (Pts 18..34) from Right Ear to Chin Apex (Pt 26) to Left Ear
        float jaw_x[] = {-70.0f, -68.0f, -64.0f, -58.0f, -50.0f, -40.0f, -28.0f, -15.0f, 0.0f, 15.0f, 28.0f, 40.0f, 50.0f, 58.0f, 64.0f, 68.0f, 70.0f};
        float jaw_y[] = {-35.0f, -20.0f,  -5.0f,  12.0f,  28.0f,  44.0f,  58.0f,  68.0f, 72.0f, 68.0f, 58.0f, 44.0f, 28.0f, 12.0f, -5.0f, -20.0f, -35.0f};
        float jaw_z[] = { 40.0f,  35.0f,  28.0f,  18.0f,   8.0f,  -2.0f, -10.0f, -15.0f, -17.0f, -15.0f, -10.0f, -2.0f, 8.0f, 18.0f, 28.0f, 35.0f, 40.0f};

        for (int i = 0; i < 17; ++i)
        {
            pts[18 + i] = Gaze::GazeVector3(jaw_x[i], jaw_y[i], jaw_z[i]);
        }

        return pts;
    }

} // namespace

TEST_CASE("Phase 3 Head Pose Estimation: Dense 35-Point Levenberg-Marquardt PnP Solver")
{
    std::string lm_model_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    if (!file_exists(lm_model_path)) lm_model_path = "../project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";

    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    if (!file_exists(yunet_path)) yunet_path = "../project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";

    REQUIRE(file_exists(lm_model_path));
    REQUIRE(file_exists(yunet_path));

    Gaze::ORTLandmarkModel landmark_model(lm_model_path);
    REQUIRE(landmark_model.initialize() == true);

    Gaze::ORTYuNetDetector detector(yunet_path);
    REQUIRE(detector.initialize() == true);

    auto model_35pt = get_canonical_35pt_model();

    struct BenchmarkSample
    {
        std::string filename;
        std::string description;
        float roll_hint_deg;
    };

    std::vector<BenchmarkSample> samples = {
        {"self_center.jpg", "Baseline Upright Center", 0.0f},
        {"self_center2.jpg", "Baseline Center Secondary", 0.0f},
        {"self_top_top.jpg", "Pitch UP (Head Up, Eyes Up)", 0.0f},
        {"self_down_down.jpg", "Pitch DOWN (Head Down, Eyes Down)", 0.0f},
        {"self_left_left.jpg", "Yaw LEFT (Head Left, Eyes Left)", 0.0f},
        {"self_right_right.jpg", "Yaw RIGHT (Head Right, Eyes Right)", 0.0f},
        {"self_noseleft_eyesright.jpg", "Decoupled (Head Left, Eyes Right)", 0.0f},
        {"self_noseright_eyesleft.jpg", "Decoupled (Head Right, Eyes Left)", 0.0f},
        {"self_roll_left.jpg", "Roll Left (Anatomical Left)", -45.0f},
        {"self_roll_right.jpg", "Roll Right (Anatomical Right)", 45.0f},
        {"self_yaw_left_roll_left.jpg", "Combined Yaw Left + Roll Left", -15.0f},
        {"self_yaw_right_roll_left.jpg", "Combined Yaw Right + Roll Right/Left", 25.0f},
    };

    struct PnPResult
    {
        float pitch_deg;
        float yaw_deg;
        float roll_deg;
        float pitch_rad;
        float yaw_rad;
        float roll_rad;
        float tx_mm;
        float ty_mm;
        float tz_mm;
        double time_us;
    };

    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << " PHASE 3 DENSE 35-POINT PNP HEAD POSE EVALUATION (Camera Basis)\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << std::left << std::setw(28) << "Image"
              << std::setw(26) << "Pitch / Yaw / Roll (deg)"
              << std::setw(26) << "Translation (X, Y, Z mm)" << "\n";
    std::cout << std::string(80, '-') << "\n";

    PnPResult res_top{}, res_down{}, res_left{}, res_right{}, res_roll_l{}, res_roll_r{}, res_yaw_l_roll_l{}, res_yaw_r_roll_l{};

    for (const auto &sample : samples)
    {
        std::string img_path = "tests/resources/" + sample.filename;
        if (!file_exists(img_path)) img_path = "../tests/resources/" + sample.filename;

        LoadedImage img = load_image(img_path);
        REQUIRE(img.data.size() > 0);

        Gaze::Frame frame;
        frame.width = img.width;
        frame.height = img.height;
        frame.data = img.data.data();

        float roll_hint_rad = sample.roll_hint_deg * (3.14159265f / 180.0f);

        // 1. Post-rotated space: counter-rotate image by -roll_hint_rad
        std::vector<unsigned char> rot_buffer;
        const unsigned char *working_data = frame.data;
        if (std::abs(roll_hint_rad) > 1e-4f)
        {
            rot_buffer.resize(frame.width * frame.height * 3);
            Gaze::rotate_image_bgr(frame.data, frame.width, frame.height, rot_buffer.data(), -roll_hint_rad);
            working_data = rot_buffer.data();
        }
        Gaze::Frame working_frame = frame;
        working_frame.data = const_cast<unsigned char *>(working_data);

        // 2. Detect Face ROI in upright working_frame
        Gaze::YuNetResult yunet_res;
        bool det_ok = detector.process_frame(working_frame, yunet_res, 0.0f);
        REQUIRE(det_ok);
        REQUIRE(yunet_res.face_detected);

        Gaze::GazeRect face_bbox(yunet_res.roi_x, yunet_res.roi_y, yunet_res.roi_w, yunet_res.roi_h);

        // 3. Extract 35 Landmarks directly from upright working_frame (1:1 scale)
        std::vector<Gaze::GazeVector2> landmarks_35;
        bool lm_ok = landmark_model.extract_landmarks(working_frame.data, working_frame.width, working_frame.height, face_bbox, landmarks_35, 0.0f);
        REQUIRE(lm_ok);
        REQUIRE(landmarks_35.size() == 35);

        // 4. Dense 35-Point LM PnP Solver in upright frame
        double focal = static_cast<double>(frame.width);
        double cx = frame.width * 0.5;
        double cy = frame.height * 0.5;

        Gaze::GazeVector3 rvec(0.0f, 0.0f, 0.0f);
        Gaze::GazeVector3 tvec(0.0f, 0.0f, 600.0f);

        auto t0 = std::chrono::high_resolution_clock::now();
        bool pnp_ok = Gaze::solve_pnp_lm(model_35pt, landmarks_35, focal, focal, cx, cy, rvec, tvec, false);
        auto t1 = std::chrono::high_resolution_clock::now();
        double dt_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

        REQUIRE(pnp_ok);

        Gaze::GazeBasis3D R_up = Gaze::rodrigues_to_basis(rvec);
        Gaze::GazeBasis3D R_z = Gaze::rodrigues_to_basis(Gaze::GazeVector3(0.0, 0.0, roll_hint_rad));
        Gaze::GazeBasis3D R_orig = R_z * R_up;
        Gaze::GazeVector3 t_orig = R_z.multiply_vector(tvec);
        Gaze::GazeVector3 r_orig = Gaze::basis_to_rodrigues(R_orig);

        PnPResult res;
        res.pitch_rad = r_orig.x;
        res.yaw_rad = r_orig.y;
        res.roll_rad = r_orig.z;
        res.pitch_deg = r_orig.x * Gaze::RAD_TO_DEG;
        res.yaw_deg = r_orig.y * Gaze::RAD_TO_DEG;
        res.roll_deg = r_orig.z * Gaze::RAD_TO_DEG;
        res.tx_mm = t_orig.x;
        res.ty_mm = t_orig.y;
        res.tz_mm = t_orig.z;
        res.time_us = dt_us;

        char buf_rot[64], buf_trans[64];
        snprintf(buf_rot, sizeof(buf_rot), "%5.1f / %5.1f / %5.1f deg", res.pitch_deg, res.yaw_deg, res.roll_deg);
        snprintf(buf_trans, sizeof(buf_trans), "(%5.1f, %5.1f, %5.1f mm)", res.tx_mm, res.ty_mm, res.tz_mm);

        std::cout << std::left << std::setw(28) << sample.filename
                  << std::setw(26) << buf_rot
                  << std::setw(26) << buf_trans << "\n";

        if (sample.filename == "self_top_top.jpg") res_top = res;
        if (sample.filename == "self_down_down.jpg") res_down = res;
        if (sample.filename == "self_left_left.jpg") res_left = res;
        if (sample.filename == "self_right_right.jpg") res_right = res;
        if (sample.filename == "self_roll_left.jpg") res_roll_l = res;
        if (sample.filename == "self_roll_right.jpg") res_roll_r = res;
        if (sample.filename == "self_yaw_left_roll_left.jpg") res_yaw_l_roll_l = res;
        if (sample.filename == "self_yaw_right_roll_left.jpg") res_yaw_r_roll_l = res;
    }

    std::cout << std::string(80, '=') << "\n";

    // 4. Invariant Assertions
    // Signal Separation Metrics
    float delta_yaw = std::abs(res_left.yaw_rad - res_right.yaw_rad);
    std::cout << "\n=== SIGNAL SEPARATION SUMMARY (Target: Delta >= 0.50 rad) ===\n";
    std::cout << " Yaw Delta (Left vs Right): " << delta_yaw << " rad (" << (delta_yaw * Gaze::RAD_TO_DEG) << " deg)\n";
    std::cout << " Roll Angle (Roll Left):    " << res_roll_l.roll_deg << " deg\n";
    std::cout << " Roll Angle (Roll Right):   " << res_roll_r.roll_deg << " deg\n";

    // Enforce strict domain signal separation bounds
    CHECK(delta_yaw >= 0.50f);
    CHECK(res_roll_l.roll_deg <= -35.0f);
    CHECK(res_roll_l.roll_deg >= -55.0f);
    CHECK(res_roll_r.roll_deg >= 35.0f);
    CHECK(res_roll_r.roll_deg <= 55.0f);

    // Combined Pose Regression Bounds
    CHECK(res_yaw_l_roll_l.yaw_deg <= -5.0f);
    CHECK(res_yaw_r_roll_l.yaw_deg >= 10.0f);
    CHECK(res_yaw_l_roll_l.roll_deg >= 15.0f);
    CHECK(res_yaw_r_roll_l.roll_deg >= 20.0f);
}

TEST_CASE("PnP DLT Initial Guess Correctness for 35-point Face Model")
{
    auto model_35pt = get_canonical_35pt_model();
    double fx = 800.0, fy = 800.0;
    double cx = 320.0, cy = 240.0;

    for (float roll_deg : {-45.0f, -30.0f, 0.0f, 30.0f, 45.0f})
    {
        float roll_rad = roll_deg * (3.14159265f / 180.0f);
        Gaze::GazeVector3 true_rvec(0.0f, 0.0f, roll_rad);
        Gaze::GazeBasis3D basis = Gaze::rodrigues_to_basis(true_rvec);

        std::vector<Gaze::GazeVector2> img_pts(35);
        for (size_t i = 0; i < 35; ++i)
        {
            Gaze::GazeVector3 p_cam = basis * model_35pt[i] + Gaze::GazeVector3(0.0f, 0.0f, 600.0f);
            img_pts[i].x = (p_cam.x / p_cam.z) * fx + cx;
            img_pts[i].y = (p_cam.y / p_cam.z) * fy + cy;
        }

        Gaze::GazeVector3 dlt_r, dlt_t;
        bool ok = Gaze::solve_pnp_dlt(model_35pt, img_pts, fx, fy, cx, cy, dlt_r, dlt_t);
        REQUIRE(ok);

        float roll_error_deg = std::abs(dlt_r.z * (180.0f / 3.14159265f) - roll_deg);
        CHECK(roll_error_deg < 2.0f);
    }
}

TEST_CASE("Continuous Head Roll Tracking Feedback Loop")
{
    std::string lm_model_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    if (!file_exists(lm_model_path)) lm_model_path = "../project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";

    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    if (!file_exists(yunet_path)) yunet_path = "../project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";

    REQUIRE(file_exists(lm_model_path));
    REQUIRE(file_exists(yunet_path));

    Gaze::ORTLandmarkModel landmark_model(lm_model_path);
    REQUIRE(landmark_model.initialize() == true);

    Gaze::ORTYuNetDetector detector(yunet_path);
    REQUIRE(detector.initialize() == true);

    std::string img_path = "tests/resources/self_center.jpg";
    if (!file_exists(img_path)) img_path = "../tests/resources/self_center.jpg";

    LoadedImage img = load_image(img_path);
    REQUIRE(img.data.size() > 0);

    auto model_35pt = get_canonical_35pt_model();
    double focal = static_cast<double>(img.width);
    double cx = img.width * 0.5;
    double cy = img.height * 0.5;

    // Continuous sequential roll sweep from 0 -> +45 -> -45 -> 0 in 3 deg steps
    float current_roll_hint_rad = 0.0f;
    std::vector<unsigned char> rot_frame(img.width * img.height * 3);
    std::vector<unsigned char> working_frame(img.width * img.height * 3);
    // Extract ground truth base landmarks from the upright image
    Gaze::Frame base_frame;
    base_frame.width = img.width;
    base_frame.height = img.height;
    base_frame.data = img.data.data();
    Gaze::YuNetResult base_yunet_res;
    REQUIRE(detector.process_frame(base_frame, base_yunet_res, 0.0f));
    Gaze::GazeRect base_bbox(base_yunet_res.roi_x, base_yunet_res.roi_y, base_yunet_res.roi_w, base_yunet_res.roi_h);
    std::vector<Gaze::GazeVector2> base_landmarks;
    REQUIRE(landmark_model.extract_landmarks(img.data.data(), img.width, img.height, base_bbox, base_landmarks, 0.0f));
    REQUIRE(base_landmarks.size() == 35);

    for (int step = -15; step <= 15; ++step)
    {
        float true_roll_deg = static_cast<float>(step * 3);
        float true_roll_rad = true_roll_deg * (3.14159265f / 180.0f);

        // Apply true roll to base image
        Gaze::rotate_image_bgr(img.data.data(), img.width, img.height, rot_frame.data(), true_roll_rad);

        // Pipeline step: counter-rotate by current_roll_hint_rad
        Gaze::rotate_image_bgr(rot_frame.data(), img.width, img.height, working_frame.data(), -current_roll_hint_rad);

        Gaze::Frame frame;
        frame.width = img.width;
        frame.height = img.height;
        frame.data = working_frame.data();

        Gaze::YuNetResult yunet_res;
        bool det_ok = detector.process_frame(frame, yunet_res, 0.0f);
        REQUIRE(det_ok);
        REQUIRE(yunet_res.face_detected);

        Gaze::GazeRect face_bbox(yunet_res.roi_x, yunet_res.roi_y, yunet_res.roi_w, yunet_res.roi_h);
        std::vector<Gaze::GazeVector2> landmarks_35;
        bool lm_ok = landmark_model.extract_landmarks(working_frame.data(), img.width, img.height, face_bbox, landmarks_35, 0.0f);
        REQUIRE(lm_ok);
        REQUIRE(landmarks_35.size() == 35);

        Gaze::GazeVector3 rvec(0.0f, 0.0f, 0.0f);
        Gaze::GazeVector3 tvec(0.0f, 0.0f, 600.0f);
        bool pnp_ok = Gaze::solve_pnp_lm(model_35pt, landmarks_35, focal, focal, cx, cy, rvec, tvec, false);
        REQUIRE(pnp_ok);

        Gaze::GazeBasis3D R_up = Gaze::rodrigues_to_basis(rvec);
        Gaze::GazeBasis3D R_z = Gaze::rodrigues_to_basis(Gaze::GazeVector3(0.0, 0.0, current_roll_hint_rad));
        Gaze::GazeBasis3D R_orig = R_z * R_up;
        Gaze::GazeVector3 r_orig = Gaze::basis_to_rodrigues(R_orig);

        float solved_roll_rad = r_orig.z;
        float solved_roll_deg = solved_roll_rad * (180.0f / 3.14159265f);

        // Verify that 2D landmarks unrotated back to camera space align with ground truth rotated face position
        for (size_t lm_idx : {0, 1, 2, 3, 30})
        {
            Gaze::GazeVector2 expected_pt = Gaze::rotate_point_back(base_landmarks[lm_idx], -true_roll_rad, img.width, img.height);
            Gaze::GazeVector2 unrotated_pt = Gaze::rotate_point_back(landmarks_35[lm_idx], -current_roll_hint_rad, img.width, img.height);
            float dx = unrotated_pt.x - expected_pt.x;
            float dy = unrotated_pt.y - expected_pt.y;
            float dist_px = std::sqrt(dx * dx + dy * dy);
            CHECK(dist_px < 15.0f);
        }

        // Update tracking feedback
        current_roll_hint_rad = solved_roll_rad;

        float roll_error_deg = std::abs(solved_roll_deg - true_roll_deg);
        CHECK(roll_error_deg < 4.0f);
    }
}

TEST_CASE("OpenCV to Godot Space Conversion 3D Projection Invariance Across All Octants")
{
    auto cv_pts = Gaze::get_canonical_35pt_face_model();
    auto godot_pts = Gaze::FaceModelGeometry::get_canonical_godot_model_points();
    REQUIRE(cv_pts.size() == 35);
    REQUIRE(godot_pts.size() == 35);

    double fx = 800.0, fy = 800.0;
    double cx = 320.0, cy = 240.0;

    for (float pitch_deg : {-15.0f, 0.0f, 15.0f})
    {
        for (float yaw_deg : {-30.0f, 0.0f, 30.0f})
        {
            for (float roll_deg : {-25.0f, 0.0f, 25.0f})
            {
                Gaze::GazeVector3 rvec(pitch_deg * Gaze::DEG_TO_RAD, yaw_deg * Gaze::DEG_TO_RAD, roll_deg * Gaze::DEG_TO_RAD);
                Gaze::GazeVector3 tvec(20.0, -15.0, 550.0);

                Gaze::GazeBasis3D R_cv = Gaze::rodrigues_to_basis(rvec);

                // Compute Godot Head Transform
                Gaze::GazeTransform3D T_godot = Gaze::Inference::get_head_transform_in_camera_space(tvec, rvec);

                // Check projection of all 35 points in both systems
                for (size_t i = 0; i < 35; ++i)
                {
                    // 1. OpenCV projection
                    Gaze::GazeVector3 p_cam_cv = R_cv.multiply_vector(cv_pts[i]) + tvec;
                    double px_cv = (p_cam_cv.x / p_cam_cv.z) * fx + cx;
                    double py_cv = (p_cam_cv.y / p_cam_cv.z) * fy + cy;

                    // 2. Godot debug overlay projection
                    Gaze::GazeVector3 p_cam_godot = T_godot.basis.multiply_vector(godot_pts[i]) + T_godot.origin;
                    double depth = -p_cam_godot.z;
                    double px_godot = (p_cam_godot.x / depth) * fx + cx;
                    double py_godot = cy - (p_cam_godot.y / depth) * fy;

                    double diff_x = std::abs(px_cv - px_godot);
                    double diff_y = std::abs(py_cv - py_godot);
                    CHECK(diff_x < 1e-4);
                    CHECK(diff_y < 1e-4);
                }
            }
        }
    }
}

TEST_CASE("ORTLandmarkModel Degenerate Bounding Box Validation")
{
    std::string lm_model_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    if (!file_exists(lm_model_path)) lm_model_path = "../project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
    REQUIRE(file_exists(lm_model_path));

    Gaze::ORTLandmarkModel landmark_model(lm_model_path);
    REQUIRE(landmark_model.initialize() == true);

    std::vector<unsigned char> dummy_frame(640 * 480 * 3, 128);
    std::vector<Gaze::GazeVector2> landmarks;

    // 1. Degenerately small width (< 20)
    Gaze::GazeRect small_w_box(100.0f, 100.0f, 10.0f, 100.0f);
    CHECK_FALSE(landmark_model.extract_landmarks(dummy_frame.data(), 640, 480, small_w_box, landmarks, 0.0f));

    // 2. Degenerately small height (< 20)
    Gaze::GazeRect small_h_box(100.0f, 100.0f, 100.0f, 10.0f);
    CHECK_FALSE(landmark_model.extract_landmarks(dummy_frame.data(), 640, 480, small_h_box, landmarks, 0.0f));

    // 3. Degenerate aspect ratio (width / height = 50 / 500 = 0.1)
    Gaze::GazeRect skinny_box(100.0f, 100.0f, 50.0f, 500.0f);
    CHECK_FALSE(landmark_model.extract_landmarks(dummy_frame.data(), 640, 480, skinny_box, landmarks, 0.0f));
}


