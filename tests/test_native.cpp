#include "doctest.h"
#include "yunet_pipeline.hpp"
#include "opencv_gaze_model.hpp"
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <cstring>
#include <iostream>
#include <vector>

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

    auto project_ray_to_screen = [](const GazeVector3& origin_cam, const GazeVector3& dir_cam) -> GazeVector2 {
        double camera_offset_y = -135.0; // mm (camera Y offset above screen center)
        
        // Map origin and dir to the projection engine's mirrored camera space:
        GazeVector3 origin_proj(-origin_cam.x, origin_cam.y, origin_cam.z);
        GazeVector3 dir_proj(-dir_cam.x, dir_cam.y, dir_cam.z);

        double denom = dir_proj.z;
        if (std::abs(denom) < 1e-6) {
            return GazeVector2(0.0, 0.0);
        }
        
        double t = -origin_proj.z / denom;
        GazeVector3 P_int = origin_proj + dir_proj * t;
        
        double x_s = P_int.x;
        double y_s = P_int.y + camera_offset_y;
        
        return GazeVector2(x_s, y_s);
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
        double nose_error_mm = 0.0;
        double gaze_error_mm = 0.0;
    };

    std::vector<SampleData> samples;

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
            sd.translation = crops.head_pose_translation;
            sd.rotation = crops.head_pose_rotation;
            sd.left_eye = crops.left_eye_center_cam;
            sd.right_eye = crops.right_eye_center_cam;

            // Reconstruct head rotation matrix R_cv to get head forward vector
            double pitch_rad = sd.rotation.x * (3.141592653589793 / 180.0);
            double yaw_rad = sd.rotation.y * (3.141592653589793 / 180.0);
            double roll_rad = sd.rotation.z * (3.141592653589793 / 180.0);

            double cp = std::cos(pitch_rad), sp = std::sin(pitch_rad);
            double cy = std::cos(yaw_rad), sy = std::sin(yaw_rad);
            double cr = std::cos(roll_rad), sr = std::sin(roll_rad);

            double r02 = cr * sy * cp + sr * sp;
            double r12 = sr * sy * cp - cr * sp;
            double r22 = cy * cp;

            // Head forward vector in camera space: -Col 2 of R_cv
            GazeVector3 head_forward_cam(-r02, -r12, -r22);

            // Gaze origin (nose/head center)
            GazeVector3 head_center_cam = sd.translation;

            sd.nose_projected = project_ray_to_screen(head_center_cam, head_forward_cam);

            GazeVector3 raw_gaze_dir;
            if (model.estimate_raw_gaze(crops, raw_gaze_dir)) {
                sd.gaze_dir = raw_gaze_dir;
                // Convert raw gaze direction (model space) to camera space:
                GazeVector3 eye_gaze_cam(-raw_gaze_dir.x, -raw_gaze_dir.y, raw_gaze_dir.z);
                GazeVector3 eye_center_cam = (sd.left_eye + sd.right_eye) * 0.5;
                sd.gaze_projected = project_ray_to_screen(eye_center_cam, eye_gaze_cam);
            }

            // Calculate Euclidean errors
            double dx_nose = sd.nose_projected.x - tg.nose_target.x;
            double dy_nose = sd.nose_projected.y - tg.nose_target.y;
            sd.nose_error_mm = std::sqrt(dx_nose*dx_nose + dy_nose*dy_nose);

            double dx_gaze = sd.gaze_projected.x - tg.gaze_target.x;
            double dy_gaze = sd.gaze_projected.y - tg.gaze_target.y;
            sd.gaze_error_mm = std::sqrt(dx_gaze*dx_gaze + dy_gaze*dy_gaze);

            std::cout << "Image: " << tg.filename
                      << "\n  Target Nose: (" << tg.nose_target.x << ", " << tg.nose_target.y << ") mm"
                      << "\n  Projected Nose: (" << sd.nose_projected.x << ", " << sd.nose_projected.y << ") mm"
                      << "\n  Nose Error: " << sd.nose_error_mm << " mm (" << sd.nose_error_mm / 10.0 << " cm)"
                      << "\n  Target Gaze: (" << tg.gaze_target.x << ", " << tg.gaze_target.y << ") mm"
                      << "\n  Projected Gaze: (" << sd.gaze_projected.x << ", " << sd.gaze_projected.y << ") mm"
                      << "\n  Gaze Error: " << sd.gaze_error_mm << " mm (" << sd.gaze_error_mm / 10.0 << " cm)"
                      << std::endl;
        } else {
            std::cout << "Image: " << tg.filename << " - NO FACE DETECTED" << std::endl;
        }

        samples.push_back(sd);
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
        CHECK(right->gaze_dir.x > left->gaze_dir.x);
    }

    if (top && down) {
        CHECK(down->rotation.x < top->rotation.x);
        CHECK(down->gaze_dir.y < top->gaze_dir.y);
    }

    // Eye Spatial Configuration
    for (const auto& sd : samples) {
        if (sd.detected) {
            CHECK(sd.left_eye.x > sd.right_eye.x);
        }
    }

    // Assert that errors are within a reasonable uncalibrated baseline (e.g. within 50 cm for nose, 50 cm for gaze)
    for (const auto& sd : samples) {
        if (sd.detected) {
            CHECK_MESSAGE(sd.nose_error_mm < 500.0, "Nose error should be < 50cm in " << sd.filename << " (actual: " << sd.nose_error_mm << " mm)");
            CHECK_MESSAGE(sd.gaze_error_mm < 500.0, "Gaze error should be < 50cm in " << sd.filename << " (actual: " << sd.gaze_error_mm << " mm)");
        }
    }
}
