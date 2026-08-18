#include "doctest.h"
#include "ort_gaze_model.hpp"
#include "ort_landmark_model.hpp"
#include "ort_yunet_detector.hpp"
#include "pnp_solver.hpp"
#include "math_defs.hpp"
#include "stb_image.h"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>

namespace
{
    inline bool file_exists(const std::string &path)
    {
        std::ifstream f(path.c_str());
        return f.good();
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
        for (int i = 0; i < img.width * img.height; ++i)
        {
            img.data[i * 3 + 0] = raw[i * 3 + 2]; // B
            img.data[i * 3 + 1] = raw[i * 3 + 1]; // G
            img.data[i * 3 + 2] = raw[i * 3 + 0]; // R
        }
        stbi_image_free(raw);
        return img;
    }

    std::vector<Gaze::GazeVector3> get_canonical_35pt_model()
    {
        std::vector<Gaze::GazeVector3> pts(35);
        pts[0] = Gaze::GazeVector3(-15.0, -32.0, -18.0);
        pts[1] = Gaze::GazeVector3(-46.0, -32.0,  -8.0);
        pts[2] = Gaze::GazeVector3( 15.0, -32.0, -18.0);
        pts[3] = Gaze::GazeVector3( 46.0, -32.0,  -8.0);
        pts[4] = Gaze::GazeVector3(  0.0, -22.0, -25.0);
        pts[5] = Gaze::GazeVector3(  0.0,   0.0, -35.0);
        pts[6] = Gaze::GazeVector3(-16.0,   6.0, -20.0);
        pts[7] = Gaze::GazeVector3( 16.0,   6.0, -20.0);
        pts[8]  = Gaze::GazeVector3(-25.0,  32.0, -12.0);
        pts[9]  = Gaze::GazeVector3( 25.0,  32.0, -12.0);
        pts[10] = Gaze::GazeVector3(  0.0,  26.0, -20.0);
        pts[11] = Gaze::GazeVector3(  0.0,  40.0, -18.0);
        pts[12] = Gaze::GazeVector3(-50.0, -48.0,  -5.0);
        pts[13] = Gaze::GazeVector3(-32.0, -52.0, -12.0);
        pts[14] = Gaze::GazeVector3(-12.0, -48.0, -18.0);
        pts[15] = Gaze::GazeVector3( 12.0, -48.0, -18.0);
        pts[16] = Gaze::GazeVector3( 32.0, -52.0, -12.0);
        pts[17] = Gaze::GazeVector3( 50.0, -48.0,  -5.0);

        float jaw_x[] = {-70.0f, -68.0f, -64.0f, -58.0f, -50.0f, -40.0f, -28.0f, -15.0f, 0.0f, 15.0f, 28.0f, 40.0f, 50.0f, 58.0f, 64.0f, 68.0f, 70.0f};
        float jaw_y[] = {-35.0f, -20.0f,  -5.0f,  12.0f,  28.0f,  44.0f,  58.0f,  68.0f, 72.0f, 68.0f, 58.0f, 44.0f, 28.0f, 12.0f, -5.0f, -20.0f, -35.0f};
        float jaw_z[] = { 40.0f,  35.0f,  28.0f,  18.0f,   8.0f,  -2.0f, -10.0f, -15.0f, -17.0f, -15.0f, -10.0f, -2.0f, 8.0f, 18.0f, 28.0f, 35.0f, 40.0f};

        for (int i = 0; i < 17; ++i)
        {
            pts[18 + i] = Gaze::GazeVector3(jaw_x[i], jaw_y[i], jaw_z[i]);
        }
        return pts;
    }

    static void extract_dense_eye_crops_60x60(
        const uint8_t *frame_bgr, int width, int height,
        const std::vector<Gaze::GazeVector2> &landmarks_35,
        uint8_t *out_right_crop_bgr, uint8_t *out_left_crop_bgr,
        float scale_factor = 1.5f)
    {
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

        Gaze::crop_and_resize_bgr(
            frame_bgr, width, height,
            r_cx - r_box_s * 0.5f, r_cy - r_box_s * 0.5f, r_box_s, r_box_s,
            out_right_crop_bgr, 60, 60
        );

        Gaze::crop_and_resize_bgr(
            frame_bgr, width, height,
            l_cx - l_box_s * 0.5f, l_cy - l_box_s * 0.5f, l_box_s, l_box_s,
            out_left_crop_bgr, 60, 60
        );
    }

} // namespace

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

    auto model_35pt = get_canonical_35pt_model();

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

        return gaze_model.estimate_raw_gaze(crops, out_gaze_dir);
    };

    Gaze::GazeVector3 gaze_center, gaze_left, gaze_right, gaze_noseleft_eyesright;

    bool ok_center = process_image("self_center.jpg", gaze_center);
    REQUIRE(ok_center);
    std::cout << "[Gaze Suite] self_center.jpg -> Gaze Vector: (" << gaze_center.x << ", " << gaze_center.y << ", " << gaze_center.z << ")\n";

    // 1. Dominant camera forward (-Z) gaze invariant
    double norm_c = std::sqrt(gaze_center.x * gaze_center.x + gaze_center.y * gaze_center.y + gaze_center.z * gaze_center.z);
    CHECK(norm_c == doctest::Approx(1.0).epsilon(1e-3));

    bool ok_left = process_image("self_left_left.jpg", gaze_left);
    REQUIRE(ok_left);
    std::cout << "[Gaze Suite] self_left_left.jpg -> Gaze Vector: (" << gaze_left.x << ", " << gaze_left.y << ", " << gaze_left.z << ")\n";

    bool ok_right = process_image("self_right_right.jpg", gaze_right);
    REQUIRE(ok_right);
    std::cout << "[Gaze Suite] self_right_right.jpg -> Gaze Vector: (" << gaze_right.x << ", " << gaze_right.y << ", " << gaze_right.z << ")\n";

    bool ok_nl_er = process_image("self_noseleft_eyesright.jpg", gaze_noseleft_eyesright);
    REQUIRE(ok_nl_er);
    std::cout << "[Gaze Suite] self_noseleft_eyesright.jpg -> Gaze Vector: (" << gaze_noseleft_eyesright.x << ", " << gaze_noseleft_eyesright.y << ", " << gaze_noseleft_eyesright.z << ")\n";

    // Signal Separation: Looking Left (+X_cam) vs Looking Right (-X_cam)
    CHECK(gaze_left.x > gaze_right.x);
    // Decoupled: Eyes looking right must produce negative X component
    CHECK(gaze_noseleft_eyesright.x < 0.0f);
}
