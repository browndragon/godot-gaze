#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <map>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include "../src/core/math_defs.hpp"
#include "../src/core/projection_engine.hpp"

using namespace Gaze;

struct TargetGaze {
    std::string filename;
    GazeVector2 nose_target;
    GazeVector2 gaze_target;
};

// Target points on MacBook Pro 14" (305mm x 191mm) screen
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

struct LoadedSample {
    std::string filename;
    cv::Mat left_eye_crop;
    cv::Mat right_eye_crop;
    GazeVector3 head_pose_rotation; // SolvePnP outputs (Pitch, Yaw, Roll)
    GazeVector3 eye_center_cam_gg;
    GazeVector2 gaze_tgt;
    GazeVector2 nose_tgt;
};

// 3D face model points
std::vector<cv::Point3f> model_points = {
    cv::Point3f(-30.0f, -28.676f, 0.0f),
    cv::Point3f(30.0f, -28.676f, 0.0f),
    cv::Point3f(0.0f, -5.000f, -59.859f),
    cv::Point3f(-18.462f, 31.712f, -4.550f),
    cv::Point3f(18.462f, 31.712f, -4.550f)
};

bool crop_eye_cpp(const cv::Mat& gray, const cv::Point2f landmarks[5], bool is_left, cv::Mat& out_crop) {
    cv::Point2f eye_center = is_left ? landmarks[1] : landmarks[0];
    double roll_dx = landmarks[1].x - landmarks[0].x;
    double roll_dy = landmarks[1].y - landmarks[0].y;
    double angle = std::atan2(roll_dy, roll_dx) * (180.0 / 3.141592653589793);
    
    cv::Size target_size(60, 60);
    cv::Mat M = cv::getRotationMatrix2D(eye_center, angle, 1.0);
    M.at<double>(0, 2) += (target_size.width / 2.0) - eye_center.x;
    M.at<double>(1, 2) += (target_size.height / 2.0) - eye_center.y;
    
    cv::warpAffine(gray, out_crop, M, target_size, cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    cv::cvtColor(out_crop, out_crop, cv::COLOR_GRAY2BGR);
    return true;
}

int main() {
    cv::Ptr<cv::FaceDetectorYN> detector = cv::FaceDetectorYN::create(
        "project/models/face_detection_yunet_2023mar.onnx",
        "",
        cv::Size(320, 320),
        0.6f,
        0.3f,
        5
    );

    cv::dnn::Net gaze_net = cv::dnn::readNet("project/models/gaze-estimation-adas-0002.xml", "project/models/gaze-estimation-adas-0002.bin");
    if (gaze_net.empty()) {
        std::cerr << "Failed to load gaze model!" << std::endl;
        return 1;
    }

    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(GazeVector2(305.0, 191.0));
    CameraPlacement placement(GazeVector3(0.0, 95.5, 0.0), 0.0);
    engine.set_camera_placement(placement);

    std::vector<LoadedSample> samples;

    for (const auto& tg : targets) {
        std::string filepath = "tests/resources/" + tg.filename;
        cv::Mat img = cv::imread(filepath);
        if (img.empty()) {
            std::cerr << "Failed to load image: " << filepath << std::endl;
            continue;
        }

        detector->setInputSize(img.size());
        cv::Mat faces;
        detector->detect(img, faces);

        if (faces.empty()) {
            std::cerr << "No face detected in: " << tg.filename << std::endl;
            continue;
        }

        cv::Point2f landmarks[5];
        for (int i = 0; i < 5; ++i) {
            landmarks[i].x = faces.at<float>(0, 4 + 2 * i);
            landmarks[i].y = faces.at<float>(0, 5 + 2 * i);
        }

        double cx = img.cols / 2.0;
        double cy = img.rows / 2.0;
        double fx = img.cols * 1.5625;
        cv::Mat camera_matrix = (cv::Mat_<double>(3, 3) << 
            fx,  0.0, cx,
            0.0, fx,  cy,
            0.0, 0.0, 1.0
        );
        cv::Mat dist_coeffs = cv::Mat::zeros(4, 1, CV_64F);

        cv::Mat rvec, tvec;
        bool pnp_success = cv::solvePnP(model_points, std::vector<cv::Point2f>(landmarks, landmarks + 5), camera_matrix, dist_coeffs, rvec, tvec, false, cv::SOLVEPNP_SQPNP);

        if (!pnp_success) {
            std::cerr << "SolvePnP failed for: " << tg.filename << std::endl;
            continue;
        }

        cv::Mat R;
        cv::Rodrigues(rvec, R);

        double sy = std::sqrt(R.at<double>(0, 0) * R.at<double>(0, 0) + R.at<double>(1, 0) * R.at<double>(1, 0));
        bool singular = sy < 1e-6;
        double pitch_deg = 0.0, yaw_deg = 0.0, roll_deg = 0.0;
        if (!singular) {
            pitch_deg = std::atan2(R.at<double>(2, 1), R.at<double>(2, 2)) * (180.0 / 3.141592653589793);
            yaw_deg   = std::atan2(-R.at<double>(2, 0), sy) * (180.0 / 3.141592653589793);
            roll_deg  = std::atan2(R.at<double>(1, 0), R.at<double>(0, 0)) * (180.0 / 3.141592653589793);
        }

        cv::Mat gray;
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

        LoadedSample s;
        s.filename = tg.filename;
        crop_eye_cpp(gray, landmarks, true, s.left_eye_crop);
        crop_eye_cpp(gray, landmarks, false, s.right_eye_crop);
        s.head_pose_rotation = GazeVector3(pitch_deg, yaw_deg, roll_deg);
        s.gaze_tgt = tg.gaze_target;
        s.nose_tgt = tg.nose_target;

        // Eye centers in camera space (OpenCV)
        cv::Mat left_eye_cam_mat = R * (cv::Mat_<double>(3, 1) << 30.0, -28.676, 0.0) + tvec;
        cv::Mat right_eye_cam_mat = R * (cv::Mat_<double>(3, 1) << -30.0, -28.676, 0.0) + tvec;
        GazeVector3 left_eye_cam(left_eye_cam_mat.at<double>(0), left_eye_cam_mat.at<double>(1), left_eye_cam_mat.at<double>(2));
        GazeVector3 right_eye_cam(right_eye_cam_mat.at<double>(0), right_eye_cam_mat.at<double>(1), right_eye_cam_mat.at<double>(2));
        GazeVector3 eye_center_cv = (left_eye_cam + right_eye_cam) * 0.5;

        // Convert to GodotGaze standard Camera Space (Y=-Y, Z=-Z)
        s.eye_center_cam_gg = GazeVector3(eye_center_cv.x, -eye_center_cv.y, -eye_center_cv.z);

        samples.push_back(s);
    }

    std::cout << "Successfully loaded " << samples.size() << " samples." << std::endl;

    // Grid search variables
    double best_avg_err = 1e9;
    std::string best_eye_swap = "";
    GazeVector3 best_head_pose_mults(0, 0, 0);
    GazeVector3 best_gaze_mults(0, 0, 0);

    // Multipliers
    std::vector<double> mults = {1.0, -1.0};
    std::vector<std::string> eye_swaps = {"anatomical", "swapped"};

    for (const auto& es : eye_swaps) {
        for (double sy : mults) {
            for (double sp : mults) {
                for (double sr : mults) {
                    for (double mx : mults) {
                        for (double my : mults) {
                            for (double mz : mults) {
                                double total_err = 0.0;
                                bool valid = true;

                                for (const auto& s : samples) {
                                    cv::Mat l_crop = (es == "anatomical") ? s.left_eye_crop : s.right_eye_crop;
                                    cv::Mat r_crop = (es == "anatomical") ? s.right_eye_crop : s.left_eye_crop;

                                    cv::Mat left_blob = cv::dnn::blobFromImage(l_crop, 1.0, cv::Size(60, 60), cv::Scalar(0), false);
                                    cv::Mat right_blob = cv::dnn::blobFromImage(r_crop, 1.0, cv::Size(60, 60), cv::Scalar(0), false);

                                    // Head pose in model coordinates (Yaw, Pitch, Roll)
                                    float head_pose_data[3] = {
                                        static_cast<float>(sy * s.head_pose_rotation.y),
                                        static_cast<float>(sp * s.head_pose_rotation.x),
                                        static_cast<float>(sr * s.head_pose_rotation.z)
                                    };
                                    cv::Mat head_pose_blob(1, 3, CV_32F, head_pose_data);

                                    gaze_net.setInput(left_blob, "left_eye_image");
                                    gaze_net.setInput(right_blob, "right_eye_image");
                                    gaze_net.setInput(head_pose_blob, "head_pose_angles");

                                    cv::Mat output = gaze_net.forward("gaze_vector");
                                    if (output.empty() || output.cols != 3) {
                                        valid = false;
                                        break;
                                    }

                                    double rx = output.at<float>(0, 0);
                                    double ry = output.at<float>(0, 1);
                                    double rz = output.at<float>(0, 2);
                                    double len = std::sqrt(rx*rx + ry*ry + rz*rz);
                                    rx /= len;
                                    ry /= len;
                                    rz /= len;

                                    // Gaze direction mapped to GodotGaze Camera Space
                                    GazeVector3 gaze_dir_gg(mx * rx, my * ry, mz * rz);

                                    GazeVector2 projected;
                                    if (engine.project_gaze(s.eye_center_cam_gg, gaze_dir_gg, projected)) {
                                        // Convert pixel projection to mm relative to screen center
                                        double x_s = (projected.x - 960.0) * (305.0 / 1920.0);
                                        double y_s = (projected.y - 540.0) * (191.0 / 1080.0);
                                        double dx = x_s - s.gaze_tgt.x;
                                        double dy = y_s - s.gaze_tgt.y;
                                        total_err += std::sqrt(dx*dx + dy*dy);
                                    } else {
                                        valid = false;
                                        break;
                                    }
                                }

                                if (valid) {
                                    double avg_err = total_err / samples.size();
                                    if (avg_err < best_avg_err) {
                                        best_avg_err = avg_err;
                                        best_eye_swap = es;
                                        best_head_pose_mults = GazeVector3(sp, sy, sr);
                                        best_gaze_mults = GazeVector3(mx, my, mz);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    std::cout << "\n=== Optimization Results ===" << std::endl;
    std::cout << "Best Average Error: " << best_avg_err << " mm (" << best_avg_err / 10.0 << " cm)" << std::endl;
    std::cout << "Best Eye Swap Strategy: " << best_eye_swap << std::endl;
    std::cout << "Best Head Pose Multipliers: Pitch=" << best_head_pose_mults.x << ", Yaw=" << best_head_pose_mults.y << ", Roll=" << best_head_pose_mults.z << std::endl;
    std::cout << "Best Output Gaze Mapping: X=" << best_gaze_mults.x << ", Y=" << best_gaze_mults.y << ", Z=" << best_gaze_mults.z << std::endl;

    return 0;
}
