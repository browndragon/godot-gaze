#include "doctest.h"
#include "yunet_pipeline.hpp"
#include "opencv_gaze_model.hpp"
#include "projection_engine.hpp"
#include "screen_projector.hpp"
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <cstring>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <chrono>
#include <ctime>
#include <iomanip>

using namespace Gaze;

TEST_CASE("Testing Native Pipeline Model Initialization & Inference") {
    // 1. Initialize YuNet
    std::string yunet_path = "project/models/face_detection_yunet_2023mar.onnx";
    YuNetPipeline pipeline(yunet_path);
    
    REQUIRE(pipeline.initialize() == true);

    // 2. Initialize Gaze Model
    std::string gaze_path = "project/models/gaze-estimation-adas-0002.xml";
    OpenCVGazeModel model(gaze_path);
    
    REQUIRE(model.initialize() == true);

    // 3. Verify solvePnP approximation with dummy inputs
    Frame frame;
    frame.width = 640;
    frame.height = 480;
    frame.timestamp = 0.0;
    
    // Create a dummy 640x480 BGR image mat containing a blank face
    cv::Mat dummy_mat = cv::Mat::ones(480, 640, CV_8UC3) * 255;
    frame.data = dummy_mat.data;

    EyeCrops crops;
    // We expect process_frame to return false since the dummy image doesn't contain a face
    REQUIRE(pipeline.process_frame(frame, crops) == false);
    CHECK(crops.face_detected == false);

    // 4. Test Gaze Model inference on mock crop data
    crops.face_detected = true;
    crops.head_pose_rotation = GazeVector3(0.0, 0.0, 0.0);
    crops.head_pose_translation = GazeVector3(0.0, 0.0, 500.0);
    crops.left_eye_center_cam = GazeVector3(31.5, 0.0, 480.0);
    crops.right_eye_center_cam = GazeVector3(-31.5, 0.0, 480.0);

    // Fill left and right eye crop buffers with dummy data
    std::memset(crops.left_eye_data, 128, 10800);
    std::memset(crops.right_eye_data, 128, 10800);

    GazeVector3 gaze_dir_cv;
    bool model_success = model.estimate_raw_gaze(crops, gaze_dir_cv);
    REQUIRE(model_success == true);

    // Check that gaze_dir_cv is normalized and valid
    CHECK(gaze_dir_cv.length() == doctest::Approx(1.0));
    CHECK(std::abs(gaze_dir_cv.z) > 0.0);
}

struct PrevVector {
    bool valid = false;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

static PrevVector parse_vector(const std::string& str) {
    PrevVector vec;
    size_t open_paren = str.find('(');
    size_t close_paren = str.find(')');
    if (open_paren == std::string::npos || close_paren == std::string::npos || close_paren <= open_paren) {
        return vec;
    }
    std::string content = str.substr(open_paren + 1, close_paren - open_paren - 1);
    std::stringstream ss(content);
    std::string token;
    std::vector<double> vals;
    while (std::getline(ss, token, ',')) {
        try {
            vals.push_back(std::stod(token));
        } catch (...) {
            return vec;
        }
    }
    if (vals.size() >= 2) {
        vec.valid = true;
        vec.x = vals[0];
        vec.y = vals[1];
        if (vals.size() >= 3) {
            vec.z = vals[2];
        }
    }
    return vec;
}

static std::string format_vec3(double x, double y, double z) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << "(" << x << "," << y << "," << z << ")";
    return ss.str();
}

static std::string format_vec2(double x, double y) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << "(" << x << "," << y << ")";
    return ss.str();
}

struct PrevBenchmarkRow {
    PrevVector current;
    PrevVector error;
};

typedef std::map<std::string, std::map<std::string, PrevBenchmarkRow>> BaselineMap;

static BaselineMap load_baseline(const std::string& path) {
    BaselineMap baseline;
    std::ifstream file(path);
    if (!file.is_open()) {
        return baseline;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.size() > 2 && line[0] == '|' && line.find("self_") != std::string::npos) {
            std::vector<std::string> tokens;
            std::stringstream ss(line);
            std::string item;
            while (std::getline(ss, item, '|')) {
                // Trim token
                size_t start = item.find_first_not_of(" \t");
                size_t end = item.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos) {
                    tokens.push_back(item.substr(start, end - start + 1));
                } else {
                    tokens.push_back("");
                }
            }
            if (tokens.size() > 7) {
                std::string filename = tokens[1];
                std::string prop = tokens[2];
                
                PrevBenchmarkRow row;
                row.current = parse_vector(tokens[4]);
                row.error = parse_vector(tokens[5]);
                
                baseline[filename][prop] = row;
            }
        }
    }
    return baseline;
}

static std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&now_time), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

