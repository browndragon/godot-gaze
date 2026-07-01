#pragma once

#include "face_pipeline.hpp"
#include <algorithm>
#include <cmath>

namespace Gaze {

class CPUImageWarper : public ImageWarper {
private:
    static inline double get_gray_pixel(const uint8_t* src_data, int x, int y, int width, int height, int channels) {
        x = std::max(0, std::min(x, width - 1));
        y = std::max(0, std::min(y, height - 1));
        int idx = (y * width + x) * channels;
        double b = src_data[idx + 0];
        double g = src_data[idx + 1];
        double r = src_data[idx + 2];
        // BGR to Grayscale weights
        return 0.114 * b + 0.587 * g + 0.299 * r;
    }

public:
    virtual bool warp(
        const uint8_t* src_data,
        int src_width,
        int src_height,
        int src_channels,
        const GazeVector2& eye_center,
        double angle_deg,
        double scale,
        uint8_t* out_bgr_buffer
    ) override {
        double angle_rad = angle_deg * DEG_TO_RAD;
        double cos_theta = std::cos(angle_rad);
        double sin_theta = std::sin(angle_rad);

        double scale_inv = 1.0 / (scale > 1e-6 ? scale : 1.0);

        for (int v = 0; v < EyeCrops::EYE_CROP_HEIGHT; ++v) {
            double v_rel = v - (EyeCrops::EYE_CROP_HEIGHT / 2.0);
            for (int u = 0; u < EyeCrops::EYE_CROP_WIDTH; ++u) {
                double u_rel = u - (EyeCrops::EYE_CROP_WIDTH / 2.0);

                // Map target (u, v) to source (x, y) coordinates
                double x = eye_center.x + scale_inv * (u_rel * cos_theta - v_rel * sin_theta);
                double y = eye_center.y + scale_inv * (u_rel * sin_theta + v_rel * cos_theta);

                int x0 = static_cast<int>(std::floor(x));
                int y0 = static_cast<int>(std::floor(y));
                int x1 = x0 + 1;
                int y1 = y0 + 1;

                double dx = x - x0;
                double dy = y - y0;

                double p00 = get_gray_pixel(src_data, x0, y0, src_width, src_height, src_channels);
                double p10 = get_gray_pixel(src_data, x1, y0, src_width, src_height, src_channels);
                double p01 = get_gray_pixel(src_data, x0, y1, src_width, src_height, src_channels);
                double p11 = get_gray_pixel(src_data, x1, y1, src_width, src_height, src_channels);

                double val = (1.0 - dx) * (1.0 - dy) * p00 +
                             dx * (1.0 - dy) * p10 +
                             (1.0 - dx) * dy * p01 +
                             dx * dy * p11;

                uint8_t gray_val = static_cast<uint8_t>(std::max(0.0, std::min(255.0, val)));
                int out_idx = (v * EyeCrops::EYE_CROP_WIDTH + u) * EyeCrops::EYE_CROP_CHANNELS;
                out_bgr_buffer[out_idx + 0] = gray_val;
                out_bgr_buffer[out_idx + 1] = gray_val;
                out_bgr_buffer[out_idx + 2] = gray_val;
            }
        }
        return true;
    }
};

} // namespace Gaze
