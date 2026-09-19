#include "projection_engine.hpp"
#include <cmath>

namespace Gaze
{

    double ProjectionEngine::estimate_depth_z(double eye_distance_px, double ipd_mm) const
    {
        if (eye_distance_px <= 0.0)
            return 0.0;
        return (ipd_mm * camera_focal_length_px) / eye_distance_px;
    }


    bool ProjectionEngine::project_gaze(const GodotCameraVector3 &gaze_origin_cam,
                                        const GodotCameraVector3 &raw_gaze_dir_cam,
                                        GodotDisplayVector2 &out_pixel) const
    {
        if (screen_size_mm.x <= 0.0 || screen_size_mm.y <= 0.0 ||
            screen_size_pixels.x <= 0.0 || screen_size_pixels.y <= 0.0)
        {
            return false;
        }

        double x_disp_mm = 0.0;
        double y_disp_mm = 0.0;

        if (std::abs(placement.tilt_degrees) < 1e-4 && std::abs(placement.offset.z) < 1e-4)
        {
            GodotCameraVector3 pt_cam = project_ray_to_camera_plane(gaze_origin_cam, raw_gaze_dir_cam);
            if (!pt_cam.is_finite())
            {
                return false;
            }
            double W_half = screen_size_mm.x * 0.5;
            double H_half = screen_size_mm.y * 0.5;
            x_disp_mm = W_half - (pt_cam.x + placement.offset.x);
            y_disp_mm = H_half - (pt_cam.y + placement.offset.y);
        }
        else
        {
            double theta_rad = placement.tilt_degrees * DEG_TO_RAD;
            double cos_t = std::cos(theta_rad);
            double sin_t = std::sin(theta_rad);

            double O_disp_z = sin_t * (gaze_origin_cam.y - placement.offset.y) + cos_t * (gaze_origin_cam.z - placement.offset.z);
            double v_disp_z = sin_t * raw_gaze_dir_cam.y + cos_t * raw_gaze_dir_cam.z;

            if (std::abs(v_disp_z) < 1e-6)
            {
                return false;
            }

            double t = -O_disp_z / v_disp_z;
            if (t <= 0.0)
            {
                return false;
            }

            double W_half = screen_size_mm.x * 0.5;
            double int_x = gaze_origin_cam.x + t * raw_gaze_dir_cam.x;
            double int_y = gaze_origin_cam.y + t * raw_gaze_dir_cam.y;
            double int_z = gaze_origin_cam.z + t * raw_gaze_dir_cam.z;

            x_disp_mm = W_half - (int_x + placement.offset.x);
            y_disp_mm = -(int_y - placement.offset.y) * cos_t + (int_z - placement.offset.z) * sin_t;
        }

        double W_phys = screen_size_mm.x;
        double H_phys = screen_size_mm.y;
        double W_px = screen_size_pixels.x;
        double H_px = screen_size_pixels.y;

        double x_vp_mm = 0.0;
        double y_vp_mm = 0.0;
        double scale_x = W_px / W_phys;
        double scale_y = H_px / H_phys;

        switch (display_orientation)
        {
        case DisplayOrientation::ORIENTATION_0:
            x_vp_mm = x_disp_mm;
            y_vp_mm = y_disp_mm;
            scale_x = W_px / W_phys;
            scale_y = H_px / H_phys;
            break;

        case DisplayOrientation::ORIENTATION_90:
            x_vp_mm = H_phys - y_disp_mm;
            y_vp_mm = x_disp_mm;
            scale_x = H_px / H_phys;
            scale_y = W_px / W_phys;
            break;

        case DisplayOrientation::ORIENTATION_180:
            x_vp_mm = W_phys - x_disp_mm;
            y_vp_mm = H_phys - y_disp_mm;
            scale_x = W_px / W_phys;
            scale_y = H_px / H_phys;
            break;

        case DisplayOrientation::ORIENTATION_270:
            x_vp_mm = y_disp_mm;
            y_vp_mm = W_phys - x_disp_mm;
            scale_x = H_px / H_phys;
            scale_y = W_px / W_phys;
            break;
        }

        out_pixel.x = x_vp_mm * scale_x - window_offset_pixels.x;
        out_pixel.y = y_vp_mm * scale_y - window_offset_pixels.y;
        return true;
    }