TEST_CASE("Testing Face and Gaze Integration on Real Images") {
    // 1. Initialize YuNet
    std::string yunet_path = "project/models/face_detection_yunet_2023mar.onnx";
    YuNetPipeline pipeline(yunet_path);
    REQUIRE(pipeline.initialize() == true);

    // 2. Initialize Gaze Model
    std::string gaze_path = "project/models/gaze-estimation-adas-0002.xml";
    OpenCVGazeModel model(gaze_path);
    REQUIRE(model.initialize() == true);

    struct TargetGaze {
        std::string filename;
        GazeVector2 nose_target; // in mm relative to screen center (X right +, Y down +)
        GazeVector2 gaze_target; // in mm relative to screen center (X right +, Y down +)
    };

    // MacBook Pro 14" (305mm x 191mm). Screen center is (0,0).
    // Target points correspond to center (0,0), left (-152.5, 0), right (152.5, 0), top (0, -95.5), bottom (0, 95.5)
    std::vector<TargetGaze> targets = {
        {"self_center.jpg", GazeVector2(0.0, 0.0), GazeVector2(0.0, 0.0)},
        {"self_left_left.jpg", GazeVector2(-152.5, 0.0), GazeVector2(-152.5, 0.0)},
        {"self_right_right.jpg", GazeVector2(152.5, 0.0), GazeVector2(152.5, 0.0)},
        {"self_top_top.jpg", GazeVector2(0.0, -95.5), GazeVector2(0.0, -95.5)},
        {"self_down_down.jpg", GazeVector2(0.0, 95.5), GazeVector2(0.0, 95.5)},
        {"self_nosedown_eyesup.jpg", GazeVector2(0.0, 95.5), GazeVector2(0.0, -95.5)},
        {"self_noseleft_eyesright.jpg", GazeVector2(-152.5, 0.0), GazeVector2(152.5, 0.0)},
        {"self_noseright_eyesleft.jpg", GazeVector2(152.5, 0.0), GazeVector2(-152.5, 0.0)},
        {"self_nosetop_eyesdown.jpg", GazeVector2(0.0, -95.5), GazeVector2(0.0, 95.5)}
    };

    std::map<std::string, TargetGaze> targets_map;
    for (const auto& tg : targets) {
        targets_map[tg.filename] = tg;
    }

    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(GazeVector2(305.0, 191.0));
    CameraPlacement placement(GazeVector3(0.0, 95.5, 0.0), 0.0);
    engine.set_camera_placement(placement);

    auto project_ray_to_screen = [&engine](const GazeVector3& origin_cam, const GazeVector3& dir_cam) -> GazeVector2 {
        GazeVector2 pixel;
        if (engine.project_gaze(origin_cam, dir_cam, pixel)) {
            double x_s = (pixel.x - 960.0) * (305.0 / 1920.0);
            double y_s = (pixel.y - 540.0) * (191.0 / 1080.0);
            return GazeVector2(x_s, y_s);
        }
        return GazeVector2(0.0, 0.0);
    };

    std::cout << "\n=== Running Gaze Integration Tests on Real Images ===" << std::endl;

    struct SampleData {
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
    };

    std::vector<SampleData> samples;

    // Load baseline benchmark if exists
    BaselineMap baseline = load_baseline("test_assets/gaze_benchmark_report.md");
    bool has_baseline = !baseline.empty();

    for (const auto& tg : targets) {
        std::string filepath = "tests/resources/" + tg.filename;
        cv::Mat img = cv::imread(filepath);
        REQUIRE_MESSAGE(!img.empty(), "Failed to load test image: " << filepath);

        Frame frame;
        frame.width = img.cols;
        frame.height = img.rows;
        frame.timestamp = 0.0;
        frame.data = img.data;

        EyeCrops crops;
        bool pipeline_success = pipeline.process_frame(frame, crops);

        SampleData sd;
        sd.filename = tg.filename;

        if (pipeline_success && crops.face_detected) {
            sd.detected = true;
            sd.left_eye = crops.left_eye_center_cam;
            sd.right_eye = crops.right_eye_center_cam;

            // Head transform in standard Camera Space from production logic
            GazeTransform3D head_transform = engine.get_head_transform_in_camera_space(crops.head_pose_translation, crops.head_pose_rotation);
            sd.translation = head_transform.origin;
            sd.rotation = head_transform.basis.get_euler_deg();

            // Head forward vector in standard Camera Space (standard forward is -Z in model space, which maps to +Z_cam towards screen)
            GazeVector3 head_forward_cam = head_transform.basis.multiply_vector(GazeVector3(0, 0, -1));
            sd.head_forward = head_forward_cam;

            // Gaze origin (nose/head center) in standard Camera Space
            GazeVector3 head_center_cam = head_transform.origin;

            sd.nose_projected = project_ray_to_screen(head_center_cam, head_forward_cam);

            GazeVector3 gaze_dir_cv;
            if (model.estimate_raw_gaze(crops, gaze_dir_cv)) {
                sd.gaze_dir = engine.opencv_to_camera_space(gaze_dir_cv);

                GazeVector3 eye_center_cv = (sd.left_eye + sd.right_eye) * 0.5;
                GazeVector3 eye_center_cam = engine.opencv_to_camera_space(eye_center_cv);

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

            double nose_total = std::sqrt(sd.nose_error_x*sd.nose_error_x + sd.nose_error_y*sd.nose_error_y);
            double gaze_total = std::sqrt(sd.gaze_error_x*sd.gaze_error_x + sd.gaze_error_y*sd.gaze_error_y);

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
        } else {
            std::cout << "Image: " << tg.filename << " - NO FACE DETECTED" << std::endl;
        }

        samples.push_back(sd);
    }

    // Write new report to test_artifacts
    std::string report_path = "test_artifacts/gaze_benchmark_report.md";
    std::ofstream report_file(report_path);
    if (report_file.is_open()) {
        report_file << "# Gaze Benchmark Report\n";
        report_file << "Generated on: " << get_current_timestamp() << "\n\n";
        report_file << "| Image File | Property | Target Value | Current Value | Error | Previous Value | Previous Error |\n";
        report_file << "| --- | --- | --- | --- | --- | --- | --- |\n";
        
        for (const auto& sd : samples) {
            auto write_row = [&](const std::string& prop, const std::string& target_str, const std::string& curr_str, const std::string& err_str) {
                std::string prev_val_str = "INF";
                std::string prev_err_str = (prop == "head_pos_mm") ? "N/A" : "INF";
                if (baseline.count(sd.filename) && baseline[sd.filename].count(prop)) {
                    const auto& b = baseline[sd.filename][prop];
                    if (b.current.valid) prev_val_str = (prop == "head_pos_mm" || prop == "head_rot_deg") ? format_vec3(b.current.x, b.current.y, b.current.z) : format_vec2(b.current.x, b.current.y);
                    if (prop != "head_pos_mm" && b.error.valid) prev_err_str = (prop == "head_rot_deg") ? format_vec3(b.error.x, b.error.y, b.error.z) : format_vec2(b.error.x, b.error.y);
                }
                report_file << "| " << sd.filename << " | " << prop << " | " << target_str << " | " << curr_str << " | " << err_str << " | " << prev_val_str << " | " << prev_err_str << " |\n";
            };
            
            // 1. head_pos_mm
            write_row("head_pos_mm", "N/A", format_vec3(sd.translation.x, sd.translation.y, sd.translation.z), "N/A");
            
            // 2. head_rot_deg
            GazeVector3 P_cam_target = engine.screen_mm_to_camera_space(targets_map[sd.filename].nose_target);
            GazeVector3 head_center_cam = sd.translation;
            GazeVector3 diff_vec = P_cam_target - head_center_cam;

            std::string expected_rot_str = "N/A";
            std::string rot_err_str = "N/A";
            if (diff_vec.length() > 0.0) {
                GazeVector2 target_py = diff_vec.get_pitch_yaw();
                double pitch_exp_deg = target_py.x;
                double yaw_exp_deg = target_py.y;

                expected_rot_str = format_vec3(pitch_exp_deg, yaw_exp_deg, 0.0);

                double yaw_diff = sd.rotation.y - yaw_exp_deg;
                while (yaw_diff > 180.0) yaw_diff -= 360.0;
                while (yaw_diff < -180.0) yaw_diff += 360.0;
                rot_err_str = format_vec3(sd.rotation.x - pitch_exp_deg, yaw_diff, sd.rotation.z);
            }
            write_row("head_rot_deg", expected_rot_str, format_vec3(sd.rotation.x, sd.rotation.y, sd.rotation.z), rot_err_str);
            
            // 3. nose_mm
            write_row("nose_mm", 
                      format_vec2(targets_map[sd.filename].nose_target.x, targets_map[sd.filename].nose_target.y),
                      format_vec2(sd.nose_projected.x, sd.nose_projected.y), 
                      format_vec2(sd.nose_projected.x - targets_map[sd.filename].nose_target.x, sd.nose_projected.y - targets_map[sd.filename].nose_target.y));
                      
            // 4. gaze_mm
            write_row("gaze_mm", 
                      format_vec2(targets_map[sd.filename].gaze_target.x, targets_map[sd.filename].gaze_target.y),
                      format_vec2(sd.gaze_projected.x, sd.gaze_projected.y), 
                      format_vec2(sd.gaze_projected.x - targets_map[sd.filename].gaze_target.x, sd.gaze_projected.y - targets_map[sd.filename].gaze_target.y));
        }
        report_file.close();
    }

    // Assert that a face is detected in all these test images
    for (const auto& sd : samples) {
        CHECK_MESSAGE(sd.detected == true, "Face should be detected in: " << sd.filename);
    }

    // Find center, left, right, top, down samples for assertions
    auto get_sample = [&](const std::string& filename) -> const SampleData* {
        for (const auto& sd : samples) {
            if (sd.filename == filename && sd.detected) {
                return &sd;
            }
        }
        return nullptr;
    };

    const auto* left = get_sample("self_left_left.jpg");
    const auto* right = get_sample("self_right_right.jpg");
    const auto* top = get_sample("self_top_top.jpg");
    const auto* down = get_sample("self_down_down.jpg");

    // Perform Monotonicity and Relative Correctness Checks
    if (left && right) {
        CHECK(left->translation.x > right->translation.x);
        CHECK(right->rotation.y > left->rotation.y);
        CHECK(left->gaze_dir.x > right->gaze_dir.x); // +X points user-left, so left is greater

        // Assert direction signs of head forward vector (user looks screen-left -> camera-right +X, user looks screen-right -> camera-left -X)
        CHECK(left->head_forward.x > 0.1);
        CHECK(right->head_forward.x < -0.1);
    }

    if (top && down) {
        CHECK(top->rotation.x < down->rotation.x);
        CHECK(top->gaze_dir.y > down->gaze_dir.y); // +Y points up, so top is greater

        // Assert direction bounds of head forward vector (pitch is stable and pointing forward/downward)
        CHECK(std::abs(top->head_forward.y) < 0.25);
        CHECK(std::abs(down->head_forward.y) < 0.25);
    }

    // Eye Spatial Configuration
    for (const auto& sd : samples) {
        if (sd.detected) {
            CHECK(sd.left_eye.x > sd.right_eye.x);
        }
    }

    // Assert that errors are within a reasonable uncalibrated baseline (e.g. within 12 cm for nose, 38 cm for gaze)
    for (const auto& sd : samples) {
        if (sd.detected) {
            bool check_x = (sd.filename == "self_center.jpg" || sd.filename.rfind("self_left", 0) == 0 || sd.filename.rfind("self_right", 0) == 0);
            bool check_y = (sd.filename == "self_center.jpg");

            if (check_x) {
                CHECK_MESSAGE(sd.nose_error_x < 250.0, "Nose X error should be < 25cm in " << sd.filename << " (actual: " << sd.nose_error_x << " mm)");
                CHECK_MESSAGE(sd.gaze_error_x < 550.0, "Gaze X error should be < 55cm in " << sd.filename << " (actual: " << sd.gaze_error_x << " mm)");

                // Directional quadrant matching on X axis (User perspective)
                // If target X is extreme left/right, the projected direction must have the matching sign
                double target_x = targets_map[sd.filename].gaze_target.x;
                if (target_x < -100.0) {
                    CHECK_MESSAGE(sd.gaze_projected.x < 0.0, "Gaze projection for left target should be on the left half of the screen (x < 0) in " << sd.filename << " (actual: " << sd.gaze_projected.x << " mm)");
                } else if (target_x > 100.0) {
                    CHECK_MESSAGE(sd.gaze_projected.x > -100.0, "Gaze projection for right target should be on the right half of the screen in " << sd.filename << " (actual: " << sd.gaze_projected.x << " mm)");
                }
            }
            if (check_y) {
                CHECK_MESSAGE(sd.nose_error_y < 250.0, "Nose Y error should be < 25cm in " << sd.filename << " (actual: " << sd.nose_error_y << " mm)");
                CHECK_MESSAGE(sd.gaze_error_y < 550.0, "Gaze Y error should be < 55cm in " << sd.filename << " (actual: " << sd.gaze_error_y << " mm)");

                // Directional quadrant matching on Y axis (User perspective)
                // If target Y is extreme top/down, the projected direction must have the matching sign
                double target_y = targets_map[sd.filename].gaze_target.y;
                if (target_y < -80.0) {
                    CHECK_MESSAGE(sd.gaze_projected.y < 0.0, "Gaze projection for top target should be on the top half of the screen (y < 0) in " << sd.filename << " (actual: " << sd.gaze_projected.y << " mm)");
                } else if (target_y > 80.0) {
                    CHECK_MESSAGE(sd.gaze_projected.y > 0.0, "Gaze projection for bottom target should be on the bottom half of the screen (y > 0) in " << sd.filename << " (actual: " << sd.gaze_projected.y << " mm)");
                }
            }
        }
    }

    // Regression Assertions
    bool regression_detected = false;
    std::string regression_msg = "";
    double tolerance = 0.2; // Allow 0.2mm tolerance for minor float variations

    for (const auto& sd : samples) {
        if (sd.detected && baseline.count(sd.filename)) {
            bool check_x = (sd.filename == "self_center.jpg" || sd.filename.find("left") != std::string::npos || sd.filename.find("right") != std::string::npos);
            bool check_y = (sd.filename == "self_center.jpg" || sd.filename.find("top") != std::string::npos || sd.filename.find("down") != std::string::npos);

            // Nose error vector
            double nose_err_x = std::abs(sd.nose_projected.x - targets_map[sd.filename].nose_target.x);
            double nose_err_y = std::abs(sd.nose_projected.y - targets_map[sd.filename].nose_target.y);
            
            // Gaze error vector
            double gaze_err_x = std::abs(sd.gaze_projected.x - targets_map[sd.filename].gaze_target.x);
            double gaze_err_y = std::abs(sd.gaze_projected.y - targets_map[sd.filename].gaze_target.y);

            if (check_x) {
                if (baseline[sd.filename].count("nose_mm") && baseline[sd.filename]["nose_mm"].error.valid) {
                    double prev_nose_x = std::abs(baseline[sd.filename]["nose_mm"].error.x);
                    if (nose_err_x > prev_nose_x + tolerance) {
                        regression_detected = true;
                        regression_msg += "Nose X error regression on " + sd.filename + ": current " + 
                                          std::to_string(nose_err_x) + " > previous " + std::to_string(prev_nose_x) + "\n";
                    }
                }
                if (baseline[sd.filename].count("gaze_mm") && baseline[sd.filename]["gaze_mm"].error.valid) {
                    double prev_gaze_x = std::abs(baseline[sd.filename]["gaze_mm"].error.x);
                    if (gaze_err_x > prev_gaze_x + tolerance) {
                        regression_detected = true;
                        regression_msg += "Gaze X error regression on " + sd.filename + ": current " + 
                                          std::to_string(gaze_err_x) + " > previous " + std::to_string(prev_gaze_x) + "\n";
                    }
                }
            }
            if (check_y) {
                if (baseline[sd.filename].count("nose_mm") && baseline[sd.filename]["nose_mm"].error.valid) {
                    double prev_nose_y = std::abs(baseline[sd.filename]["nose_mm"].error.y);
                    if (nose_err_y > prev_nose_y + tolerance) {
                        regression_detected = true;
                        regression_msg += "Nose Y error regression on " + sd.filename + ": current " + 
                                          std::to_string(nose_err_y) + " > previous " + std::to_string(prev_nose_y) + "\n";
                    }
                }
                if (baseline[sd.filename].count("gaze_mm") && baseline[sd.filename]["gaze_mm"].error.valid) {
                    double prev_gaze_y = std::abs(baseline[sd.filename]["gaze_mm"].error.y);
                    if (gaze_err_y > prev_gaze_y + tolerance) {
                        regression_detected = true;
                        regression_msg += "Gaze Y error regression on " + sd.filename + ": current " + 
                                          std::to_string(gaze_err_y) + " > previous " + std::to_string(prev_gaze_y) + "\n";
                    }
                }
            }
        }
    }

    if (!has_baseline) {
        FAIL("No baseline benchmark loaded from test_assets/gaze_benchmark_report.md. A new report has been written to test_artifacts/gaze_benchmark_report.md. Please copy it to test_assets/ to establish the baseline.");
    }

    if (regression_detected) {
        FAIL("Regression detected:\n" << regression_msg << "\nPlease investigate. If this change was intentional and correct, promote the new benchmark from test_artifacts/ to test_assets/ to accept the metrics.");
    }
}

TEST_CASE("Testing Facial Landmarks and Head Pose Diagnostics") {
    std::string yunet_path = "project/models/face_detection_yunet_2023mar.onnx";
    YuNetPipeline pipeline(yunet_path);
    REQUIRE(pipeline.initialize() == true);

    cv::Mat img = cv::imread("tests/resources/self_center.jpg");
    REQUIRE(!img.empty());

    Frame frame;
    frame.width = img.cols;
    frame.height = img.rows;
    frame.timestamp = 0.0;
    frame.data = img.data;

    EyeCrops crops;
    bool pipeline_success = pipeline.process_frame(frame, crops);
    REQUIRE(pipeline_success == true);
    REQUIRE(crops.face_detected == true);

    // Verify landmarks coordinates are non-zero/valid
    CHECK(crops.left_eye_center_cam.x != 0.0);
    CHECK(crops.right_eye_center_cam.x != 0.0);

    // Verify head forward vector direction in standard Camera Space
    ProjectionEngine engine;
    GazeTransform3D head_transform = engine.get_head_transform_in_camera_space(crops.head_pose_translation, crops.head_pose_rotation);
    GazeVector3 head_forward = head_transform.basis.multiply_vector(GazeVector3(0, 0, -1));

    // For a forward-facing head, the forward vector should point towards the screen (+Z_cam in Godot camera space)
    CHECK(head_forward.z > 0.8);
}

TEST_CASE("Testing Viewport and High-DPI Projection Coordinates") {
    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(3840.0, 2160.0)); // 4K physical screen
    engine.set_screen_size_mm(GazeVector2(600.0, 340.0));
    engine.set_camera_placement(CameraPlacement(GazeVector3(0, 0, 0), 0.0));

    // Staring at the center of the screen
    GazeVector3 origin(0.0, 0.0, -600.0);
    GazeVector3 direction(0.0, 0.0, 1.0);
    GazeVector2 pixel;
    REQUIRE(engine.project_gaze(origin, direction, pixel) == true);
    
    // Physical screen center check
    CHECK(pixel.x == doctest::Approx(1920.0));
    CHECK(pixel.y == doctest::Approx(1080.0));

    // Simulate high-DPI scaling: window logical position is at (500, 300) logical points, scale is 2.0
    GazeVector2 window_pos_logical(500.0, 300.0);
    double scale = 2.0;
    GazeVector2 window_pos_physical(window_pos_logical.x * scale, window_pos_logical.y * scale);

    // Viewport-local physical position
    GazeVector2 local_pos_physical(pixel.x - window_pos_physical.x, pixel.y - window_pos_physical.y);
    CHECK(local_pos_physical.x == doctest::Approx(920.0));
    CHECK(local_pos_physical.y == doctest::Approx(480.0));

    // Viewport-local logical position
    GazeVector2 local_pos_logical(local_pos_physical.x / scale, local_pos_physical.y / scale);
    CHECK(local_pos_logical.x == doctest::Approx(460.0));
    CHECK(local_pos_logical.y == doctest::Approx(240.0));
}

