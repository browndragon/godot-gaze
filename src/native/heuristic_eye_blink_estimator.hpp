#pragma once
#include "../core/eye_blink_estimator.hpp"

namespace Gaze {

/**
 * @brief Heuristic implementation of EyeBlinkEstimator evaluating eye crop pixel contrast & standard deviation.
 */
class HeuristicEyeBlinkEstimator : public EyeBlinkEstimator {
public:
    HeuristicEyeBlinkEstimator() = default;
    virtual ~HeuristicEyeBlinkEstimator() override = default;

    virtual void estimate_openness(
        const uint8_t* left_crop_rgb,
        const uint8_t* right_crop_rgb,
        float& out_left_openness,
        float& out_right_openness
    ) override;

    static float calculate_crop_openness(const uint8_t* crop_rgb, int width = 60, int height = 60);
};

} // namespace Gaze
