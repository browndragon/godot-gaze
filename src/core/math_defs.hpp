/**
 * @file math_defs.hpp
 * @brief Zero-Dependency Image and Geometric Math utilities
 *
 * Implements lightweight image warping, cropping, bilinear resizing,
 * and pinhole camera geometric utilities.
 */
#pragma once

#include <cmath>
#include <type_traits>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace Gaze
{

    // Bilinear resize and BGR-to-RGB conversion helper
    inline void resize_bgr_to_rgb(const uint8_t *src, int src_w, int src_h, uint8_t *dst, int dst_w, int dst_h)
    {
        float x_ratio = ((float)(src_w - 1)) / dst_w;
        float y_ratio = ((float)(src_h - 1)) / dst_h;
        for (int i = 0; i < dst_h; i++)
        {
            for (int j = 0; j < dst_w; j++)
            {
                int x = (int)(x_ratio * j);
                int y = (int)(y_ratio * i);
                float x_diff = (x_ratio * j) - x;
                float y_diff = (y_ratio * i) - y;
                int src_idx = (y * src_w + x) * 3;

                for (int c = 0; c < 3; c++)
                {
                    float a = src[src_idx + c];
                    float b = src[src_idx + 3 + c];
                    float d = src[src_idx + src_w * 3 + c];
                    float e = src[src_idx + src_w * 3 + 3 + c];

                    float val = a * (1.0f - x_diff) * (1.0f - y_diff) +
                                b * (x_diff) * (1.0f - y_diff) +
                                d * (y_diff) * (1.0f - x_diff) +
                                e * (x_diff) * (y_diff);

                    // Write as RGB: destination index c is swapped for R/B (c=0 B -> RGB red; c=2 R -> RGB blue)
                    // BGR to RGB channel reversal
                    dst[(i * dst_w + j) * 3 + (2 - c)] = (uint8_t)val;
                }
            }
        }
    }

    inline void crop_and_resize_bgr(
        const unsigned char *src, int src_w, int src_h,
        float crop_x, float crop_y, float crop_w, float crop_h,
        unsigned char *dst, int dst_w, int dst_h)
    {
        if (dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0 || crop_w <= 0.0f || crop_h <= 0.0f) return;

        float scale_x = crop_w / static_cast<float>(dst_w);
        float scale_y = crop_h / static_cast<float>(dst_h);

        for (int y = 0; y < dst_h; ++y)
        {
            float src_y = crop_y + (y + 0.5f) * scale_y - 0.5f;
            src_y = std::max(0.0f, std::min(src_y, static_cast<float>(src_h - 1)));
            int y0 = static_cast<int>(std::floor(src_y));
            int y1 = std::min(y0 + 1, src_h - 1);
            float dy = src_y - y0;

            for (int x = 0; x < dst_w; ++x)
            {
                float src_x = crop_x + (x + 0.5f) * scale_x - 0.5f;
                src_x = std::max(0.0f, std::min(src_x, static_cast<float>(src_w - 1)));
                int x0 = static_cast<int>(std::floor(src_x));
                int x1 = std::min(x0 + 1, src_w - 1);
                float dx = src_x - x0;

                for (int c = 0; c < 3; ++c)
                {
                    float corners[4] = {
                        static_cast<float>(src[(y0 * src_w + x0) * 3 + c]),
                        static_cast<float>(src[(y0 * src_w + x1) * 3 + c]),
                        static_cast<float>(src[(y1 * src_w + x0) * 3 + c]),
                        static_cast<float>(src[(y1 * src_w + x1) * 3 + c])
                    };
                    float val = (1.0f - dx) * (1.0f - dy) * corners[0] +
                                dx * (1.0f - dy) * corners[1] +
                                (1.0f - dx) * dy * corners[2] +
                                dx * dy * corners[3];
                    dst[(y * dst_w + x) * 3 + c] =
                        static_cast<unsigned char>(std::max(0.0f, std::min(255.0f, val)));
                }
            }
        }
    }

    inline void crop_and_resize_bgr_to_rgb(const uint8_t *src, int src_w, int src_h, float roi_x, float roi_y, float roi_w, float roi_h, uint8_t *dst, int dst_w, int dst_h)
    {
        if (roi_w <= 0.0f || roi_h <= 0.0f) {
            resize_bgr_to_rgb(src, src_w, src_h, dst, dst_w, dst_h);
            return;
        }
        float x_ratio = roi_w / dst_w;
        float y_ratio = roi_h / dst_h;
        for (int i = 0; i < dst_h; i++)
        {
            for (int j = 0; j < dst_w; j++)
            {
                int x = std::max(0, std::min(src_w - 1, (int)(roi_x + x_ratio * j)));
                int y = std::max(0, std::min(src_h - 1, (int)(roi_y + y_ratio * i)));
                int src_idx = (y * src_w + x) * 3;

                dst[(i * dst_w + j) * 3 + 0] = src[src_idx + 2];
                dst[(i * dst_w + j) * 3 + 1] = src[src_idx + 1];
                dst[(i * dst_w + j) * 3 + 2] = src[src_idx + 0];
            }
        }
    }

    static constexpr double PI = 3.14159265358979323846;
    static constexpr double TAU = 6.28318530717958647692;
    static constexpr double DEG_TO_RAD = PI / 180.0;
    static constexpr double RAD_TO_DEG = 180.0 / PI;

    // Default horizontal field of view in degrees.
    // A ratio of 1000/640 (1.5625) corresponds to a horizontal FOV of ~35.49 degrees:
    // f_x / W = 1 / (2 * tan(FOV/2)) => FOV = 2 * arctan(1 / (2 * 1.5625)) = 35.4885 degrees.
    static constexpr double DEFAULT_CAMERA_FOV_DEGREES = 35.488537576579634;
    static constexpr double DEFAULT_FOCAL_TO_WIDTH_RATIO = 1.5625;

    // Computes focal length in pixels from camera sensor width and horizontal field of view.
    inline double get_focal_length_px(double width_px, double fov_degrees)
    {
        return width_px / (2.0 * std::tan(fov_degrees * DEG_TO_RAD * 0.5));
    }

    // Computes focal length of a scaled image: f' = f_original * (new_dim / original_dim)
    inline double get_focal_length_under_scaling(double f_original, double original_dim, double new_dim)
    {
        if (original_dim <= 0.0) return 0.0;
        return f_original * (new_dim / original_dim);
    }

    // Computes the expected pixel width of a credit card on screen for a given HFOV and distance
    inline double get_card_width_px(double fov_degrees, double card_distance_mm, double frame_width, double card_width_mm = 85.603)
    {
        if (card_distance_mm <= 0.0) return 0.0;
        double fov_rad = fov_degrees * DEG_TO_RAD;
        double denom = 2.0 * card_distance_mm * std::tan(fov_rad * 0.5);
        if (denom <= 0.0) return 0.0;
        return (frame_width * card_width_mm) / denom;
    }

    // Converts Diagonal FOV to Horizontal FOV given current aspect ratio
    inline double diagonal_to_horizontal_fov(double diagonal_fov_degrees, double width, double height)
    {
        if (width <= 0.0 || height <= 0.0 || diagonal_fov_degrees <= 0.0) return 0.0;
        double diag_rad = diagonal_fov_degrees * DEG_TO_RAD;
        double diag_px = std::sqrt(width * width + height * height);
        double h_fov_rad = 2.0 * std::atan((width / diag_px) * std::tan(diag_rad * 0.5));
        return h_fov_rad * RAD_TO_DEG;
    }

    // Binary-compatible layout with Godot's Rect2 class.
    // Storing sequentially as x, y (position) and width, height (size) is binary identical 
    // to a struct of two Vector2 components, allowing direct memory mapping.
    struct GazeRect
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;

        constexpr GazeRect() = default;
        constexpr GazeRect(float px, float py, float pw, float ph) : x(px), y(py), width(pw), height(ph) {}

        constexpr float area() const
        {
            return width * height;
        }
    };

    /**
     * @brief Computes default pinhole focal length in pixels for a given frame width and horizontal FOV.
     * Standard webcam default: HFOV = 65.0 deg.
     */
    inline double calculate_default_focal_length(double frame_width, double hfov_deg = 65.0)
    {
        double hfov_rad = hfov_deg * (PI / 180.0);
        return frame_width / (2.0 * std::tan(hfov_rad * 0.5));
    }

    /**
     * @brief Rotates a 3-channel 8-bit image by angle_rad around its center.
     * Operates identically on 3-byte-per-pixel buffers regardless of channel order.
     */
    inline void rotate_image(const unsigned char *src, int w, int h, unsigned char *dst, float angle_rad)
    {
        float cos_a = std::cos(angle_rad);
        float sin_a = std::sin(angle_rad);
        float cx = w / 2.0f;
        float cy = h / 2.0f;

        for (int y = 0; y < h; ++y)
        {
            float dy = y - cy;
            for (int x = 0; x < w; ++x)
            {
                float dx = x - cx;
                float src_x = cx + dx * cos_a + dy * sin_a;
                float src_y = cy - dx * sin_a + dy * cos_a;

                int dst_idx = (y * w + x) * 3;

                // Clamp to valid source coordinates (replicate border) to avoid CNN edge artifacts
                src_x = std::max(0.0f, std::min(static_cast<float>(w - 1.001f), src_x));
                src_y = std::max(0.0f, std::min(static_cast<float>(h - 1.001f), src_y));

                int x0 = static_cast<int>(std::floor(src_x));
                int y0 = static_cast<int>(std::floor(src_y));
                int x1 = std::min(w - 1, x0 + 1);
                int y1 = std::min(h - 1, y0 + 1);
                float tx = src_x - x0;
                float ty = src_y - y0;

                for (int c = 0; c < 3; ++c)
                {
                    float p00 = src[(y0 * w + x0) * 3 + c];
                    float p10 = src[(y0 * w + x1) * 3 + c];
                    float p01 = src[(y1 * w + x0) * 3 + c];
                    float p11 = src[(y1 * w + x1) * 3 + c];

                    float val = (1.0f - tx) * (1.0f - ty) * p00 +
                                tx * (1.0f - ty) * p10 +
                                (1.0f - tx) * ty * p01 +
                                tx * ty * p11;
                    dst[dst_idx + c] = static_cast<unsigned char>(std::max(0.0f, std::min(255.0f, val)));
                }
            }
        }
    }

    static_assert(std::is_standard_layout<GazeRect>::value, "GazeRect must be standard-layout");
    static_assert(std::is_trivially_copyable<GazeRect>::value, "GazeRect must be trivially copyable");

} // namespace Gaze