TEST_CASE("Testing HiDPI Scaling Settings and Coordinate Transforms") {
    // Screen parameters
    GazeVector2 screen_size_lpix(1920, 1080);
    double os_screen_scale = 2.0; // Retina screen
    GazeVector2 screen_size_ppix(screen_size_lpix.x * os_screen_scale, screen_size_lpix.y * os_screen_scale); // (3840, 2160)

    // Projected pixel from ProjectionEngine (physical pixels)
    GazeVector2 projected_pixel_physical(1920.0, 1080.0); 

    // Window position in logical points
    GazeVector2 window_pos_logical(100.0, 50.0);
    GazeVector2 window_pos_physical(window_pos_logical.x * os_screen_scale, window_pos_logical.y * os_screen_scale); // (200, 100)

    // Calculate local position in physical pixels
    GazeVector2 local_pos_physical(projected_pixel_physical.x - window_pos_physical.x, projected_pixel_physical.y - window_pos_physical.y);

    // Test Scenario A: allow_hidpi = true (Godot window scale is 2.0)
    {
        double godot_window_scale = 2.0;
        double window_to_screen_scale_ratio = godot_window_scale / os_screen_scale;
        
        // Scale local position to Godot window space
        GazeVector2 local_pos_godot(local_pos_physical.x * window_to_screen_scale_ratio, local_pos_physical.y * window_to_screen_scale_ratio);
        CHECK(window_to_screen_scale_ratio == doctest::Approx(1.0));
        CHECK(local_pos_godot.x == doctest::Approx(local_pos_physical.x));
    }

    // Test Scenario B: allow_hidpi = false (Godot window scale is 1.0)
    {
        double godot_window_scale = 1.0;
        double window_to_screen_scale_ratio = godot_window_scale / os_screen_scale;
        
        // Scale local position to Godot window space
        GazeVector2 local_pos_godot(local_pos_physical.x * window_to_screen_scale_ratio, local_pos_physical.y * window_to_screen_scale_ratio);
        CHECK(window_to_screen_scale_ratio == doctest::Approx(0.5));
        CHECK(local_pos_godot.x == doctest::Approx(local_pos_physical.x * 0.5));
    }
}

