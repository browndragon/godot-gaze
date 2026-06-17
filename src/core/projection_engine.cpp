// src/core/projection_engine.cpp
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
    // 1. Apply 3D angular bias to get calibrated direction vector
    GazeVector3 v = apply_3d_bias(raw_gaze_dir_cam);

    double theta_rad = placement.tilt_degrees * (PI / 180.0);

    // 2. Solve for ray-plane intersection parameter t
    // Normal of screen plane in camera space: N = (0, sin(theta), cos(theta))
    // Equation of plane: (P_cam - C_cam_in_screen_space_inverse).dot(N) = 0
    // Solving for t in: (gaze_origin_cam + t * v - P_screen_origin_cam).dot(N) = 0
    double denom = v.y * std::sin(theta_rad) + v.z * std::cos(theta_rad);
    if (std::abs(denom) < 1e-6) {
        return false; // Ray is parallel to screen plane
    }

    double num = gaze_origin_cam.y * std::sin(theta_rad) + 
                 gaze_origin_cam.z * std::cos(theta_rad) + 
                 placement.offset.z;
    
    double t = -num / denom;
    if (t < 0.0) {
        return false; // Ray points away from the screen
    }

    // 3. Compute camera space intersection point
    GazeVector3 P_int_cam = gaze_origin_cam + v * t;

    // 4. Transform intersection point to screen space (mm relative to center)
    double x_s = P_int_cam.x + placement.offset.x;
    double y_s = P_int_cam.y * std::cos(theta_rad) - P_int_cam.z * std::sin(theta_rad) + placement.offset.y;

    // 5. Convert to screen pixels
    if (screen_size_mm.x <= 0.0 || screen_size_mm.y <= 0.0) {
        return false;
    }

    double x_px_raw = (screen_size_pixels.x / 2.0) + (x_s * (screen_size_pixels.x / screen_size_mm.x));
    double y_px_raw = (screen_size_pixels.y / 2.0) + (y_s * (screen_size_pixels.y / screen_size_mm.y));

    // 6. Apply 2D pixel bias
    out_pixel.x = x_px_raw + calibration.bias_pixel_x;
    out_pixel.y = y_px_raw + calibration.bias_pixel_y;

    return true;
}

bool ProjectionEngine::calibrate_3d_bias(const GazeVector3& gaze_origin_cam,
                                       const GazeVector3& raw_gaze_dir_cam,
                                       const GazeVector2& target_pixel,
                                       GazeCalibration& out_calib) const {
    if (screen_size_pixels.x <= 0.0 || screen_size_pixels.y <= 0.0 ||
        screen_size_mm.x <= 0.0 || screen_size_mm.y <= 0.0) {
        return false;
    }

    // 1. Map target pixel back to screen space physical dimensions (mm relative to center)
    double x_s_target = (target_pixel.x - (screen_size_pixels.x / 2.0)) * (screen_size_mm.x / screen_size_pixels.x);
    double y_s_target = (target_pixel.y - (screen_size_pixels.y / 2.0)) * (screen_size_mm.y / screen_size_pixels.y);
    double z_s_target = 0.0; // By definition of the screen plane

    double theta_rad = placement.tilt_degrees * (PI / 180.0);

    // 2. Perform inverse transform from screen space to camera space
    // P_cam = R^T * (P_screen - C_screen)
    double dx = x_s_target - placement.offset.x;
    double dy = y_s_target - placement.offset.y;
    double dz = z_s_target - placement.offset.z; // -z_off

    GazeVector3 P_cam_target(
        dx,
        dy * std::cos(theta_rad) + dz * std::sin(theta_rad),
        -dy * std::sin(theta_rad) + dz * std::cos(theta_rad)
    );

    // 3. Compute the required 3D gaze direction vector in camera space
    GazeVector3 v_req = (P_cam_target - gaze_origin_cam).normalized();

    // 4. Extract yaw and pitch of the required vector
    double v_req_y = v_req.y;
    if (v_req_y > 1.0) v_req_y = 1.0;
    else if (v_req_y < -1.0) v_req_y = -1.0;

    double yaw_req = std::atan2(v_req.x, v_req.z);
    double pitch_req = std::asin(v_req_y);

    // 5. Extract yaw and pitch of the raw vector
    GazeVector3 v_raw = raw_gaze_dir_cam.normalized();
    double v_raw_y = v_raw.y;
    if (v_raw_y > 1.0) v_raw_y = 1.0;
    else if (v_raw_y < -1.0) v_raw_y = -1.0;

    double yaw_raw = std::atan2(v_raw.x, v_raw.z);
    double pitch_raw = std::asin(v_raw_y);

    // 6. Compute required delta biases
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

} // namespace Gaze
