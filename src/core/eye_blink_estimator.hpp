#pragma once
#include <cstdint>
#include <cstddef>

namespace Gaze {

/**
 * @brief Abstract interface for estimating per-eye openness/blink confidence.
 */
class EyeBlinkEstimator {
public:
    virtual ~EyeBlinkEstimator() = default;

    /**
     * @brief Estimate openness confidence [0.0f, 1.0f] for left and right eyes.
     * @param left_crop_rgb 60x60 RGB pixel buffer for left eye (or nullptr).
     * @param right_crop_rgb 60x60 RGB pixel buffer for right eye (or nullptr).
     * @param out_left_openness Output float for left eye openness [0.0 = closed, 1.0 = open].
     * @param out_right_openness Output float for right eye openness [0.0 = closed, 1.0 = open].
     */
    virtual void estimate_openness(
        const uint8_t* left_crop_rgb,
        const uint8_t* right_crop_rgb,
        float& out_left_openness,
        float& out_right_openness
    ) = 0;
};

} // namespace Gaze