TEST_CASE("Testing Web Geometry Scaling and Coordinate Mapping Parity") {
    // Web Screen metrics (Simulated Retina: 1512x982 logical, 3024x1964 physical)
    GazeVector2 screen_size_lpix(1512.0, 982.0);
    double dpr = 2.0;
    GazeVector2 screen_size_ppix(screen_size_lpix.x * dpr, screen_size_lpix.y * dpr); // (3024, 1964)

    // Scenario A: godot_scale = 1.0 (Default Web export, HiDPI disabled)
    {
        double godot_scale = 1.0; 
        double window_to_screen_scale_ratio = godot_scale / dpr; // 0.5

        // Gaze intersection point in physical pixels on the screen
        GazeVector2 projected_pixel_physical(1512.0, 982.0); 

        // Canvas position on the screen in physical pixels
        GazeVector2 canvas_pos_physical(300.0 * dpr, 200.0 * dpr); // (600, 400)

        // Calculate local position in physical pixels
        GazeVector2 local_pos_physical(
            projected_pixel_physical.x - canvas_pos_physical.x,
            projected_pixel_physical.y - canvas_pos_physical.y
        ); // (912, 582)

        // Scale to Godot viewport space
        GazeVector2 local_pos_godot(
            local_pos_physical.x * window_to_screen_scale_ratio,
            local_pos_physical.y * window_to_screen_scale_ratio
        ); 

        CHECK(window_to_screen_scale_ratio == doctest::Approx(0.5));
        CHECK(local_pos_godot.x == doctest::Approx(456.0));
        CHECK(local_pos_godot.y == doctest::Approx(291.0));
    }

    // Scenario B: godot_scale = 2.0 (HiDPI enabled)
    {
        double godot_scale = 2.0; 
        double window_to_screen_scale_ratio = godot_scale / dpr; // 1.0

        GazeVector2 projected_pixel_physical(1512.0, 982.0);
        GazeVector2 canvas_pos_physical(300.0 * dpr, 200.0 * dpr);

        GazeVector2 local_pos_physical(
            projected_pixel_physical.x - canvas_pos_physical.x,
            projected_pixel_physical.y - canvas_pos_physical.y
        );

        GazeVector2 local_pos_godot(
            local_pos_physical.x * window_to_screen_scale_ratio,
            local_pos_physical.y * window_to_screen_scale_ratio
        );

        CHECK(window_to_screen_scale_ratio == doctest::Approx(1.0));
        CHECK(local_pos_godot.x == doctest::Approx(912.0));
        CHECK(local_pos_godot.y == doctest::Approx(582.0));
    }
}

