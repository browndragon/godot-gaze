#include "doctest.h"
#include "ort_gaze_model.hpp"
#include "ort_landmark_model.hpp"
#include "ort_yunet_detector.hpp"
#include "ort_eye_state_model.hpp"
#include "projection_engine.hpp"
#include "camera_placement.hpp"
#include "space_conversions.hpp"
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

        Gaze::GazeVector3 raw_gaze;
        if (!gaze_model.estimate_raw_gaze(crops, raw_gaze)) return false;
        out_gaze_dir = Gaze::Inference::ONNX_GAZE_TO_GODOT_CAM.multiply_vector(raw_gaze);
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
        Gaze::GazeTransform3D head_xform = Gaze::Inference::get_head_transform_in_camera_space(tvec, rvec);
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
        Gaze::GazeVector3 gaze_dir_godot = Gaze::Inference::ONNX_GAZE_TO_GODOT_CAM.multiply_vector(raw_gaze_dir_cam);
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

    // 1. NOSEGAZE PITCH INVARIANT:
    // Tilting head UP (top_top) must project higher on screen (smaller Y) than tilting head DOWN (down_down).
    CHECK(nose_top.y < nose_down.y - 30.0f);

    // 2. NOSEGAZE YAW INVARIANT:
    // Turning head to user's left (screen left, smaller X) must project left of center (< center.x);
    // Turning head to user's right (screen right, larger X) must project right of center (> center.x).
    CHECK(nose_left.x < nose_center.x - 50.0f);
    CHECK(nose_right.x > nose_center.x + 50.0f);

    // 3. EYEGAZE YAW INVARIANT:
    // Gazing to user's left (screen left, smaller X) must project left of center (< center.x);
    // Gazing to user's right (screen right, larger X) must project right of center (> center.x).
    CHECK(gaze_left.x < gaze_center.x - 30.0f);
    CHECK(gaze_right.x > gaze_center.x + 30.0f);

    // 4. EYEGAZE PITCH INVARIANT:
    // Gazing UP (top_top) must project higher on screen (smaller Y) than gazing DOWN (down_down).
    CHECK(gaze_top.y < gaze_down.y - 20.0f);

    // 5. DECOUPLED HEAD/EYE GAZE INVARIANTS:
    // On self_noseleft_eyesright.jpg:
    // - Head/Nose is facing user's left (screen left): nose_nl_er.x < nose_center.x
    // - Eyes are looking to user's right (screen right): gaze_nl_er.x > nose_nl_er.x + 50px
    CHECK(nose_nl_er.x < nose_center.x);
    CHECK(gaze_nl_er.x > nose_nl_er.x + 50.0f);

    // On self_nosetop_eyesdown.jpg:
    // - Eyes looking down must project below the upward-pointing nose
    CHECK(gaze_top_down.y > nose_top_down.y + 50.0f);
}

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static std::string base64_encode_bytes(const unsigned char *data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t octet_a = i < len ? data[i] : 0;
        uint32_t octet_b = (i + 1) < len ? data[i + 1] : 0;
        uint32_t octet_c = (i + 2) < len ? data[i + 2] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        out.push_back(b64_table[(triple >> 18) & 0x3F]);
        out.push_back(b64_table[(triple >> 12) & 0x3F]);
        out.push_back((i + 1) < len ? b64_table[(triple >> 6) & 0x3F] : '=');
        out.push_back((i + 2) < len ? b64_table[triple & 0x3F] : '=');
    }
    return out;
}

