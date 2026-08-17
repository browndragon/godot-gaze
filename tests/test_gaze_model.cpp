#include "doctest.h"
#include "ort_gaze_model.hpp"
#include <fstream>
#include <iostream>
#include <cmath>

inline bool gaze_file_exists(const std::string &filename) {
    std::ifstream f(filename.c_str());
    return f.good();
}

TEST_CASE("ORT Gaze Model Boundary Invariants and Directional Sensitivity")
{
    std::string model_path = "project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";
    if (!gaze_file_exists(model_path)) {
        model_path = "../project/addons/godot-gaze/models/gaze-estimation-adas-0002.ort";
    }

    if (!gaze_file_exists(model_path)) {
        MESSAGE("Skipping ORT Gaze Model test: Model file not found");
        return;
    }

    Gaze::ORTGazeModel gaze_model(model_path);
    REQUIRE(gaze_model.initialize() == true);

    Gaze::EyeCrops crops;
    crops.face_detected = true;
    std::memset(crops.left_eye_data, 128, sizeof(crops.left_eye_data));
    std::memset(crops.right_eye_data, 128, sizeof(crops.right_eye_data));

    // Test Case 1: Straight Forward Head Pose (0, 0, -650mm)
    crops.head_pose_translation = Gaze::GazeVector3(0.0, 0.0, -650.0);
    crops.head_pose_rotation = Gaze::GazeVector3(0.0, 0.0, 0.0);
    crops.left_eye_center_cam = Gaze::GazeVector3(-30.0, 20.0, -650.0);
    crops.right_eye_center_cam = Gaze::GazeVector3(33.0, 20.0, -650.0);

    Gaze::GazeVector3 gaze_dir_forward;
    bool ok = gaze_model.estimate_raw_gaze(crops, gaze_dir_forward);
    REQUIRE(ok == true);

    // Unit vector invariant
    double norm_fwd = std::sqrt(gaze_dir_forward.x * gaze_dir_forward.x + gaze_dir_forward.y * gaze_dir_forward.y + gaze_dir_forward.z * gaze_dir_forward.z);
    CHECK(norm_fwd == doctest::Approx(1.0).epsilon(1e-4));

    // Test Case 2: Head Pitch UP +15 deg (looking higher up)
    crops.head_pose_rotation = Gaze::GazeVector3(0.2618, 0.0, 0.0); // +15 deg pitch
    Gaze::GazeVector3 gaze_dir_pitch_up;
    ok = gaze_model.estimate_raw_gaze(crops, gaze_dir_pitch_up);
    REQUIRE(ok == true);

    // Pitching head UP must increase +Y_cam component in Godot Camera Space (+Y up)
    CHECK(gaze_dir_pitch_up.y > gaze_dir_forward.y);

    // Test Case 3: Head Yaw RIGHT +15 deg (looking to anatomical right)
    crops.head_pose_rotation = Gaze::GazeVector3(0.0, 0.2618, 0.0); // +15 deg yaw
    Gaze::GazeVector3 gaze_dir_yaw_right;
    ok = gaze_model.estimate_raw_gaze(crops, gaze_dir_yaw_right);
    REQUIRE(ok == true);

    // Unit vector invariant
    double norm_yaw = std::sqrt(gaze_dir_yaw_right.x * gaze_dir_yaw_right.x + gaze_dir_yaw_right.y * gaze_dir_yaw_right.y + gaze_dir_yaw_right.z * gaze_dir_yaw_right.z);
    CHECK(norm_yaw == doctest::Approx(1.0).epsilon(1e-4));
}