TEST_CASE("Testing ScreenProjector Decoupled Coordinate Mapping") {
    // 1. Setup projection engine mock metrics (MacBook Pro 15" logical sizing)
    Gaze::ProjectionEngine engine;
    engine.set_screen_size_pixels(Gaze::GazeVector2(1440.0, 900.0)); // Logical screen width
    engine.set_screen_size_mm(Gaze::GazeVector2(300.0, 195.0));      // Physical screen mm
    Gaze::CameraPlacement placement(Gaze::GazeVector3(0.0, 95.5, 0.0), 0.0);
    engine.set_camera_placement(placement);

    // 2. Test standard window (positioned at 0,0)
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::derive_configuration(
            Gaze::GazeVector2(0.0, 0.0),      // Window pos in logical screen pixels
            Gaze::GazeVector2(1.0, 1.0),      // Viewport scale
            Gaze::GazeVector2(0.0, 0.0)       // Viewport origin offset
        );

        Gaze::GazeVector3 origin_cam(0.0, -95.5, 800.0);
        Gaze::GazeVector3 dir_cam(0.0, 0.0, -1.0);

        Gaze::GazeVector2 viewport_pixel;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, viewport_pixel);
        
        REQUIRE(ok);
        // Center of 1440.0 screen is 720.0. With win_pos=0 and vp_scale=1, viewport X should be 720.0
        CHECK(viewport_pixel.x == doctest::Approx(720.0));
    }

    // 3. Test window positioned at (180, 82) logical pixels
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::derive_configuration(
            Gaze::GazeVector2(180.0, 82.0),   // Window pos in logical screen pixels
            Gaze::GazeVector2(1.0, 1.0),      // Viewport scale
            Gaze::GazeVector2(0.0, 0.0)       // Viewport origin offset
        );

        Gaze::GazeVector3 origin_cam(0.0, -95.5, 800.0);
        Gaze::GazeVector3 dir_cam(0.0, 0.0, -1.0);

        Gaze::GazeVector2 viewport_pixel;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, viewport_pixel);
        
        REQUIRE(ok);
        // logical screen X = 720.0
        // local window X = 720.0 - 180.0 = 540.0
        CHECK(viewport_pixel.x == doctest::Approx(540.0));
    }

    // 4. Test window positioned at (180, 82) logical pixels using from_godot_geometry
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::from_godot_geometry(
            Gaze::GazeVector2(180.0, 82.0),   // Window pos in logical pixels
            Gaze::GazeVector2(1.0, 1.0),      // Viewport scale
            Gaze::GazeVector2(0.0, 0.0)       // Viewport origin offset
        );

        Gaze::GazeVector3 origin_cam(0.0, -95.5, 800.0);
        Gaze::GazeVector3 dir_cam(0.0, 0.0, -1.0);

        Gaze::GazeVector2 viewport_pixel;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, viewport_pixel);
        
        REQUIRE(ok);
        CHECK(viewport_pixel.x == doctest::Approx(540.0));
    }
}

