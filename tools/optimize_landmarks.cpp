#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/calib3d.hpp>
#include "src/core/math_defs.hpp"
#include "src/core/projection_engine.hpp"

using namespace std;
using namespace Gaze;

struct TargetGaze {
    string filename;
    GazeVector2 nose_target; // in mm relative to screen center
    GazeVector2 gaze_target;
};

int main() {
    string model_path = "project/models/face_detection_yunet_2023mar.onnx";
    cv::Ptr<cv::FaceDetectorYN> detector = cv::FaceDetectorYN::create(
        model_path, "", cv::Size(320, 320), 0.6f, 0.3f, 5000
    );

    if (detector.empty()) {
        cerr << "Failed to load detector model!" << endl;
        return 1;
    }

    vector<TargetGaze> targets = {
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

    struct ImageSample {
        string filename;
        cv::Point2f landmarks[5];
        GazeVector2 nose_target;
    };

    vector<ImageSample> samples;

    for (const auto& tg : targets) {
        string filepath = "tests/resources/" + tg.filename;
        cv::Mat img = cv::imread(filepath);
        if (img.empty()) {
            cerr << "Failed to load image: " << filepath << endl;
            continue;
        }

        detector->setInputSize(img.size());
        cv::Mat faces;
        detector->detect(img, faces);

        if (faces.empty() || faces.rows == 0) {
            cerr << "No face detected in: " << tg.filename << endl;
            continue;
        }

        ImageSample s;
        s.filename = tg.filename;
        s.nose_target = tg.nose_target;
        for (int i = 0; i < 5; ++i) {
            s.landmarks[i].x = faces.at<float>(0, 4 + 2 * i);
            s.landmarks[i].y = faces.at<float>(0, 5 + 2 * i);
        }
        samples.push_back(s);
    }

    // Physical setup: camera is at the top, so y_offset = -135.0 mm (since top is negative Y)
    CameraPlacement placement(GazeVector3(0.0, -135.0, 10.0), 15.0);

    auto project_ray_to_screen_corrected = [&placement](const GazeVector3& origin_cam, const GazeVector3& dir_cam) -> GazeVector2 {
        double theta_rad = placement.tilt_degrees * (3.14159265358979323846 / 180.0);
        double cos_t = std::cos(theta_rad);
        double sin_t = std::sin(theta_rad);

        // 1. Transform origin to Screen Space in mm
        double O_s_x = -origin_cam.x + placement.offset.x;
        double O_s_y = -origin_cam.y * cos_t - origin_cam.z * sin_t + placement.offset.y;
        double O_s_z = origin_cam.y * sin_t - origin_cam.z * cos_t + placement.offset.z;

        // 2. Transform direction to Screen Space in mm
        double v_s_x = -dir_cam.x;
        double v_s_y = -dir_cam.y * cos_t - dir_cam.z * sin_t;
        double v_s_z = dir_cam.y * sin_t - dir_cam.z * cos_t;

        if (std::abs(v_s_z) < 1e-6) return GazeVector2(0,0);

        double t = -O_s_z / v_s_z;
        if (t < 0.0) return GazeVector2(0,0);

        double x_s = O_s_x + t * v_s_x;
        double y_s = O_s_y + t * v_s_y;

        return GazeVector2(x_s, y_s);
    };

    auto get_head_transform_in_camera_space = [](const GazeVector3& opencv_translation, const GazeVector3& opencv_rotation_deg) -> GazeTransform3D {
        GazeBasis3D r_x_180(
            GazeVector3(1, 0, 0),
            GazeVector3(0, -1, 0),
            GazeVector3(0, 0, -1)
        );
        GazeTransform3D T_cv_cam_to_ggaze_cam(r_x_180, GazeVector3(0, 0, 0));

        GazeBasis3D R_cv = GazeBasis3D::from_euler_zyx(opencv_rotation_deg.x, opencv_rotation_deg.y, opencv_rotation_deg.z);
        GazeTransform3D T_cv_face_to_cv_cam(R_cv, opencv_translation);

        GazeBasis3D r_z_180(
            GazeVector3(-1, 0, 0),
            GazeVector3(0, -1, 0),
            GazeVector3(0, 0, 1)
        );
        GazeTransform3D T_ggaze_face_to_cv_face(r_z_180, GazeVector3(0, 0, 0));

        return T_cv_cam_to_ggaze_cam * T_cv_face_to_cv_cam * T_ggaze_face_to_cv_face;
    };

    double best_err = 1e9;
    double best_nose_z = 0;
    double best_nose_y = 0;
    double best_mouth_z = 0;
    double best_mouth_y = 0;

    // Grid search over potential values
    // Search nose_z from 10.0 to 80.0, nose_y from -15.0 to 15.0
    // search mouth_z from -20.0 to 20.0, mouth_y from 20.0 to 40.0
    for (double nose_z = 10.0; nose_z <= 80.0; nose_z += 2.0) {
        for (double nose_y = -15.0; nose_y <= 15.0; nose_y += 1.0) {
            for (double mouth_z = -20.0; mouth_z <= 20.0; mouth_z += 2.0) {
                for (double mouth_y = 20.0; mouth_y <= 40.0; mouth_y += 2.0) {
                    
                    std::vector<cv::Point3f> model_points = {
                        cv::Point3f(-30.0f, -28.676f, 0.0f), // Right eye
                        cv::Point3f(30.0f, -28.676f, 0.0f),  // Left eye
                        cv::Point3f(0.0f, (float)nose_y, (float)nose_z),   // Nose tip
                        cv::Point3f(-18.462f, (float)mouth_y, (float)mouth_z), // Right mouth corner
                        cv::Point3f(18.462f, (float)mouth_y, (float)mouth_z)  // Left mouth corner
                    };

                    double total_err = 0.0;
                    bool valid = true;

                    for (const auto& s : samples) {
                        std::vector<cv::Point2f> image_points = {
                            s.landmarks[0],
                            s.landmarks[1],
                            s.landmarks[2],
                            s.landmarks[3],
                            s.landmarks[4]
                        };

                        double cx = 1440.0 / 2.0;
                        double cy = 960.0 / 2.0;
                        double fx = 1440.0 * 1.5625;
                        cv::Mat camera_matrix = (cv::Mat_<double>(3, 3) << 
                            fx,  0.0, cx,
                            0.0, fx,  cy,
                            0.0, 0.0, 1.0
                        );
                        cv::Mat dist_coeffs = cv::Mat::zeros(4, 1, CV_64F);

                        cv::Mat rvec, tvec;
                        bool pnp_success = cv::solvePnP(model_points, image_points, camera_matrix, dist_coeffs, rvec, tvec, false, cv::SOLVEPNP_SQPNP);

                        if (!pnp_success) {
                            valid = false;
                            break;
                        }

                        cv::Mat R;
                        cv::Rodrigues(rvec, R);

                        double sy = std::sqrt(R.at<double>(0, 0) * R.at<double>(0, 0) + R.at<double>(1, 0) * R.at<double>(1, 0));
                        double pitch_deg = 0.0, yaw_deg = 0.0, roll_deg = 0.0;
                        if (sy > 1e-6) {
                            pitch_deg = std::atan2(R.at<double>(2, 1), R.at<double>(2, 2)) * (180.0 / 3.141592653589793);
                            yaw_deg   = std::atan2(-R.at<double>(2, 0), sy) * (180.0 / 3.141592653589793);
                            roll_deg  = std::atan2(R.at<double>(1, 0), R.at<double>(0, 0)) * (180.0 / 3.141592653589793);
                        }
                        GazeTransform3D head_transform = get_head_transform_in_camera_space(
                            GazeVector3(tvec.at<double>(0), tvec.at<double>(1), tvec.at<double>(2)),
                            GazeVector3(pitch_deg, yaw_deg, roll_deg)
                        );

                        GazeVector3 head_center_cam = head_transform.origin;
                        GazeVector3 head_forward_cam = head_transform.basis.multiply_vector(GazeVector3(0, 0, -1));

                        GazeVector2 nose_projected = project_ray_to_screen_corrected(head_center_cam, head_forward_cam);
                        double dx = nose_projected.x - s.nose_target.x;
                        double dy = nose_projected.y - s.nose_target.y;
                        total_err += std::sqrt(dx*dx + dy*dy);
                    }

                    if (valid) {
                        double err = total_err / samples.size();
                        if (err < best_err) {
                            best_err = err;
                            best_nose_z = nose_z;
                            best_nose_y = nose_y;
                            best_mouth_z = mouth_z;
                            best_mouth_y = mouth_y;
                        }
                    }
                }
            }
        }
    }

    cout << "Best nose_z: " << best_nose_z << endl;
    cout << "Best nose_y: " << best_nose_y << endl;
    cout << "Best mouth_z: " << best_mouth_z << endl;
    cout << "Best mouth_y: " << best_mouth_y << endl;
    cout << "Best average error: " << best_err << " mm" << endl;

    return 0;
}
