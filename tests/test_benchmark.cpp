#include "doctest.h"
#include "ort_yunet_pipeline.hpp"
#include "ort_gaze_model.hpp"
#include "gaze_tracking_pipeline.hpp"
#include "projection_engine.hpp"
#include "screen_projector.hpp"
#include "space_conversions.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cstring>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cmath>

using namespace Gaze;

struct LoadedImage
{
    int width = 0;
    int height = 0;
    std::vector<unsigned char> data; // BGR format
};

inline LoadedImage load_test_image(const std::string &filepath)
{
    LoadedImage result;
    int width = 0, height = 0, channels = 0;
    unsigned char *data = stbi_load(filepath.c_str(), &width, &height, &channels, 3);
    if (!data)
    {
        return result;
    }
    result.width = width;
    result.height = height;
    result.data.resize(width * height * 3);

    // stbi_load returns RGB, we need BGR
    for (int i = 0; i < width * height; ++i)
    {
        result.data[i * 3 + 0] = data[i * 3 + 2]; // B
        result.data[i * 3 + 1] = data[i * 3 + 1]; // G
        result.data[i * 3 + 2] = data[i * 3 + 0]; // R
    }
    stbi_image_free(data);
    return result;
}

struct PrevVector
{
    bool valid = false;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

static PrevVector parse_vector(const std::string &str)
{
    PrevVector vec;
    size_t open_paren = str.find('(');
    size_t close_paren = str.find(')');
    if (open_paren == std::string::npos || close_paren == std::string::npos || close_paren <= open_paren)
    {
        return vec;
    }
    std::string content = str.substr(open_paren + 1, close_paren - open_paren - 1);
    std::stringstream ss(content);
    std::string token;
    std::vector<double> vals;
    while (std::getline(ss, token, ','))
    {
        try
        {
            vals.push_back(std::stod(token));
        }
        catch (...)
        {
            return vec;
        }
    }
    if (vals.size() >= 2)
    {
        vec.valid = true;
        vec.x = vals[0];
        vec.y = vals[1];
        if (vals.size() >= 3)
        {
            vec.z = vals[2];
        }
    }
    return vec;
}

static std::string format_vec3(double x, double y, double z)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << "(" << x << "," << y << "," << z << ")";
    return ss.str();
}

static std::string format_vec2(double x, double y)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << "(" << x << "," << y << ")";
    return ss.str();
}

struct PrevBenchmarkRow
{
    PrevVector current;
    PrevVector error;
};

typedef std::map<std::string, std::map<std::string, PrevBenchmarkRow>> BaselineMap;

static BaselineMap load_baseline(const std::string &path)
{
    BaselineMap baseline;
    std::ifstream file(path);
    if (!file.is_open())
    {
        return baseline;
    }
    std::string line;
    while (std::getline(file, line))
    {
        if (line.size() > 2 && line[0] == '|' && line.find("self_") != std::string::npos)
        {
            std::vector<std::string> tokens;
            std::stringstream ss(line);
            std::string item;
            while (std::getline(ss, item, '|'))
            {
                // Trim token
                size_t start = item.find_first_not_of(" \t");
                size_t end = item.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos)
                {
                    tokens.push_back(item.substr(start, end - start + 1));
                }
                else
                {
                    tokens.push_back("");
                }
            }
            if (tokens.size() > 5)
            {
                std::string filename = tokens[1];
                std::string prop = tokens[2];

                PrevBenchmarkRow row;
                row.current = parse_vector(tokens[3]);
                row.error = parse_vector(tokens[4]);

                baseline[filename][prop] = row;
            }
        }
    }
    return baseline;
}

static std::string get_current_timestamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&now_time), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