    SpacedVector2<Space::GodotDisplayMm> ProjectionEngine::pixel_to_millimeter(const GodotDisplayVector2 &pixel) const
    {
        if (screen_size_pixels.x <= 0.0 || screen_size_pixels.y <= 0.0 ||
            screen_size_mm.x <= 0.0 || screen_size_mm.y <= 0.0)
        {
            return SpacedVector2<Space::GodotDisplayMm>(0.0, 0.0);
        }
        double scale_x = screen_size_pixels.x / screen_size_mm.x;
        double scale_y = -screen_size_pixels.y / screen_size_mm.y;
        double W_half = screen_size_pixels.x / 2.0;
        double H_half = screen_size_pixels.y / 2.0;

        return SpacedVector2<Space::GodotDisplayMm>(
            (pixel.x - W_half) / scale_x,
            -((pixel.y - H_half) / scale_y));
    }

    GodotCameraVector3 ProjectionEngine::screen_mm_to_camera_space(const SpacedVector2<Space::GodotDisplayMm> &screen_mm) const
    {
        double A = -screen_mm.y - placement.offset.y;
        double theta_rad = placement.tilt_degrees * DEG_TO_RAD;
        double cos_t = std::cos(theta_rad);
        double sin_t = std::sin(theta_rad);

        double P_cam_target_x = -screen_mm.x + placement.offset.x;
        double P_cam_target_y = A * cos_t - placement.offset.z * sin_t;
        double P_cam_target_z = A * sin_t + placement.offset.z * cos_t;

        return GodotCameraVector3(P_cam_target_x, P_cam_target_y, P_cam_target_z);
    }

    GodotCameraVector3 apply_head_space_calibration(
        const GodotCameraVector3 &raw_gaze_dir_cam,
        const SpacedBasis<Space::GodotFaceLocal, Space::GodotCamera> &head_basis_cam,
        const GazeBioCalibrationParams &bio_params)
    {
        // 1. Transform raw gaze ray from camera space to head space:
        SpacedBasis<Space::GodotCamera, Space::GodotFaceLocal> cam_to_head = head_basis_cam.transposed();
        GodotFaceVector3 v_head = cam_to_head.transform(raw_gaze_dir_cam).normalized();

        // 2. Convert v_head to spherical angles (pitch psi, yaw phi):
        // In Godot Face Local Space: -Z is forward out of the face, +X is right, +Y is up.
        double vy_clamped = std::clamp(v_head.y, -1.0, 1.0);
        double pitch_rad = std::asin(vy_clamped);
        double yaw_rad = std::atan2(v_head.x, -v_head.z);

        // 3. Apply angular bias (Angle Kappa):
        double pitch_cal = pitch_rad + (bio_params.bias_pitch_deg * DEG_TO_RAD);
        double yaw_cal = yaw_rad + (bio_params.bias_yaw_deg * DEG_TO_RAD);

        // 4. Convert calibrated spherical angles back to head-space unit vector (-Z forward):
        double cos_pitch = std::cos(pitch_cal);
        GodotFaceVector3 v_head_cal(
            std::sin(yaw_cal) * cos_pitch,
            std::sin(pitch_cal),
            -std::cos(yaw_cal) * cos_pitch
        );

        // 5. Transform calibrated ray back to camera space:
        return head_basis_cam.transform(v_head_cal).normalized();
    }

    bool solve_head_space_bias(
        const std::vector<double> &measured_angles_rad,
        const std::vector<double> &target_angles_rad,
        double &out_bias_deg)
    {
        if (measured_angles_rad.empty() || measured_angles_rad.size() != target_angles_rad.size())
        {
            out_bias_deg = 0.0;
            return false;
        }

        size_t N = measured_angles_rad.size();
        double sum_sin = 0.0;
        double sum_cos = 0.0;

        for (size_t i = 0; i < N; ++i)
        {
            double delta = wrap_angle_rad(target_angles_rad[i] - measured_angles_rad[i]);
            sum_sin += std::sin(delta);
            sum_cos += std::cos(delta);
        }

        if (std::abs(sum_sin) < 1e-12 && std::abs(sum_cos) < 1e-12)
        {
            out_bias_deg = 0.0;
            return false;
        }

        double mean_delta_rad = std::atan2(sum_sin, sum_cos);
        out_bias_deg = mean_delta_rad * RAD_TO_DEG;
        return true;
    }

} // namespace Gaze
