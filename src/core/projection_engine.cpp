#include "projection_engine.hpp"
#include "math_defs.hpp" // For PI & DEG_TO_RAD
#include <cmath>

namespace Gaze
{

    double ProjectionEngine::estimate_depth_z(double eye_distance_px, double ipd_mm) const
    {
        if (eye_distance_px <= 0.0)
            return 0.0;
        return (ipd_mm * camera_focal_length_px) / eye_distance_px;
    }

    GazeVector3 ProjectionEngine::apply_3d_bias(const GazeVector3 &raw_gaze_dir) const
    {
        return apply_3d_bias_vector(
            raw_gaze_dir,
            GazeVector2(calibration.bias_pitch, calibration.bias_yaw),
            GazeVector2(calibration.scale_pitch, calibration.scale_yaw));
    }

    bool ProjectionEngine::project_gaze(const GazeVector3 &gaze_origin_cam,
                                        const GazeVector3 &raw_gaze_dir_cam,
                                        GazeVector2 &out_pixel) const
    {
        if (screen_size_mm.x <= 0.0 || screen_size_mm.y <= 0.0 ||
            screen_size_pixels.x <= 0.0 || screen_size_pixels.y <= 0.0)
        {
            return false;
        }

        GazeVector2 pos_mm;
        if (!project_ray_to_screen_mm(
                gaze_origin_cam,
                raw_gaze_dir_cam,
                placement.offset,
                placement.tilt_degrees,
                screen_size_mm,
                pos_mm))
        {
            return false;
        }

        double scale_x = screen_size_pixels.x / screen_size_mm.x;
        double scale_y = screen_size_pixels.y / screen_size_mm.y;
        out_pixel.x = pos_mm.x * scale_x;
        out_pixel.y = pos_mm.y * scale_y;
        return true;
    }

    GazeVector2 ProjectionEngine::pixel_to_millimeter(const GazeVector2 &pixel) const
    {
        if (screen_size_pixels.x <= 0.0 || screen_size_pixels.y <= 0.0 ||
            screen_size_mm.x <= 0.0 || screen_size_mm.y <= 0.0)
        {
            return GazeVector2(0.0, 0.0);
        }
        double scale_x = screen_size_pixels.x / screen_size_mm.x;
        double scale_y = -screen_size_pixels.y / screen_size_mm.y;
        double W_half = screen_size_pixels.x / 2.0;
        double H_half = screen_size_pixels.y / 2.0;

        return GazeVector2(
            (pixel.x - W_half) / scale_x,
            -((pixel.y - H_half) / scale_y));
    }

    GazeVector3 ProjectionEngine::screen_mm_to_camera_space(const GazeVector2 &screen_mm) const
    {
        double A = -screen_mm.y - placement.offset.y;
        double theta_rad = placement.tilt_degrees * DEG_TO_RAD;
        double cos_t = std::cos(theta_rad);
        double sin_t = std::sin(theta_rad);

        double P_cam_target_x = -screen_mm.x + placement.offset.x;
        double P_cam_target_y = A * cos_t - placement.offset.z * sin_t;
        double P_cam_target_z = A * sin_t + placement.offset.z * cos_t;

        return GazeVector3(P_cam_target_x, P_cam_target_y, P_cam_target_z);
    }
} // namespace Gaze