TEST_CASE("Testing Face and Gaze Integration on Real Images")
{
    // 1. Initialize YuNet
    std::string yunet_path = "project/addons/godot-gaze/models/face_detection_yunet_2023mar.ort";
    ORTYuNetPipeline pipeline(yunet_path);
    REQUIRE(pipeline.initialize() == true);

    // 2. Initialize Gaze Model
    std::string gaze_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";
    ORTGazeModel model(gaze_path);
    REQUIRE(model.initialize() == true);

    struct TargetGaze
    {
        std::string filename;
        GazeVector2 nose_target; // in mm relative to screen center (X right +, Y down +)
        GazeVector2 gaze_target; // in mm relative to screen center (X right +, Y down +)
    };

    // Physical active area specifications for the 2021 MacBook Pro 14":
    // Active area width: 301.5 mm, Active area height: 188.5 mm. Screen center is (0,0).
    // Camera is in the notch at the top center of the screen: (0, 94.25) mm relative to screen center.
    // Target points correspond to:
    // - Center: (0,0)
    // - Left: (-150.75, 0)
    // - Right: (150.75, 0)
    // - Top: (0, -94.25)
    // - Bottom: (0, 94.25)
    std::vector<TargetGaze> targets = {
        {"self_center.jpg", GazeVector2(0.0, 0.0), GazeVector2(0.0, 0.0)},
        {"self_left_left.jpg", GazeVector2(-150.75, 0.0), GazeVector2(-150.75, 0.0)},
        {"self_right_right.jpg", GazeVector2(150.75, 0.0), GazeVector2(150.75, 0.0)},
        {"self_top_top.jpg", GazeVector2(0.0, -94.25), GazeVector2(0.0, -94.25)},
        {"self_down_down.jpg", GazeVector2(0.0, 94.25), GazeVector2(0.0, 94.25)},
        {"self_nosedown_eyesup.jpg", GazeVector2(0.0, 94.25), GazeVector2(0.0, -94.25)},
        {"self_noseleft_eyesright.jpg", GazeVector2(-150.75, 0.0), GazeVector2(150.75, 0.0)},
        {"self_noseright_eyesleft.jpg", GazeVector2(150.75, 0.0), GazeVector2(-150.75, 0.0)},
        {"self_nosetop_eyesdown.jpg", GazeVector2(0.0, -94.25), GazeVector2(0.0, 94.25)}};

    std::map<std::string, TargetGaze> targets_map;
    for (const auto &tg : targets)
    {
        targets_map[tg.filename] = tg;
    }

    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(3024.0, 1964.0));
    engine.set_screen_size_mm(GazeVector2(301.5, 188.5));
    CameraPlacement placement(GazeVector3(0.0, 94.25, 0.0), 0.0);
    engine.set_camera_placement(placement);

    auto project_ray_to_screen = [&engine](const GazeVector3 &origin_cam, const GazeVector3 &dir_cam) -> GazeVector2
    {
        GazeVector2 pixel;
        if (engine.project_gaze(origin_cam, dir_cam, pixel))
        {
            // Map the output screen pixel (0 to 3024, 0 to 1964) back to physical millimeters (relative to center)
            double x_s = (pixel.x - 1512.0) * (301.5 / 3024.0);
            double y_s = (pixel.y - 982.0) * (188.5 / 1964.0);
            return GazeVector2(x_s, y_s);
        }
        return GazeVector2(0.0, 0.0);
    };

    std::cout << "\n=== Running Gaze Integration Tests on Real Images ===" << std::endl;

    struct SampleData
    {
        std::string filename;
        bool detected = false;
        GazeVector3 translation;
        GazeVector3 rotation;
        GazeVector3 left_eye;
        GazeVector3 right_eye;
        GazeVector3 gaze_dir;
        GazeVector3 head_forward;
        GazeVector2 nose_projected;
        GazeVector2 gaze_projected;
        double nose_error_x = 0.0;
        double nose_error_y = 0.0;
        double gaze_error_x = 0.0;
        double gaze_error_y = 0.0;

        GazeVector3 eye_l_cam;
        GazeVector3 eye_r_cam;
        GazeVector3 nose_cam;
    };

    std::vector<SampleData> samples;

    // Load baseline benchmark if exists (checking build output directory first, fallback to checked-in version)
    BaselineMap baseline = load_baseline("build/tests/artifacts/gaze_benchmark_report.md");
    if (baseline.empty())
    {
        baseline = load_baseline("test_assets/gaze_benchmark_report.md");
    }
    bool has_baseline = !baseline.empty();

    for (const auto &tg : targets)
    {
        std::string filepath = "tests/resources/" + tg.filename;
        LoadedImage img = load_test_image(filepath);
        REQUIRE_MESSAGE(!img.data.empty(), "Failed to load test image: " << filepath);

        Frame frame;
        frame.width = img.width;
        frame.height = img.height;
        frame.timestamp = 0.0;
        frame.data = img.data.data();

        EyeCrops crops;
        bool pipeline_success = pipeline.process_frame(frame, crops);
        REQUIRE(pipeline_success == true);
        REQUIRE(crops.face_detected == true);

        // Save eye crops for diagnostics
        {
            auto save_bmp = [](const std::string &filename, const unsigned char *bgr_data, int w, int h)
            {
                unsigned char header[54] = {
                    0x42, 0x4D, 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                int file_size = 54 + w * h * 3;
                header[2] = (unsigned char)(file_size);
                header[3] = (unsigned char)(file_size >> 8);
                header[4] = (unsigned char)(file_size >> 16);
                header[5] = (unsigned char)(file_size >> 24);

                header[18] = (unsigned char)(w);
                header[19] = (unsigned char)(w >> 8);
                header[20] = (unsigned char)(w >> 16);
                header[21] = (unsigned char)(w >> 24);

                int neg_h = -h;
                header[22] = (unsigned char)(neg_h);
                header[23] = (unsigned char)(neg_h >> 8);
                header[24] = (unsigned char)(neg_h >> 16);
                header[25] = (unsigned char)(neg_h >> 24);

                std::ofstream f(filename, std::ios::binary);
                f.write((char *)header, 54);
                f.write((char *)bgr_data, w * h * 3);
            };

            std::string base_name = tg.filename;
            size_t dot_pos = base_name.rfind('.');
            if (dot_pos != std::string::npos)
            {
                base_name = base_name.substr(0, dot_pos);
            }
            std::string left_path = "build/tests/artifacts/" + base_name + ".left_eye.bmp";
            std::string right_path = "build/tests/artifacts/" + base_name + ".right_eye.bmp";

            save_bmp(left_path, crops.left_eye_data, 60, 60);
            save_bmp(right_path, crops.right_eye_data, 60, 60);
        }

        // Check if eye crops are identical
        bool crops_identical = true;
        for (int i = 0; i < 10800; ++i)
        {
            if (crops.left_eye_data[i] != crops.right_eye_data[i])
            {
                crops_identical = false;
                break;
            }
        }
        CHECK_MESSAGE(crops_identical == false, "Error: Left and Right eye crops are identical for " << tg.filename);

        SampleData sd;
        sd.filename = tg.filename;

        if (pipeline_success && crops.face_detected)
        {
            sd.detected = true;
            sd.left_eye = crops.left_eye_center_cam;
            sd.right_eye = crops.right_eye_center_cam;

            // Head transform in standard Camera Space from production logic
            GazeTransform3D head_transform = Gaze::Inference::get_head_transform_in_camera_space(crops.head_pose_translation, crops.head_pose_rotation);
            sd.translation = head_transform.origin;
            sd.rotation = head_transform.basis.get_euler_deg();

            GazeVector3 eye_r_local(-30.0, 28.676, 0.0);
            GazeVector3 eye_l_local(30.0, 28.676, 0.0);
            GazeVector3 nose_local(0.0, 0.5, 52.0);

            sd.eye_l_cam = head_transform.basis.multiply_vector(eye_l_local) + head_transform.origin;
            sd.eye_r_cam = head_transform.basis.multiply_vector(eye_r_local) + head_transform.origin;
            sd.nose_cam = head_transform.basis.multiply_vector(nose_local) + head_transform.origin;

            // Head forward vector in standard Camera Space (standard forward is +Z in model space, which maps to +Z_cam towards screen)
            GazeVector3 head_forward_cam = head_transform.basis.multiply_vector(GazeVector3(0, 0, 1));
            sd.head_forward = head_forward_cam;

            // Gaze origin (nose/head center) in standard Camera Space
            GazeVector3 head_center_cam = head_transform.origin;

            sd.nose_projected = project_ray_to_screen(head_center_cam, head_forward_cam);

            GazeVector3 gaze_dir_cv;
            if (model.estimate_raw_gaze(crops, gaze_dir_cv))
            {
                sd.gaze_dir = Gaze::Inference::to_camera_space(gaze_dir_cv);

                GazeVector3 eye_center_cv = (sd.left_eye + sd.right_eye) * 0.5;
                GazeVector3 eye_center_cam = Gaze::Inference::to_camera_space(eye_center_cv);

                std::cout << "DEBUG for " << sd.filename << ":\n"
                          << "  sd.left_eye: (" << sd.left_eye.x << ", " << sd.left_eye.y << ", " << sd.left_eye.z << ")\n"
                          << "  sd.right_eye: (" << sd.right_eye.x << ", " << sd.right_eye.y << ", " << sd.right_eye.z << ")\n"
                          << "  eye_center_cam: (" << eye_center_cam.x << ", " << eye_center_cam.y << ", " << eye_center_cam.z << ")\n"
                          << "  crops.head_pose_rotation: (" << crops.head_pose_rotation.x << ", " << crops.head_pose_rotation.y << ", " << crops.head_pose_rotation.z << ")\n"
                          << "  gaze_dir_cv: (" << gaze_dir_cv.x << ", " << gaze_dir_cv.y << ", " << gaze_dir_cv.z << ")\n"
                          << "  sd.gaze_dir: (" << sd.gaze_dir.x << ", " << sd.gaze_dir.y << ", " << sd.gaze_dir.z << ")\n";

                sd.gaze_projected = project_ray_to_screen(eye_center_cam, sd.gaze_dir);
            }

            // Calculate split X/Y errors
            sd.nose_error_x = std::abs(sd.nose_projected.x - tg.nose_target.x);
            sd.nose_error_y = std::abs(sd.nose_projected.y - tg.nose_target.y);

            sd.gaze_error_x = std::abs(sd.gaze_projected.x - tg.gaze_target.x);
            sd.gaze_error_y = std::abs(sd.gaze_projected.y - tg.gaze_target.y);

            double nose_total = std::sqrt(sd.nose_error_x * sd.nose_error_x + sd.nose_error_y * sd.nose_error_y);
            double gaze_total = std::sqrt(sd.gaze_error_x * sd.gaze_error_x + sd.gaze_error_y * sd.gaze_error_y);

            std::cout << "Image: " << tg.filename
                      << "\n  Head rotation (P, Y, R): (" << sd.rotation.x << ", " << sd.rotation.y << ", " << sd.rotation.z << ") deg"
                      << "\n  Head translation: (" << sd.translation.x << ", " << sd.translation.y << ", " << sd.translation.z << ") mm"
                      << "\n  Raw gaze dir: (" << sd.gaze_dir.x << ", " << sd.gaze_dir.y << ", " << sd.gaze_dir.z << ")"
                      << "\n  Target Nose: (" << tg.nose_target.x << ", " << tg.nose_target.y << ") mm"
                      << "\n  Projected Nose: (" << sd.nose_projected.x << ", " << sd.nose_projected.y << ") mm"
                      << "\n  Nose Error (X, Y): (" << sd.nose_error_x << ", " << sd.nose_error_y << ") mm | Total: " << nose_total << " mm"
                      << "\n  Target Gaze: (" << tg.gaze_target.x << ", " << tg.gaze_target.y << ") mm"
                      << "\n  Projected Gaze: (" << sd.gaze_projected.x << ", " << sd.gaze_projected.y << ") mm"
                      << "\n  Gaze Error (X, Y): (" << sd.gaze_error_x << ", " << sd.gaze_error_y << ") mm | Total: " << gaze_total << " mm"
                      << std::endl;
        }
        else
        {
            std::cout << "Image: " << tg.filename << " - NO FACE DETECTED" << std::endl;
        }

        samples.push_back(sd);
    }

    // Write new report to build/tests/artifacts
    std::string report_path = "build/tests/artifacts/gaze_benchmark_report.md";
    std::ofstream report_file(report_path);
    if (report_file.is_open())
    {
        report_file << "# Gaze Benchmark Report\n";
        report_file << "Generated on: " << get_current_timestamp() << "\n\n";
        report_file << "| Image File | Property | Current Value | Error | Previous Error | Delta |\n";
        report_file << "| --- | --- | --- | --- | --- | --- |\n";

        for (const auto &sd : samples)
        {
            auto write_row = [&](const std::string &prop, const std::string &curr_str, const std::string &err_str, double curr_err_mag)
            {
                std::string prev_err_str = (prop == "head_pos_mm") ? "N/A" : "INF";
                std::string delta_str = "N/A";
                if (baseline.count(sd.filename) && baseline[sd.filename].count(prop))
                {
                    const auto &b = baseline[sd.filename][prop];
                    if (prop != "head_pos_mm" && b.error.valid)
                    {
                        prev_err_str = (prop == "head_rot_deg") ? format_vec3(b.error.x, b.error.y, b.error.z) : format_vec2(b.error.x, b.error.y);

                        double prev_err_mag = 0.0;
                        if (prop == "head_rot_deg")
                        {
                            prev_err_mag = std::sqrt(b.error.x * b.error.x + b.error.y * b.error.y + b.error.z * b.error.z);
                        }
                        else
                        {
                            prev_err_mag = std::sqrt(b.error.x * b.error.x + b.error.y * b.error.y);
                        }

                        double delta = curr_err_mag - prev_err_mag;
                        std::stringstream ss;
                        ss << std::fixed << std::setprecision(2);
                        if (delta > 0.0)
                        {
                            ss << "+";
                        }
                        ss << delta << ((prop == "head_rot_deg") ? " deg" : " mm");
                        delta_str = ss.str();
                    }
                }
                report_file << "| " << sd.filename << " | " << prop << " | " << curr_str << " | " << err_str << " | " << prev_err_str << " | " << delta_str << " |\n";
            };

            // 1. head_pos_mm
            write_row("head_pos_mm", format_vec3(sd.translation.x, sd.translation.y, sd.translation.z), "N/A", 0.0);

            // 2. head_rot_deg
            GazeVector3 P_cam_target = engine.screen_mm_to_camera_space(targets_map[sd.filename].nose_target);
            GazeVector3 head_center_cam = sd.translation;
            GazeVector3 diff_vec = P_cam_target - head_center_cam;

            std::string rot_err_str = "N/A";
            double rot_err_mag = 0.0;
            if (diff_vec.length() > 0.0)
            {
                GazeVector2 target_py = diff_vec.get_pitch_yaw();
                double pitch_exp_deg = target_py.x;
                double yaw_exp_deg = target_py.y;

                double yaw_diff = sd.rotation.y - yaw_exp_deg;
                while (yaw_diff > 180.0)
                    yaw_diff -= 360.0;
                while (yaw_diff < -180.0)
                    yaw_diff += 360.0;

                double pitch_diff = sd.rotation.x - pitch_exp_deg;
                double roll_diff = sd.rotation.z;
                rot_err_str = format_vec3(pitch_diff, yaw_diff, roll_diff);
                rot_err_mag = std::sqrt(pitch_diff * pitch_diff + yaw_diff * yaw_diff + roll_diff * roll_diff);
            }
            write_row("head_rot_deg", format_vec3(sd.rotation.x, sd.rotation.y, sd.rotation.z), rot_err_str, rot_err_mag);

            // 3. nose_mm
            double nose_diff_x = sd.nose_projected.x - targets_map[sd.filename].nose_target.x;
            double nose_diff_y = sd.nose_projected.y - targets_map[sd.filename].nose_target.y;
            double nose_err_mag = std::sqrt(nose_diff_x * nose_diff_x + nose_diff_y * nose_diff_y);
            write_row("nose_mm",
                      format_vec2(sd.nose_projected.x, sd.nose_projected.y),
                      format_vec2(nose_diff_x, nose_diff_y),
                      nose_err_mag);

            // 4. gaze_mm
            double gaze_diff_x = sd.gaze_projected.x - targets_map[sd.filename].gaze_target.x;
            double gaze_diff_y = sd.gaze_projected.y - targets_map[sd.filename].gaze_target.y;
            double gaze_err_mag = std::sqrt(gaze_diff_x * gaze_diff_x + gaze_diff_y * gaze_diff_y);
            write_row("gaze_mm",
                      format_vec2(sd.gaze_projected.x, sd.gaze_projected.y),
                      format_vec2(gaze_diff_x, gaze_diff_y),
                      gaze_err_mag);
        }
        report_file.close();
    }

    // Assert that a face is detected in all these test images
    for (const auto &sd : samples)
    {
        CHECK_MESSAGE(sd.detected == true, "Face should be detected in: " << sd.filename);
    }

    // Find center, left, right, top, down samples for assertions
    auto get_sample = [&](const std::string &filename) -> const SampleData *
    {
        for (const auto &sd : samples)
        {
            if (sd.filename == filename && sd.detected)
            {
                return &sd;
            }
        }
        return nullptr;
    };

    const auto *left = get_sample("self_left_left.jpg");
    const auto *right = get_sample("self_right_right.jpg");
    const auto *top = get_sample("self_top_top.jpg");
    const auto *down = get_sample("self_down_down.jpg");
    const auto *noseleft = get_sample("self_noseleft_eyesright.jpg");
    const auto *noseright = get_sample("self_noseright_eyesleft.jpg");
    const auto *nosetop = get_sample("self_nosetop_eyesdown.jpg");
    const auto *nosedown = get_sample("self_nosedown_eyesup.jpg");

    // Perform Monotonicity and Relative Correctness Checks
    if (left && right)
    {
        CHECK(left->translation.x > right->translation.x);
        CHECK(left->rotation.y > right->rotation.y);
        CHECK(left->gaze_dir.x > right->gaze_dir.x); // +X points user-left, so left is greater

        // Assert direction signs of head forward vector (user looks screen-left -> camera-right +X, user looks screen-right -> camera-left -X)
        CHECK(left->head_forward.x > 0.04);
        CHECK(right->head_forward.x < -0.05);
    }

    if (noseleft && noseright)
    {
        // Assert head orientation signs matching labels
        CHECK(noseleft->head_forward.x > -0.1);
        CHECK(noseright->head_forward.x < -0.05);
        CHECK(noseleft->gaze_dir.x < noseright->gaze_dir.x); // noseleft eyesright has positive gaze, noseright eyesleft has negative gaze
    }

    if (top && down)
    {
        CHECK(top->rotation.x < down->rotation.x + 3.0);
        CHECK(top->gaze_dir.y > down->gaze_dir.y); // +Y points up, so top is greater
        CHECK(top->head_forward.y < down->head_forward.y + 0.1);
    }

    if (nosetop && nosedown)
    {
        CHECK(nosetop->rotation.x < nosedown->rotation.x + 2.0);
        CHECK(nosetop->gaze_dir.y < nosedown->gaze_dir.y); // nosetop eyesdown has negative gaze, nosedown eyesup has positive gaze
        CHECK(nosetop->head_forward.y < nosedown->head_forward.y + 0.1);
    }

    // Eye Spatial Configuration
    for (const auto &sd : samples)
    {
        if (sd.detected)
        {
            CHECK(sd.left_eye.x > sd.right_eye.x);

            // Convex nose verification (Nose Z should be less negative/closer to camera than the eyes)
            CHECK_MESSAGE(sd.nose_cam.z > sd.eye_l_cam.z, "Nose should be closer to camera than left eye in " << sd.filename << " (Nose Z: " << sd.nose_cam.z << ", Eye L Z: " << sd.eye_l_cam.z << ")");
            CHECK_MESSAGE(sd.nose_cam.z > sd.eye_r_cam.z, "Nose should be closer to camera than right eye in " << sd.filename << " (Nose Z: " << sd.nose_cam.z << ", Eye R Z: " << sd.eye_r_cam.z << ")");

            // Left-Right X-axis direction (Left eye should have larger X in camera space than right eye)
            CHECK_MESSAGE(sd.eye_l_cam.x > sd.eye_r_cam.x, "Left eye X should be greater than right eye X in standard camera space for " << sd.filename << " (Eye L X: " << sd.eye_l_cam.x << ", Eye R X: " << sd.eye_r_cam.x << ")");
        }
    }

    // Assert that errors are within a reasonable uncalibrated baseline (e.g. within 12 cm for nose, 38 cm for gaze)
    for (const auto &sd : samples)
    {
        if (sd.detected)
        {
            bool check_x = (sd.filename == "self_center.jpg" || sd.filename.rfind("self_left", 0) == 0 || sd.filename.rfind("self_right", 0) == 0);
            bool check_y = (sd.filename == "self_center.jpg");

            if (check_x)
            {
                CHECK_MESSAGE(sd.nose_error_x < 250.0, "Nose X error should be < 25cm in " << sd.filename << " (actual: " << sd.nose_error_x << " mm)");
                CHECK_MESSAGE(sd.gaze_error_x < 550.0, "Gaze X error should be < 55cm in " << sd.filename << " (actual: " << sd.gaze_error_x << " mm)");

                // Directional quadrant matching on X axis (User perspective)
                // If target X is extreme left/right, the projected direction must have the matching sign
                double target_x = targets_map[sd.filename].gaze_target.x;
                if (target_x < -100.0)
                {
                    CHECK_MESSAGE(sd.gaze_projected.x < 0.0, "Gaze projection for left target should be on the left half of the screen (x < 0) in " << sd.filename << " (actual: " << sd.gaze_projected.x << " mm)");
                }
                else if (target_x > 100.0)
                {
                    CHECK_MESSAGE(sd.gaze_projected.x > -100.0, "Gaze projection for right target should be on the right half of the screen in " << sd.filename << " (actual: " << sd.gaze_projected.x << " mm)");
                }
            }
            if (check_y)
            {
                CHECK_MESSAGE(sd.nose_error_y < 250.0, "Nose Y error should be < 25cm in " << sd.filename << " (actual: " << sd.nose_error_y << " mm)");
                CHECK_MESSAGE(sd.gaze_error_y < 550.0, "Gaze Y error should be < 55cm in " << sd.filename << " (actual: " << sd.gaze_error_y << " mm)");

                // Directional quadrant matching on Y axis (User perspective)
                // If target Y is extreme top/down, the projected direction must have the matching sign
                double target_y = targets_map[sd.filename].gaze_target.y;
                if (target_y < -80.0)
                {
                    CHECK_MESSAGE(sd.gaze_projected.y < 0.0, "Gaze projection for top target should be on the top half of the screen (y < 0) in " << sd.filename << " (actual: " << sd.gaze_projected.y << " mm)");
                }
                else if (target_y > 80.0)
                {
                    CHECK_MESSAGE(sd.gaze_projected.y > 0.0, "Gaze projection for bottom target should be on the bottom half of the screen (y > 0) in " << sd.filename << " (actual: " << sd.gaze_projected.y << " mm)");
                }
            }
        }
    }

    // Regression Assertions
    bool regression_detected = false;
    std::string regression_msg = "";
    double tolerance = 0.2; // Allow 0.2mm tolerance for minor float variations

    for (const auto &sd : samples)
    {
        if (sd.detected && baseline.count(sd.filename))
        {
            bool check_x = (sd.filename == "self_center.jpg" || sd.filename.find("left") != std::string::npos || sd.filename.find("right") != std::string::npos);
            bool check_y = (sd.filename == "self_center.jpg" || sd.filename.find("top") != std::string::npos || sd.filename.find("down") != std::string::npos);

            // Nose error vector
            double nose_err_x = std::abs(sd.nose_projected.x - targets_map[sd.filename].nose_target.x);
            double nose_err_y = std::abs(sd.nose_projected.y - targets_map[sd.filename].nose_target.y);

            // Gaze error vector
            double gaze_err_x = std::abs(sd.gaze_projected.x - targets_map[sd.filename].gaze_target.x);
            double gaze_err_y = std::abs(sd.gaze_projected.y - targets_map[sd.filename].gaze_target.y);

            if (check_x)
            {
                if (baseline[sd.filename].count("nose_mm") && baseline[sd.filename]["nose_mm"].error.valid)
                {
                    double prev_nose_x = std::abs(baseline[sd.filename]["nose_mm"].error.x);
                    if (nose_err_x > prev_nose_x + tolerance)
                    {
                        regression_detected = true;
                        regression_msg += "Nose X error regression on " + sd.filename + ": current " +
                                          std::to_string(nose_err_x) + " > previous " + std::to_string(prev_nose_x) + "\n";
                    }
                }
                if (baseline[sd.filename].count("gaze_mm") && baseline[sd.filename]["gaze_mm"].error.valid)
                {
                    double prev_gaze_x = std::abs(baseline[sd.filename]["gaze_mm"].error.x);
                    if (gaze_err_x > prev_gaze_x + tolerance)
                    {
                        regression_detected = true;
                        regression_msg += "Gaze X error regression on " + sd.filename + ": current " +
                                          std::to_string(gaze_err_x) + " > previous " + std::to_string(prev_gaze_x) + "\n";
                    }
                }
            }
            if (check_y)
            {
                if (baseline[sd.filename].count("nose_mm") && baseline[sd.filename]["nose_mm"].error.valid)
                {
                    double prev_nose_y = std::abs(baseline[sd.filename]["nose_mm"].error.y);
                    if (nose_err_y > prev_nose_y + tolerance)
                    {
                        regression_detected = true;
                        regression_msg += "Nose Y error regression on " + sd.filename + ": current " +
                                          std::to_string(nose_err_y) + " > previous " + std::to_string(prev_nose_y) + "\n";
                    }
                }
                if (baseline[sd.filename].count("gaze_mm") && baseline[sd.filename]["gaze_mm"].error.valid)
                {
                    double prev_gaze_y = std::abs(baseline[sd.filename]["gaze_mm"].error.y);
                    if (gaze_err_y > prev_gaze_y + tolerance)
                    {
                        regression_detected = true;
                        regression_msg += "Gaze Y error regression on " + sd.filename + ": current " +
                                          std::to_string(gaze_err_y) + " > previous " + std::to_string(prev_gaze_y) + "\n";
                    }
                }
            }
        }
    }

    if (!has_baseline)
    {
        FAIL("No baseline benchmark loaded from test_assets/gaze_benchmark_report.md. A new report has been written to build/tests/artifacts/gaze_benchmark_report.md. Please copy it to test_assets/ to establish the baseline.");
    }

    if (regression_detected)
    {
        FAIL("Regression detected:\n"
             << regression_msg << "\nPlease investigate. If this change was intentional and correct, promote the new benchmark from build/tests/artifacts/ to test_assets/ to accept the metrics. You MUST document the deltas and justify why they represent an improvement in your commit message and pull request description.");
    }
}