TEST_CASE("Export All 14 Benchmark Mockup Data in Godot Camera Space")
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

    // Configure 2021 14" MacBook Pro (Liquid Retina XDR: 3024x1964 native, 1512x982 logical @ 2x)
    // Physical size: 304.1 x 196.4 mm
    Gaze::ProjectionEngine engine_mbp;
    engine_mbp.set_screen_size_pixels(Gaze::GazeVector2(1512.0, 982.0));
    engine_mbp.set_screen_size_mm(Gaze::GazeVector2(304.1, 196.4));
    // Top-center notch camera (offset: 0, 107.5, 0 mm), 0 deg tilt
    engine_mbp.set_camera_placement(Gaze::CameraPlacement(Gaze::GazeVector3(0.0, 107.5, 0.0), 0.0));

    auto model_35 = Gaze::get_canonical_35pt_face_model();

    std::vector<std::string> fixtures = {
        "self_center.jpg",
        "self_center2.jpg",
        "self_left_left.jpg",
        "self_right_right.jpg",
        "self_top_top.jpg",
        "self_down_down.jpg",
        "self_nosetop_eyesdown.jpg",
        "self_nosedown_eyesup.jpg",
        "self_noseleft_eyesright.jpg",
        "self_noseright_eyesleft.jpg",
        "self_roll_left.jpg",
        "self_roll_right.jpg",
        "self_yaw_left_roll_left.jpg",
        "self_yaw_right_roll_left.jpg"
    };

    std::ofstream json_out("/Users/browndragon/.gemini/antigravity/brain/5c6240bd-66e7-497b-ac64-7ab32e79fc65/scratch/benchmark_data.json");
    REQUIRE(json_out.is_open());

    json_out << "[\n";

    for (size_t f_idx = 0; f_idx < fixtures.size(); ++f_idx)
    {
        const std::string &fname = fixtures[f_idx];
        LoadedImage img = load_image("tests/resources/" + fname);
        REQUIRE(!img.data.empty());

        Gaze::Frame frame{img.width, img.height, img.data.data(), 0};

        // 1. Initial face detection to check roll
        Gaze::YuNetResult yunet_initial;
        bool det_ok = detector.process_frame(frame, yunet_initial, 0.0f);
        REQUIRE(det_ok);
        REQUIRE(yunet_initial.face_detected);

        float roll_hint_rad = 0.0f;
        float roll_hint_deg = 0.0f;
        float dx = yunet_initial.left_eye_px.x - yunet_initial.right_eye_px.x;
        float dy = yunet_initial.left_eye_px.y - yunet_initial.right_eye_px.y;
        if (std::abs(dx) > 1e-4f || std::abs(dy) > 1e-4f)
        {
            roll_hint_rad = std::atan2(dy, dx);
            roll_hint_deg = roll_hint_rad * 180.0f / 3.1415926535f;
        }

        // 2. Counter-rotate frame if face is rolled
        std::vector<unsigned char> working_buffer;
        const unsigned char *working_data = frame.data;
        if (std::abs(roll_hint_rad) > 1e-4f)
        {
            working_buffer.resize(frame.width * frame.height * 3);
            Gaze::rotate_image_bgr(frame.data, frame.width, frame.height, working_buffer.data(), -roll_hint_rad);
            working_data = working_buffer.data();
        }
        Gaze::Frame working_frame = frame;
        working_frame.data = const_cast<unsigned char *>(working_data);

        Gaze::YuNetResult yunet_upright;
        detector.process_frame(working_frame, yunet_upright, 0.0f);

        Gaze::GazeRect bbox(yunet_upright.roi_x, yunet_upright.roi_y, yunet_upright.roi_w, yunet_upright.roi_h);
        std::vector<Gaze::GazeVector2> landmarks_35_upright;
        bool lm_ok = lm_model.extract_landmarks(working_frame.data, working_frame.width, working_frame.height, bbox, landmarks_35_upright, 0.0f);
        REQUIRE(lm_ok);
        REQUIRE(landmarks_35_upright.size() == 35);

        // Solve PnP on upright landmarks
        double focal = static_cast<double>(frame.width);
        double cx = frame.width * 0.5;
        double cy = frame.height * 0.5;
        Gaze::GazeVector3 rvec(0.0f, 0.0f, 0.0f), tvec(0.0f, 0.0f, 600.0f);
        bool pnp_ok = Gaze::solve_pnp_lm(model_35, landmarks_35_upright, focal, focal, cx, cy, rvec, tvec, false);
        REQUIRE(pnp_ok);

        Gaze::GazeVector3 final_rvec = rvec;
        Gaze::GazeVector3 final_tvec = tvec;
        if (std::abs(roll_hint_rad) > 1e-4f)
        {
            Gaze::GazeBasis3D R_up = Gaze::rodrigues_to_basis(rvec);
            Gaze::GazeBasis3D R_z = Gaze::rodrigues_to_basis(Gaze::GazeVector3(0.0, 0.0, roll_hint_rad));
            Gaze::GazeBasis3D R_orig = R_z * R_up;
            final_tvec = R_z.multiply_vector(tvec);
            final_rvec = Gaze::basis_to_rodrigues(R_orig);
        }

        // Project landmarks back to original frame for 2D visualization
        std::vector<Gaze::GazeVector2> landmarks_35_orig(35);
        for (int i = 0; i < 35; ++i)
        {
            if (std::abs(roll_hint_rad) > 1e-4f)
            {
                landmarks_35_orig[i] = Gaze::rotate_point_back(landmarks_35_upright[i], -roll_hint_rad, frame.width, frame.height);
            }
            else
            {
                landmarks_35_orig[i] = landmarks_35_upright[i];
            }
        }

        // Head Transform in Godot Camera Space
        Gaze::GazeTransform3D head_xform = Gaze::Inference::get_head_transform_in_camera_space(final_tvec, final_rvec);
        Gaze::GazeVector3 head_origin_godot = head_xform.origin;
        Gaze::GazeBasis3D R_godot = head_xform.basis;
        Gaze::GazeVector3 head_fwd_godot = -R_godot.z.normalized();
        Gaze::GazeVector3 euler_godot = R_godot.get_euler_deg();

        // 3D 10cm Head Basis endpoints in Godot Camera Space
        Gaze::GazeVector3 head_pt_origin = head_origin_godot;
        Gaze::GazeVector3 head_pt_x = head_origin_godot + R_godot.x * 100.0;
        Gaze::GazeVector3 head_pt_y = head_origin_godot + R_godot.y * 100.0;
        Gaze::GazeVector3 head_pt_fwd = head_origin_godot + head_fwd_godot * 100.0;

        auto project_cam_to_px = [&](const Gaze::GazeVector3 &p) -> Gaze::GazeVector2 {
            double u = cx + (p.x * focal) / (-p.z);
            double v = cy - (p.y * focal) / (-p.z);
            return Gaze::GazeVector2(u, v);
        };

        Gaze::GazeVector2 head_px_origin = project_cam_to_px(head_pt_origin);
        Gaze::GazeVector2 head_px_x = project_cam_to_px(head_pt_x);
        Gaze::GazeVector2 head_px_y = project_cam_to_px(head_pt_y);
        Gaze::GazeVector2 head_px_fwd = project_cam_to_px(head_pt_fwd);

        // Extract eye crops from upright frame
        uint8_t right_crop[60 * 60 * 3];
        uint8_t left_crop[60 * 60 * 3];
        extract_dense_eye_crops_60x60(working_frame.data, working_frame.width, working_frame.height, landmarks_35_upright, right_crop, left_crop, 2.2f);

        float left_openness = 1.0f;
        float right_openness = 1.0f;
        eye_model.estimate_openness(left_crop, left_openness);
        eye_model.estimate_openness(right_crop, right_openness);

        Gaze::EyeCrops crops;
        crops.face_detected = true;
        crops.head_pose_translation = final_tvec;
        crops.head_pose_rotation = final_rvec;
        std::memcpy(crops.left_eye_data, left_crop, 60 * 60 * 3);
        std::memcpy(crops.right_eye_data, right_crop, 60 * 60 * 3);

        Gaze::GazeVector3 raw_gaze_dir;
        bool gaze_ok = gaze_model.estimate_raw_gaze(crops, raw_gaze_dir);
        REQUIRE(gaze_ok);

        Gaze::GazeVector3 gaze_dir_godot = Gaze::Inference::ONNX_GAZE_TO_GODOT_CAM.multiply_vector(raw_gaze_dir).normalized();

        // Eye midpoint in Godot camera space
        Gaze::GazeVector3 eye_origin_godot = head_origin_godot + R_godot.multiply_vector(Gaze::GazeVector3(0.0, 32.0, 13.0));
        Gaze::GazeVector3 eye_pt_x = eye_origin_godot + R_godot.x * 100.0;
        Gaze::GazeVector3 eye_pt_y = eye_origin_godot + R_godot.y * 100.0;
        Gaze::GazeVector3 eye_pt_gaze = eye_origin_godot + gaze_dir_godot * 100.0;

        Gaze::GazeVector2 eye_px_origin = project_cam_to_px(eye_origin_godot);
        Gaze::GazeVector2 eye_px_x = project_cam_to_px(eye_pt_x);
        Gaze::GazeVector2 eye_px_y = project_cam_to_px(eye_pt_y);
        Gaze::GazeVector2 eye_px_gaze = project_cam_to_px(eye_pt_gaze);

        // Project Nose Ray and Eye Ray to MacBook Pro 14" Screen
        Gaze::GazeVector2 nose_target_px(0.0, 0.0);
        Gaze::GazeVector2 eye_target_px(0.0, 0.0);
        bool nose_proj_ok = engine_mbp.project_gaze(head_origin_godot, head_fwd_godot, nose_target_px);
        bool eye_proj_ok = engine_mbp.project_gaze(head_origin_godot, gaze_dir_godot, eye_target_px);

        double mm_per_px_x = 304.1 / 1512.0;
        double mm_per_px_y = 196.4 / 982.0;
        Gaze::GazeVector2 nose_target_mm(
            (nose_target_px.x - 1512.0 * 0.5) * mm_per_px_x,
            (nose_target_px.y - 982.0 * 0.5) * mm_per_px_y
        );
        Gaze::GazeVector2 eye_target_mm(
            (eye_target_px.x - 1512.0 * 0.5) * mm_per_px_x,
            (eye_target_px.y - 982.0 * 0.5) * mm_per_px_y
        );

        std::string left_crop_b64 = base64_encode_bytes(left_crop, 60 * 60 * 3);
        std::string right_crop_b64 = base64_encode_bytes(right_crop, 60 * 60 * 3);

        json_out << "  {\n";
        json_out << "    \"filename\": \"" << fname << "\",\n";
        json_out << "    \"width\": " << frame.width << ",\n";
        json_out << "    \"height\": " << frame.height << ",\n";
        json_out << "    \"roll_hint_deg\": " << roll_hint_deg << ",\n";
        json_out << "    \"left_openness\": " << left_openness << ",\n";
        json_out << "    \"right_openness\": " << right_openness << ",\n";
        // Compute YuNet and Landmark crop bounding boxes
        float adj_bx = bbox.x - 0.067f * bbox.width;
        float adj_by = bbox.y - 0.028f * bbox.height;
        float adj_bw = bbox.width * 1.15f;
        float adj_bh = bbox.height * 1.13f;
        if (adj_bw < adj_bh) {
            float dx_pad = adj_bh - adj_bw;
            adj_bx -= dx_pad * 0.5f;
            adj_bw = adj_bh;
        } else {
            float dy_pad = adj_bw - adj_bh;
            adj_by -= dy_pad * 0.5f;
            adj_bh = adj_bw;
        }

        auto rotate_corners_back = [&](float x, float y, float w, float h) -> std::vector<Gaze::GazeVector2> {
            std::vector<Gaze::GazeVector2> corners = {
                Gaze::GazeVector2(x, y),
                Gaze::GazeVector2(x + w, y),
                Gaze::GazeVector2(x + w, y + h),
                Gaze::GazeVector2(x, y + h)
            };
            if (std::abs(roll_hint_rad) > 1e-4f) {
                for (auto& c : corners) {
                    c = Gaze::rotate_point_back(c, -roll_hint_rad, frame.width, frame.height);
                }
            }
            return corners;
        };

        auto yunet_corners = rotate_corners_back(bbox.x, bbox.y, bbox.width, bbox.height);
        auto lm_crop_corners = rotate_corners_back(adj_bx, adj_by, adj_bw, adj_bh);

        // Eye crop corners in upright space
        float eye_dist_up = (landmarks_35_upright[0] - landmarks_35_upright[1]).length();
        float crop_sz_r = eye_dist_up * 1.5f;
        Gaze::GazeVector2 r_center = (landmarks_35_upright[0] + landmarks_35_upright[1]) * 0.5f;
        auto r_eye_corners = rotate_corners_back(r_center.x - crop_sz_r * 0.5f, r_center.y - crop_sz_r * 0.5f, crop_sz_r, crop_sz_r);

        float eye_dist_l_up = (landmarks_35_upright[2] - landmarks_35_upright[3]).length();
        float crop_sz_l = eye_dist_l_up * 1.5f;
        Gaze::GazeVector2 l_center = (landmarks_35_upright[2] + landmarks_35_upright[3]) * 0.5f;
        auto l_eye_corners = rotate_corners_back(l_center.x - crop_sz_l * 0.5f, l_center.y - crop_sz_l * 0.5f, crop_sz_l, crop_sz_l);

        json_out << "    \"yunet_corners\": [";
        for (size_t c = 0; c < 4; ++c) json_out << "[" << yunet_corners[c].x << "," << yunet_corners[c].y << "]" << (c == 3 ? "" : ",");
        json_out << "],\n";

        json_out << "    \"lm_crop_corners\": [";
        for (size_t c = 0; c < 4; ++c) json_out << "[" << lm_crop_corners[c].x << "," << lm_crop_corners[c].y << "]" << (c == 3 ? "" : ",");
        json_out << "],\n";

        json_out << "    \"r_eye_crop_corners\": [";
        for (size_t c = 0; c < 4; ++c) json_out << "[" << r_eye_corners[c].x << "," << r_eye_corners[c].y << "]" << (c == 3 ? "" : ",");
        json_out << "],\n";

        json_out << "    \"l_eye_crop_corners\": [";
        for (size_t c = 0; c < 4; ++c) json_out << "[" << l_eye_corners[c].x << "," << l_eye_corners[c].y << "]" << (c == 3 ? "" : ",");
        json_out << "],\n";

        json_out << "    \"left_crop_b64\": \"" << left_crop_b64 << "\",\n";
        json_out << "    \"right_crop_b64\": \"" << right_crop_b64 << "\",\n";

        json_out << "    \"landmarks_35\": [\n";
        for (int i = 0; i < 35; ++i)
        {
            json_out << "      [" << landmarks_35_orig[i].x << ", " << landmarks_35_orig[i].y << "]" << (i == 34 ? "" : ",") << "\n";
        }
        json_out << "    ],\n";

        json_out << "    \"head_origin_cam\": [" << head_origin_godot.x << ", " << head_origin_godot.y << ", " << head_origin_godot.z << "],\n";
        json_out << "    \"head_fwd_cam\": [" << head_fwd_godot.x << ", " << head_fwd_godot.y << ", " << head_fwd_godot.z << "],\n";
        json_out << "    \"head_euler_deg\": [" << euler_godot.x << ", " << euler_godot.y << ", " << euler_godot.z << "],\n";

        json_out << "    \"head_axis_2d\": {\n";
        json_out << "      \"origin\": [" << head_px_origin.x << ", " << head_px_origin.y << "],\n";
        json_out << "      \"x_axis\": [" << head_px_x.x << ", " << head_px_x.y << "],\n";
        json_out << "      \"y_axis\": [" << head_px_y.x << ", " << head_px_y.y << "],\n";
        json_out << "      \"fwd_axis\": [" << head_px_fwd.x << ", " << head_px_fwd.y << "]\n";
        json_out << "    },\n";

        json_out << "    \"eye_origin_cam\": [" << eye_origin_godot.x << ", " << eye_origin_godot.y << ", " << eye_origin_godot.z << "],\n";
        json_out << "    \"eye_gaze_cam\": [" << gaze_dir_godot.x << ", " << gaze_dir_godot.y << ", " << gaze_dir_godot.z << "],\n";

        json_out << "    \"eye_axis_2d\": {\n";
        json_out << "      \"origin\": [" << eye_px_origin.x << ", " << eye_px_origin.y << "],\n";
        json_out << "      \"x_axis\": [" << eye_px_x.x << ", " << eye_px_x.y << "],\n";
        json_out << "      \"y_axis\": [" << eye_px_y.x << ", " << eye_px_y.y << "],\n";
        json_out << "      \"gaze_axis\": [" << eye_px_gaze.x << ", " << eye_px_gaze.y << "]\n";
        json_out << "    },\n";

        json_out << "    \"mbp14_screen\": {\n";
        json_out << "      \"nose_px\": [" << nose_target_px.x << ", " << nose_target_px.y << "],\n";
        json_out << "      \"nose_mm\": [" << nose_target_mm.x << ", " << nose_target_mm.y << "],\n";
        json_out << "      \"eye_px\": [" << eye_target_px.x << ", " << eye_target_px.y << "],\n";
        json_out << "      \"eye_mm\": [" << eye_target_mm.x << ", " << eye_target_mm.y << "]\n";
        json_out << "    }\n";

        json_out << "  }" << (f_idx == fixtures.size() - 1 ? "" : ",") << "\n";
    }

    json_out << "]\n";
    json_out.close();
    std::cout << "[Benchmark Exporter] Successfully wrote all 14 fixture benchmark results to benchmark_data.json!\n";
}