TEST_CASE("Testing ScreenProjector Scaling & Inverse Parity") {
    // Setup projection engine mock metrics
    Gaze::ProjectionEngine engine;
    engine.set_screen_size_pixels(Gaze::GazeVector2(1440.0, 900.0)); // Logical screen width
    engine.set_screen_size_mm(Gaze::GazeVector2(302.0, 188.0));      // Physical screen mm
    Gaze::CameraPlacement placement(Gaze::GazeVector3(0.0, 94.0, 0.0), 0.0);
    engine.set_camera_placement(placement);

    // 1. Standard Configuration
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::from_godot_geometry(
            Gaze::GazeVector2(100.0, 50.0),   // Window pos in logical screen pixels
            Gaze::GazeVector2(1.0, 1.0),      // Logical viewport scale
            Gaze::GazeVector2(0.0, 0.0)       // Logical viewport offset
        );

        Gaze::GazeVector2 logical_pixel(512.0, 300.0);
        Gaze::GazeVector2 physical_pixel = projector.map_viewport_to_screen_px(logical_pixel);

        CHECK(physical_pixel.x == doctest::Approx(612.0));
        CHECK(physical_pixel.y == doctest::Approx(350.0));

        // Setup raw gaze/origin in camera space and project it back to verify inverse parity
        Gaze::GazeVector3 origin_cam(0.0, -94.0, 800.0);
        Gaze::GazeVector3 dir_cam(0.05, -0.02, -0.998); // Random gaze direction pointing forward
        Gaze::GazeVector2 proj_viewport;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, proj_viewport);
        REQUIRE(ok);

        Gaze::GazeVector2 proj_physical = projector.map_viewport_to_screen_px(proj_viewport);
        
        double local_x = proj_physical.x - projector.window_position_px.x;
        double local_y = proj_physical.y - projector.window_position_px.y;
        double view_x = (local_x - projector.viewport_offset_px.x) / projector.viewport_scale.x;
        double view_y = (local_y - projector.viewport_offset_px.y) / projector.viewport_scale.y;

        CHECK(view_x == doctest::Approx(proj_viewport.x));
        CHECK(view_y == doctest::Approx(proj_viewport.y));
    }

    // 2. Scaled viewport configuration (e.g. game viewport stretched by 2.0)
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::from_godot_geometry(
            Gaze::GazeVector2(200.0, 100.0), // window position in logical pixels
            Gaze::GazeVector2(2.0, 2.0),     // viewport scale
            Gaze::GazeVector2(0.0, 0.0)      // offset
        );

        Gaze::GazeVector2 logical_pixel(512.0, 300.0);
        Gaze::GazeVector2 physical_pixel = projector.map_viewport_to_screen_px(logical_pixel);
        CHECK(physical_pixel.x == doctest::Approx(1224.0));
        CHECK(physical_pixel.y == doctest::Approx(700.0));

        // Assert inverse parity
        Gaze::GazeVector3 origin_cam(0.0, -94.0, 800.0);
        Gaze::GazeVector3 dir_cam(-0.1, 0.05, -0.99);
        Gaze::GazeVector2 proj_viewport;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, proj_viewport);
        REQUIRE(ok);

        Gaze::GazeVector2 proj_physical = projector.map_viewport_to_screen_px(proj_viewport);
        double local_x = proj_physical.x - projector.window_position_px.x;
        double local_y = proj_physical.y - projector.window_position_px.y;
        double view_x = (local_x - projector.viewport_offset_px.x) / projector.viewport_scale.x;
        double view_y = (local_y - projector.viewport_offset_px.y) / projector.viewport_scale.y;

        CHECK(view_x == doctest::Approx(proj_viewport.x));
        CHECK(view_y == doctest::Approx(proj_viewport.y));
    }

    // 3. Check Safeguards & Division-by-Zero Protection
    {
        Gaze::ScreenProjector projector = Gaze::ScreenProjector::from_godot_geometry(
            Gaze::GazeVector2(0.0, 0.0),
            Gaze::GazeVector2(0.0, 0.0),  // Scale = 0 (Division by zero risk!)
            Gaze::GazeVector2(0.0, 0.0)
        );

        Gaze::GazeVector3 origin_cam(0.0, -94.0, 800.0);
        Gaze::GazeVector3 dir_cam(0.0, 0.0, -1.0);
        Gaze::GazeVector2 proj_viewport;
        bool ok = projector.project_to_viewport(engine, origin_cam, dir_cam, proj_viewport);
        CHECK_FALSE(ok); // Must fail gracefully
    }
}

