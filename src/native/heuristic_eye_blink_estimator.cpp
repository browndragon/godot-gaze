#include "heuristic_eye_blink_estimator.hpp"
#include <cmath>
#include <algorithm>
#include <vector>
#include <iostream>

namespace Gaze {

float HeuristicEyeBlinkEstimator::calculate_crop_openness(const uint8_t* crop_rgb, int width, int height) {
    if (!crop_rgb) return 0.0f;
    if (width != 60 || height != 60) return 0.0f;

    // Prior Art Feature 1: Sclera (Eye White) Pixel Count
    // Sclera pixels reside in left (x: 10..22) and right (x: 38..50) regions flanking the pupil (y: 20..40).
    // Sclera pixels are bright (Y > 100) and have low color saturation (|R-G| <= 18 and |G-B| <= 18).
    int sclera_pixel_count = 0;
    int total_sclera_region_pixels = 0;

    double sclera_sum_luminance = 0.0;

    for (int y = 20; y < 40; y++) {
        for (int x : {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47}) {
            int idx = (y * 60 + x) * 3;
            int r = crop_rgb[idx + 0];
            int g = crop_rgb[idx + 1];
            int b = crop_rgb[idx + 2];
            double y_lum = 0.299 * r + 0.587 * g + 0.114 * b;

            total_sclera_region_pixels++;

            // Sclera test: low chromaticity difference & high luminance
            if (std::abs(r - g) <= 20 && std::abs(g - b) <= 20 && y_lum > 100.0) {
                sclera_pixel_count++;
                sclera_sum_luminance += y_lum;
            }
        }
    }

    double sclera_fraction = static_cast<double>(sclera_pixel_count) / total_sclera_region_pixels;

    // Prior Art Feature 2: Central Iris Trough Dip Depth
    // Measure central pupil region (x: 23..37, y: 20..40) min luminance vs sclera luminance
    double min_pupil_lum = 255.0;
    for (int y = 20; y < 40; y++) {
        for (int x = 23; x < 37; x++) {
            int idx = (y * 60 + x) * 3;
            int r = crop_rgb[idx + 0];
            int g = crop_rgb[idx + 1];
            int b = crop_rgb[idx + 2];
            double y_lum = 0.299 * r + 0.587 * g + 0.114 * b;
            if (y_lum < min_pupil_lum) {
                min_pupil_lum = y_lum;
            }
        }
    }

    double avg_sclera_lum = (sclera_pixel_count > 0) ? (sclera_sum_luminance / sclera_pixel_count) : 120.0;
    double iris_dip_ratio = (avg_sclera_lum - min_pupil_lum) / avg_sclera_lum;
    iris_dip_ratio = std::max(0.0, std::min(1.0, iris_dip_ratio));

    // Combine Sclera Fraction & Iris Dip Depth into Openness Score
    double sclera_score = std::min(1.0, sclera_fraction / 0.25); // 25%+ sclera pixels -> score 1.0
    double dip_score = std::max(0.0, (iris_dip_ratio - 0.20) / 0.40); // 60%+ dip -> score 1.0

    double combined_openness = sclera_score * 0.6 + dip_score * 0.4;

    return static_cast<float>(std::max(0.0, std::min(1.0, combined_openness)));
}

void HeuristicEyeBlinkEstimator::estimate_openness(
    const uint8_t* left_crop_rgb,
    const uint8_t* right_crop_rgb,
    float& out_left_openness,
    float& out_right_openness
) {
    out_left_openness = calculate_crop_openness(left_crop_rgb, 60, 60);
    out_right_openness = calculate_crop_openness(right_crop_rgb, 60, 60);
}

} // namespace Gaze
