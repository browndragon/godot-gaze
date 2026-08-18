#include "doctest.h"
#include "ort_landmark_model.hpp"
#include "ort_yunet_detector.hpp"
#include "pnp_solver.hpp"
#include "math_defs.hpp"

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
        {"self_roll_left.jpg", "Roll +45 deg Left", 45.0f},
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

    PnPResult res_top{}, res_down{}, res_left{}, res_right{}, res_roll_l{};

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

        // 1. Detect Face ROI
        Gaze::YuNetResult yunet_res;
        bool det_ok = detector.process_frame(frame, yunet_res, roll_hint_rad);
        REQUIRE(det_ok);
        REQUIRE(yunet_res.face_detected);

        Gaze::GazeRect face_bbox(yunet_res.roi_x, yunet_res.roi_y, yunet_res.roi_w, yunet_res.roi_h);

        // 2. Extract 35 Landmarks
        std::vector<Gaze::GazeVector2> landmarks_35;
        bool lm_ok = landmark_model.extract_landmarks(frame.data, frame.width, frame.height, face_bbox, landmarks_35, roll_hint_rad);
        REQUIRE(lm_ok);
        REQUIRE(landmarks_35.size() == 35);

        // 3. Dense 35-Point LM PnP Solver
        double focal = static_cast<double>(frame.width);
        double cx = frame.width * 0.5;
        double cy = frame.height * 0.5;
        Gaze::GazeVector3 pnp_rvec(0.0f, 0.0f, 0.0f);
        Gaze::GazeVector3 pnp_tvec(0.0f, 0.0f, 600.0f);

        auto t0 = std::chrono::high_resolution_clock::now();
        bool pnp_ok = Gaze::solve_pnp_lm(model_35pt, landmarks_35, focal, focal, cx, cy, pnp_rvec, pnp_tvec, false);
        auto t1 = std::chrono::high_resolution_clock::now();
        double pnp_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

        REQUIRE(pnp_ok);
        Gaze::GazeBasis3D basis = Gaze::rodrigues_to_basis(pnp_rvec);
        double sy = std::sqrt(basis.x.x * basis.x.x + basis.x.y * basis.x.y);
        float pnp_pitch = static_cast<float>(std::atan2(basis.y.z, basis.z.z) * Gaze::RAD_TO_DEG);
        float pnp_yaw   = static_cast<float>(std::atan2(-basis.x.z, sy) * Gaze::RAD_TO_DEG);
        float pnp_roll  = static_cast<float>(std::atan2(basis.x.y, basis.x.x) * Gaze::RAD_TO_DEG);

        PnPResult res{
            pnp_pitch,
            pnp_yaw,
            pnp_roll,
            pnp_pitch * static_cast<float>(Gaze::DEG_TO_RAD),
            pnp_yaw * static_cast<float>(Gaze::DEG_TO_RAD),
            pnp_roll * static_cast<float>(Gaze::DEG_TO_RAD),
            pnp_tvec.x,
            pnp_tvec.y,
            pnp_tvec.z,
            pnp_time_us
        };

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
    }

    std::cout << std::string(80, '=') << "\n";

    // 4. Invariant Assertions
    // Signal Separation Metrics
    float delta_yaw = std::abs(res_left.yaw_rad - res_right.yaw_rad);
    std::cout << "\n=== SIGNAL SEPARATION SUMMARY (Target: Delta >= 0.50 rad) ===\n";
    std::cout << " Yaw Delta (Left vs Right): " << delta_yaw << " rad (" << (delta_yaw * Gaze::RAD_TO_DEG) << " deg)\n";
    std::cout << " Roll Angle (Roll Left):    " << res_roll_l.roll_deg << " deg\n";

    // Enforce strict domain signal separation bounds
    CHECK(delta_yaw >= 0.50f);
    CHECK(res_roll_l.roll_deg <= -35.0f);
    CHECK(res_roll_l.roll_deg >= -55.0f);
}