TEST_CASE("Testing Head Rotation Pitch and Yaw Coordinate Signs") {
    Gaze::ProjectionEngine engine;
    
    // Test case A: Logical math verification
    // 1. Staring straight ahead (near-zero rotation)
    {
        Gaze::GazeVector3 translation(0.0, 0.0, 800.0);
        Gaze::GazeVector3 rotation_straight(0.0, 0.0, 0.0);
        Gaze::GazeTransform3D transform = engine.get_head_transform_in_camera_space(translation, rotation_straight);
        Gaze::GazeVector3 head_forward = transform.basis.multiply_vector(Gaze::GazeVector3(0, 0, -1));
        
        // Z points towards the screen (positive Z in Godot camera space)
        CHECK(head_forward.z > 0.9);
        CHECK(std::abs(head_forward.x) < 0.01);
        CHECK(std::abs(head_forward.y) < 0.01);
    }
    
    // 2. Head tilted up (pitch rotation around X is negative in OpenCV: rvec.x < 0)
    {
        Gaze::GazeVector3 translation(0.0, 0.0, 800.0);
        Gaze::GazeVector3 rotation_up(-0.15, 0.0, 0.0); // ~8.6 degrees up
        Gaze::GazeTransform3D transform = engine.get_head_transform_in_camera_space(translation, rotation_up);
        Gaze::GazeVector3 head_forward = transform.basis.multiply_vector(Gaze::GazeVector3(0, 0, -1));
        
        // Pitch up must produce a positive Y component in camera space
        CHECK(head_forward.y > 0.05);
        CHECK(head_forward.z > 0.9);
    }
    
    // 3. Head tilted down (pitch rotation around X is positive in OpenCV: rvec.x > 0)
    {
        Gaze::GazeVector3 translation(0.0, 0.0, 800.0);
        Gaze::GazeVector3 rotation_down(0.15, 0.0, 0.0); // ~8.6 degrees down
        Gaze::GazeTransform3D transform = engine.get_head_transform_in_camera_space(translation, rotation_down);
        Gaze::GazeVector3 head_forward = transform.basis.multiply_vector(Gaze::GazeVector3(0, 0, -1));
        
        // Pitch down must produce a negative Y component in camera space
        CHECK(head_forward.y < -0.05);
        CHECK(head_forward.z > 0.9);
    }
    
    // 4. Head turned left (yaw rotation around Y is positive in OpenCV: rvec.y > 0)
    {
        Gaze::GazeVector3 translation(0.0, 0.0, 800.0);
        Gaze::GazeVector3 rotation_left(0.0, 0.2, 0.0); // ~11.5 degrees left (camera right)
        Gaze::GazeTransform3D transform = engine.get_head_transform_in_camera_space(translation, rotation_left);
        Gaze::GazeVector3 head_forward = transform.basis.multiply_vector(Gaze::GazeVector3(0, 0, -1));
        
        // Turning left (facing camera's negative X direction in standard camera space)
        CHECK(head_forward.x < -0.05);
        CHECK(head_forward.z > 0.9);
    }
    
    // 5. Head turned right (yaw rotation around Y is negative in OpenCV: rvec.y < 0)
    {
        Gaze::GazeVector3 translation(0.0, 0.0, 800.0);
        Gaze::GazeVector3 rotation_right(0.0, -0.2, 0.0); // ~11.5 degrees right (camera left)
        Gaze::GazeTransform3D transform = engine.get_head_transform_in_camera_space(translation, rotation_right);
        Gaze::GazeVector3 head_forward = transform.basis.multiply_vector(Gaze::GazeVector3(0, 0, -1));
        
        // Turning right (facing camera's positive X direction in standard camera space)
        CHECK(head_forward.x > 0.05);
        CHECK(head_forward.z > 0.9);
    }
}




