#include "projection_engine.hpp"
#include <cmath>

namespace Gaze {

// Constant for Pi
static constexpr double PI = 3.14159265358979323846;

double ProjectionEngine::estimate_depth_z(double eye_distance_px, double ipd_mm) const {
    if (eye_distance_px <= 0.0) return 0.0;
    return (ipd_mm * camera_focal_length_px) / eye_distance_px;
}

GazeVector3 ProjectionEngine::apply_3d_bias(const GazeVector3& raw_gaze_dir) const {
    GazeVector3 v = raw_gaze_dir.normalized();

    // Prevent asin domain errors by clipping v.y to [-1, 1]
    double vy = v.y;
    if (vy > 1.0) vy = 1.0;
    else if (vy < -1.0) vy = -1.0;

    double yaw = std::atan2(v.x, v.z);
    double pitch = std::asin(vy);

    double calib_yaw = yaw + calibration.bias_yaw;
    double calib_pitch = pitch + calibration.bias_pitch;

    double cos_pitch = std::cos(calib_pitch);
    return GazeVector3(
        std::sin(calib_yaw) * cos_pitch,
        std::sin(calib_pitch),
        std::cos(calib_yaw) * cos_pitch
    ).normalized();
}

bool ProjectionEngine::project_gaze(const GazeVector3& gaze_origin_cam, 
                                  const GazeVector3& raw_gaze_dir_cam, 
                                  GazeVector2& out_pixel) const {
    // 1. Apply 3D angular bias to get calibrated direction vector in Camera Space
    GazeVector3 v = apply_3d_bias(raw_gaze_dir_cam);

    double theta_rad = placement.tilt_degrees * (PI / 180.0);
    double cos_t = std::cos(theta_rad);
    double sin_t = std::sin(theta_rad);

    if (screen_size_mm.x <= 0.0 || screen_size_mm.y <= 0.0 ||
        screen_size_pixels.x <= 0.0 || screen_size_pixels.y <= 0.0) {
        return false;
    }

    double scale_x = screen_size_pixels.x / screen_size_mm.x;
    double scale_y = -screen_size_pixels.y / screen_size_mm.y;
    double W_half = screen_size_pixels.x / 2.0;
    double H_half = screen_size_pixels.y / 2.0;

    // 2. Transform origin and direction from Camera Space to Display Space
    double O_disp_x = -scale_x * gaze_origin_cam.x + placement.offset.x * scale_x + W_half;
    double O_disp_y = cos_t * scale_y * gaze_origin_cam.y + sin_t * scale_y * gaze_origin_cam.z + placement.offset.y * scale_y + H_half;
    double O_disp_z = sin_t * gaze_origin_cam.y - cos_t * gaze_origin_cam.z + placement.offset.z;

    double v_disp_x = -scale_x * v.x;
    double v_disp_y = cos_t * scale_y * v.y + sin_t * scale_y * v.z;
    double v_disp_z = sin_t * v.y - cos_t * v.z;

    // 3. Solve for ray-plane intersection parameter t at Z = 0 screen plane
    if (std::abs(v_disp_z) < 1e-6) {
        return false; // Ray is parallel to screen plane
    }

    double t = -O_disp_z / v_disp_z;
    if (t < 0.0) {
        return false; // Ray points away from the screen
    }

    // 4. Compute screen pixels and apply 2D pixel bias
    out_pixel.x = O_disp_x + v_disp_x * t + calibration.bias_pixel_x;
    out_pixel.y = O_disp_y + v_disp_y * t + calibration.bias_pixel_y;

    return true;
}

GazeVector2 ProjectionEngine::pixel_to_millimeter(const GazeVector2& pixel) const {
    if (screen_size_pixels.x <= 0.0 || screen_size_pixels.y <= 0.0 ||
        screen_size_mm.x <= 0.0 || screen_size_mm.y <= 0.0) {
        return GazeVector2(0.0, 0.0);
    }
    double scale_x = screen_size_pixels.x / screen_size_mm.x;
    double scale_y = -screen_size_pixels.y / screen_size_mm.y;
    double W_half = screen_size_pixels.x / 2.0;
    double H_half = screen_size_pixels.y / 2.0;

    return GazeVector2(
        (pixel.x - W_half) / scale_x,
        -((pixel.y - H_half) / scale_y)
    );
}

GazeVector3 ProjectionEngine::screen_mm_to_camera_space(const GazeVector2& screen_mm) const {
    double A = -screen_mm.y - placement.offset.y;
    double theta_rad = placement.tilt_degrees * (PI / 180.0);
    double cos_t = std::cos(theta_rad);
    double sin_t = std::sin(theta_rad);

    double P_cam_target_x = -screen_mm.x + placement.offset.x;
    double P_cam_target_y = A * cos_t - placement.offset.z * sin_t;
    double P_cam_target_z = A * sin_t + placement.offset.z * cos_t;

    return GazeVector3(P_cam_target_x, P_cam_target_y, P_cam_target_z);
}

bool ProjectionEngine::calibrate_3d_bias(const GazeVector3& gaze_origin_cam,
                                       const GazeVector3& raw_gaze_dir_cam,
                                       const GazeVector2& target_pixel,
                                       GazeCalibration& out_calib) const {
    if (screen_size_pixels.x <= 0.0 || screen_size_pixels.y <= 0.0 ||
        screen_size_mm.x <= 0.0 || screen_size_mm.y <= 0.0) {
        return false;
    }

    GazeVector3 P_cam_target = screen_mm_to_camera_space(pixel_to_millimeter(target_pixel));

    // 2. Compute the required 3D gaze direction vector in camera space
    GazeVector3 v_req = (P_cam_target - gaze_origin_cam).normalized();

    // 3. Extract yaw and pitch of the required vector
    double v_req_y = v_req.y;
    if (v_req_y > 1.0) v_req_y = 1.0;
    else if (v_req_y < -1.0) v_req_y = -1.0;

    double yaw_req = std::atan2(v_req.x, v_req.z);
    double pitch_req = std::asin(v_req_y);

    // 4. Extract yaw and pitch of the raw vector
    GazeVector3 v_raw = raw_gaze_dir_cam.normalized();
    double v_raw_y = v_raw.y;
    if (v_raw_y > 1.0) v_raw_y = 1.0;
    else if (v_raw_y < -1.0) v_raw_y = -1.0;

    double yaw_raw = std::atan2(v_raw.x, v_raw.z);
    double pitch_raw = std::asin(v_raw_y);

    // 5. Compute required delta biases
    out_calib.bias_yaw = yaw_req - yaw_raw;
    out_calib.bias_pitch = pitch_req - pitch_raw;

    return true;
}

bool ProjectionEngine::calibrate_2d_bias(const GazeVector3& gaze_origin_cam,
                                       const GazeVector3& raw_gaze_dir_cam,
                                       const GazeVector2& target_pixel,
                                       GazeCalibration& out_calib) const {
    // We want to project the gaze with 2D bias disabled, then compute the delta
    ProjectionEngine temp_engine = *this;
    GazeCalibration zero_calib = calibration;
    zero_calib.bias_pixel_x = 0.0;
    zero_calib.bias_pixel_y = 0.0;
    temp_engine.set_calibration(zero_calib);

    GazeVector2 projected_pixel;
    if (!temp_engine.project_gaze(gaze_origin_cam, raw_gaze_dir_cam, projected_pixel)) {
        return false;
    }

    // Pixel bias is the direct delta to match the target
    out_calib.bias_pixel_x = target_pixel.x - projected_pixel.x;
    out_calib.bias_pixel_y = target_pixel.y - projected_pixel.y;

    return true;
}

GazeTransform3D ProjectionEngine::get_head_transform_in_camera_space(const GazeVector3& opencv_translation,
                                                                     const GazeVector3& opencv_rotation_deg) const {
    // 1. T_cv_cam_to_ggaze_cam = Transform(R_X(180), zero)
    // R_X(180) = diag(1, -1, -1)
    GazeBasis3D r_x_180(
        GazeVector3(1, 0, 0),
        GazeVector3(0, -1, 0),
        GazeVector3(0, 0, -1)
    );
    GazeTransform3D T_cv_cam_to_ggaze_cam(r_x_180, GazeVector3(0, 0, 0));

    // 2. T_cv_face_to_cv_cam = Transform(R_cv, t_cv)
    GazeBasis3D R_cv = GazeBasis3D::from_euler_zyx(opencv_rotation_deg.x, opencv_rotation_deg.y, opencv_rotation_deg.z);
    GazeTransform3D T_cv_face_to_cv_cam(R_cv, opencv_translation);

    // 3. T_ggaze_face_to_cv_face = Transform(R_Z(180), zero)
    // R_Z(180) = diag(-1, -1, 1)
    GazeBasis3D r_z_180(
        GazeVector3(-1, 0, 0),
        GazeVector3(0, -1, 0),
        GazeVector3(0, 0, 1)
    );
    GazeTransform3D T_ggaze_face_to_cv_face(r_z_180, GazeVector3(0, 0, 0));

    // Chain: T_ggaze_face_to_ggaze_cam = T_cv_cam_to_ggaze_cam * T_cv_face_to_cv_cam * T_ggaze_face_to_cv_face
    return T_cv_cam_to_ggaze_cam * T_cv_face_to_cv_cam * T_ggaze_face_to_cv_face;
}

} // namespace Gaze
