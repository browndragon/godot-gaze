#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/calib3d.hpp>

using namespace std;

int main() {
    string model_path = "project/models/face_detection_yunet_2023mar.onnx";
    cv::Ptr<cv::FaceDetectorYN> detector = cv::FaceDetectorYN::create(
        model_path, "", cv::Size(320, 320), 0.6f, 0.3f, 5000
    );

    if (detector.empty()) {
        cerr << "Failed to load detector model!" << endl;
        return 1;
    }

    vector<string> filenames = {
        "self_center.jpg",
        "self_left_left.jpg",
        "self_right_right.jpg",
        "self_top_top.jpg",
        "self_down_down.jpg",
        "self_nosedown_eyesup.jpg",
        "self_noseleft_eyesright.jpg",
        "self_noseright_eyesleft.jpg",
        "self_nosetop_eyesdown.jpg"
    };

    cout << "filename,le_x,le_y,re_x,re_y,nose_x,nose_y,mr_x,mr_y,ml_x,ml_y,pnp_pitch,pnp_yaw,pnp_roll,pnp_tx,pnp_ty,pnp_tz" << endl;

    for (const auto& fname : filenames) {
        string filepath = "tests/resources/" + fname;
        cv::Mat img = cv::imread(filepath);
        if (img.empty()) {
            cerr << "Failed to load image: " << filepath << endl;
            continue;
        }

        detector->setInputSize(img.size());
        cv::Mat faces;
        detector->detect(img, faces);

        if (faces.empty() || faces.rows == 0) {
            cout << fname << ",NO_FACE_DETECTED" << endl;
            continue;
        }

        // landmarks: 0=right eye, 1=left eye, 2=nose tip, 3=mouth right, 4=mouth left
        cv::Point2f landmarks[5];
        for (int i = 0; i < 5; ++i) {
            landmarks[i].x = faces.at<float>(0, 4 + 2 * i);
            landmarks[i].y = faces.at<float>(0, 5 + 2 * i);
        }

        // solvePnP
        std::vector<cv::Point3f> model_points = {
            cv::Point3f(-30.0f, -28.676f, 0.0f), // Right eye
            cv::Point3f(30.0f, -28.676f, 0.0f),  // Left eye
            cv::Point3f(0.0f, -5.000f, -59.859f),   // Nose tip
            cv::Point3f(-18.462f, 31.712f, -4.550f), // Right mouth corner
            cv::Point3f(18.462f, 31.712f, -4.550f)  // Left mouth corner
        };

        std::vector<cv::Point2f> image_points = {
            landmarks[0], // Right eye (image left)
            landmarks[1], // Left eye (image right)
            landmarks[2], // Nose tip
            landmarks[3], // Mouth right corner (image left)
            landmarks[4]  // Mouth left corner (image right)
        };

        double cx = img.cols / 2.0;
        double cy = img.rows / 2.0;
        double fx = img.cols * 1.5625; // 2250.0 for 1440 width
        cv::Mat camera_matrix = (cv::Mat_<double>(3, 3) << 
            fx,  0.0, cx,
            0.0, fx,  cy,
            0.0, 0.0, 1.0
        );
        cv::Mat dist_coeffs = cv::Mat::zeros(4, 1, CV_64F);

        cv::Mat rvec, tvec;
        bool pnp_success = cv::solvePnP(model_points, image_points, camera_matrix, dist_coeffs, rvec, tvec, false, cv::SOLVEPNP_SQPNP);

        double pitch = 0, yaw = 0, roll = 0;
        double tx = 0, ty = 0, tz = 0;
        if (pnp_success) {
            cv::Mat R;
            cv::Rodrigues(rvec, R);
            double sy = std::sqrt(R.at<double>(0, 0) * R.at<double>(0, 0) + R.at<double>(1, 0) * R.at<double>(1, 0));
            bool singular = sy < 1e-6;
            if (!singular) {
                pitch = std::atan2(R.at<double>(2, 1), R.at<double>(2, 2)) * (180.0 / 3.141592653589793);
                yaw   = std::atan2(-R.at<double>(2, 0), sy) * (180.0 / 3.141592653589793);
                roll  = std::atan2(R.at<double>(1, 0), R.at<double>(0, 0)) * (180.0 / 3.141592653589793);
            } else {
                pitch = std::atan2(-R.at<double>(1, 2), R.at<double>(1, 1)) * (180.0 / 3.141592653589793);
                yaw   = std::atan2(-R.at<double>(2, 0), sy) * (180.0 / 3.141592653589793);
                roll  = 0.0;
            }
            tx = tvec.at<double>(0);
            ty = tvec.at<double>(1);
            tz = tvec.at<double>(2);
            
            // Apply anatomical pitch correction
            pitch -= 0.5 * ty;
            cout << fname << ": R = [" 
                 << R.at<double>(0,0) << ", " << R.at<double>(0,1) << ", " << R.at<double>(0,2) << "; "
                 << R.at<double>(1,0) << ", " << R.at<double>(1,1) << ", " << R.at<double>(1,2) << "; "
                 << R.at<double>(2,0) << ", " << R.at<double>(2,1) << ", " << R.at<double>(2,2) << "], t = ["
                 << tx << ", " << ty << ", " << tz << "], pitch = " << pitch << ", yaw = " << yaw << ", roll = " << roll << endl;
        }
    }

    return 0;
}
