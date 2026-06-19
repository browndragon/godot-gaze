#include <iostream>
#include <vector>
#include <string>
#include <cmath>
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
    GazeVector2 nose_target;
};

void run_test_with_model(const string& label, const std::vector<cv::Point3f>& model_points, const vector<TargetGaze>& targets, const CameraPlacement& placement) {
    string model_path = "project/models/face_detection_yunet_2023mar.onnx";
    cv::Ptr<cv::FaceDetectorYN> detector = cv::FaceDetectorYN::create(
        model_path, "", cv::Size(320, 320), 0.6f, 0.3f, 5000
    );

    ProjectionEngine engine;
    engine.set_screen_size_pixels(GazeVector2(1920.0, 1080.0));
    engine.set_screen_size_mm(GazeVector2(305.0, 191.0));
    engine.set_camera_placement(placement);

    auto project_ray_to_screen_corrected = [&placement](const GazeVector3& origin_cam, const GazeVector3& dir_cam) -> GazeVector2 {
        double theta_rad = placement.tilt_degrees * (3.14159265358979323846 / 180.0);
        double cos_t = std::cos(theta_rad);
        double sin_t = std::sin(theta_rad);

        double O_s_x = -origin_cam.x + placement.offset.x;
        double O_s_y = -origin_cam.y * cos_t - origin_cam.z * sin_t + placement.offset.y;
        double O_s_z = origin_cam.y * sin_t - origin_cam.z * cos_t + placement.offset.z;

        double v_s_x = -dir_cam.x;
        double v_s_y = -dir_cam.y * cos_t - dir_cam.z * sin_t;
        double v_s_z = dir_cam.y * sin_t - dir_cam.z * cos_t;

        if (std::abs(v_s_z) < 1e-6) return GazeVector2(0,0);
        double t = -O_s_z / v_s_z;
        if (t < 0.0) return GazeVector2(0,0);

        return GazeVector2(O_s_x + t * v_s_x, O_s_y + t * v_s_y);
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

    cout << "\n--- Model: " << label << " ---" << endl;
    cout << "filename,target_y,proj_y,pitch_deg" << endl;

    for (const auto& tg : targets) {
        string filepath = "tests/resources/" + tg.filename;
        cv::Mat img = cv::imread(filepath);
        if (img.empty()) continue;

        detector->setInputSize(img.size());
        cv::Mat faces;
        detector->detect(img, faces);
        if (faces.empty() || faces.rows == 0) continue;

        cv::Point2f landmarks[5];
        for (int i = 0; i < 5; ++i) {
            landmarks[i].x = faces.at<float>(0, 4 + 2 * i);
            landmarks[i].y = faces.at<float>(0, 5 + 2 * i);
        }

        std::vector<cv::Point2f> image_points = {
            landmarks[0], landmarks[1], landmarks[2], landmarks[3], landmarks[4]
        };

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
        bool pnp_success = cv::solvePnP(model_points, image_points, camera_matrix, dist_coeffs, rvec, tvec, false, cv::SOLVEPNP_SQPNP);

        if (!pnp_success) continue;

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

        cout << tg.filename << "," << tg.nose_target.y << "," << nose_projected.y << "," << (head_transform.basis.get_euler_deg().x) << endl;
    }
}

int main() {
    vector<TargetGaze> targets = {
        {"self_center.jpg", GazeVector2(0.0, 0.0)},
        {"self_top_top.jpg", GazeVector2(0.0, -95.5)},
        {"self_down_down.jpg", GazeVector2(0.0, 95.5)},
        {"self_nosedown_eyesup.jpg", GazeVector2(0.0, 95.5)},
        {"self_nosetop_eyesdown.jpg", GazeVector2(0.0, -95.5)}
    };

    // Corrected placement for top camera
    CameraPlacement placement(GazeVector3(0.0, -135.0, 10.0), 15.0);

    // 1. Original model
    std::vector<cv::Point3f> original_model = {
        cv::Point3f(-30.0f, -28.676f, 0.0f),
        cv::Point3f(30.0f, -28.676f, 0.0f),
        cv::Point3f(0.0f, -5.000f, -59.859f),
        cv::Point3f(-18.462f, 31.712f, -4.550f),
        cv::Point3f(18.462f, 31.712f, -4.550f)
    };
    run_test_with_model("Original (nose_z = -59.859)", original_model, targets, placement);

    // 2. Nose depth = -30
    std::vector<cv::Point3f> model_30 = {
        cv::Point3f(-30.0f, -28.676f, 0.0f),
        cv::Point3f(30.0f, -28.676f, 0.0f),
        cv::Point3f(0.0f, -5.000f, -30.0f),
        cv::Point3f(-18.462f, 31.712f, -4.550f),
        cv::Point3f(18.462f, 31.712f, -4.550f)
    };
    run_test_with_model("Medium (nose_z = -30.0)", model_30, targets, placement);

    // 3. Nose depth = -18
    std::vector<cv::Point3f> model_18 = {
        cv::Point3f(-30.0f, -28.676f, 0.0f),
        cv::Point3f(30.0f, -28.676f, 0.0f),
        cv::Point3f(0.0f, -5.000f, -18.0f),
        cv::Point3f(-18.462f, 31.712f, -4.550f),
        cv::Point3f(18.462f, 31.712f, -4.550f)
    };
    run_test_with_model("Realistic (nose_z = -18.0)", model_18, targets, placement);

    return 0;
}
