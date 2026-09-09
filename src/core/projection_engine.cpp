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
            x_disp_mm = W_half - (pt_cam.x + placement.offset.x);
            y_disp_mm = -(pt_cam.y - placement.offset.y);
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
            if (t < 0.0)
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

        double scale_x = screen_size_pixels.x / screen_size_mm.x;
        double scale_y = screen_size_pixels.y / screen_size_mm.y;
        out_pixel.x = x_disp_mm * scale_x;
        out_pixel.y = y_disp_mm * scale_y;
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
} // namespace Gaze
