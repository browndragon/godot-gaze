#include "doctest.h"
#include "yunet_pipeline.hpp"
#include "opencv_gaze_model.hpp"
#include "projection_engine.hpp"
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

    GazeVector3 raw_gaze_dir;
    bool model_success = model.estimate_raw_gaze(crops, raw_gaze_dir);
    REQUIRE(model_success == true);

    // Check that raw_gaze_dir is normalized and valid
    CHECK(raw_gaze_dir.length() == doctest::Approx(1.0));
    CHECK(std::abs(raw_gaze_dir.z) > 0.0);
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
    CameraPlacement placement(GazeVector3(0.0, 135.0, 10.0), 15.0);
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

            // Head forward vector in standard Camera Space (standard forward is -Z)
            GazeVector3 head_forward_cam = head_transform.basis.multiply_vector(GazeVector3(0, 0, -1));

            // Gaze origin (nose/head center) in standard Camera Space
            GazeVector3 head_center_cam = head_transform.origin;

            sd.nose_projected = project_ray_to_screen(head_center_cam, head_forward_cam);

            GazeVector3 raw_gaze_dir;
            if (model.estimate_raw_gaze(crops, raw_gaze_dir)) {
                // Map raw gaze direction to Camera Space (X=-X, Y=Y, Z=-Z)
                sd.gaze_dir = GazeVector3(-raw_gaze_dir.x, raw_gaze_dir.y, -raw_gaze_dir.z);

                GazeVector3 eye_center_cv = (sd.left_eye + sd.right_eye) * 0.5;
                GazeVector3 eye_center_cam(eye_center_cv.x, -eye_center_cv.y, -eye_center_cv.z);

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
    }

    if (top && down) {
        CHECK(down->rotation.x > top->rotation.x);
        CHECK(top->gaze_dir.y > down->gaze_dir.y); // +Y points up, so top is greater
    }

    // Eye Spatial Configuration
    for (const auto& sd : samples) {
        if (sd.detected) {
            CHECK(sd.left_eye.x > sd.right_eye.x);
        }
    }

    // Assert that errors are within a reasonable uncalibrated baseline (e.g. within 15 cm for nose, 40 cm for gaze)
    for (const auto& sd : samples) {
        if (sd.detected) {
            bool check_x = (sd.filename == "self_center.jpg" || sd.filename.find("left") != std::string::npos || sd.filename.find("right") != std::string::npos);
            bool check_y = (sd.filename == "self_center.jpg" || sd.filename.find("top") != std::string::npos || sd.filename.find("down") != std::string::npos);

            if (check_x) {
                CHECK_MESSAGE(sd.nose_error_x < 150.0, "Nose X error should be < 15cm in " << sd.filename << " (actual: " << sd.nose_error_x << " mm)");
                CHECK_MESSAGE(sd.gaze_error_x < 400.0, "Gaze X error should be < 40cm in " << sd.filename << " (actual: " << sd.gaze_error_x << " mm)");
            }
            if (check_y) {
                CHECK_MESSAGE(sd.nose_error_y < 150.0, "Nose Y error should be < 15cm in " << sd.filename << " (actual: " << sd.nose_error_y << " mm)");
                CHECK_MESSAGE(sd.gaze_error_y < 400.0, "Gaze Y error should be < 40cm in " << sd.filename << " (actual: " << sd.gaze_error_y << " mm)");
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
