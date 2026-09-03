#pragma once

#include <cmath>
#include <algorithm>

namespace Gaze {

/**
 * @brief Generic temporal filter with confidence-scaled convergence and smoothstep S-curve dropout decay.
 *
 * Designed for signals that require smooth tracking during active sampling and graceful, non-penalizing
 * decay during brief dropouts (e.g. tracking priors, orientation hints).
 *
 * Dual-regime dynamics:
 * 1. Active Sampling (has_sample == true):
 *    Filters values with exponential smoothing whose effective time constant scales inversely with confidence:
 *    tau_eff = base_tau_track / confidence. High confidence gives rapid convergence; low confidence dampens jitter.
 *
 * 2. Dropout Decay (has_sample == false):
 *    Holds the last valid state over a configurable grace window (hold_window_s) with zero initial rate of
 *    change (w'(0) = 0). Beyond the hold window, smoothly accelerates and decelerates decay towards the neutral
 *    baseline along a cubic Hermite smoothstep S-curve (w(t) = 1 - 3t^2 + 2t^3) over decay_window_s.
 */
class SigmoidalFilter {
private:
    float current_val = 0.0f;
    float last_valid_val = 0.0f;
    float neutral_val = 0.0f;
    double last_timestamp_s = -1.0;
    double time_lost_s = 0.0;

    float hold_window_s = 0.060f;    // Duration in seconds to hold last valid state without decay (~3-4 frames at 60fps)
    float decay_window_s = 0.250f;   // Total dropout duration in seconds to reach neutral baseline
    float base_tau_track_s = 0.003f; // Base tracking time constant (smoothing)
    float min_cutoff_delta = 0.02f;  // Snap-to-neutral threshold

public:
    SigmoidalFilter(float neutral_value = 0.0f,
                    float hold_s = 0.060f,
                    float decay_s = 0.250f,
                    float base_tau_s = 0.003f,
                    float cutoff_delta = 0.02f)
        : current_val(neutral_value),
          last_valid_val(neutral_value),
          neutral_val(neutral_value),
          hold_window_s(hold_s),
          decay_window_s(decay_s),
          base_tau_track_s(base_tau_s),
          min_cutoff_delta(cutoff_delta) {}

    /**
     * @brief Resets filter state and timestamps to specified initial/neutral value.
     */
    void reset(float initial_val = 0.0f) {
        current_val = initial_val;
        last_valid_val = initial_val;
        last_timestamp_s = -1.0;
        time_lost_s = 0.0;
    }

    /**
     * @brief Updates filter state with a new sample observation or dropout.
     *
     * @param has_sample True if valid measurement is present; false if signal dropped out.
     * @param sample_val Observed measurement value.
     * @param confidence Detection / measurement confidence [0.0 .. 1.0].
     * @param timestamp_s Current monotonic timestamp in seconds.
     * @return Filtered output value.
     */
    float update(bool has_sample, float sample_val, float confidence = 1.0f, double timestamp_s = -1.0) {
        double dt = (last_timestamp_s >= 0.0 && timestamp_s >= last_timestamp_s)
            ? (timestamp_s - last_timestamp_s)
            : (1.0 / 60.0);
        last_timestamp_s = timestamp_s;
        if (dt > 0.5) dt = 0.5;

        if (has_sample) {
            time_lost_s = 0.0;
            float c = std::clamp(confidence, 0.1f, 1.0f);
            float tau = base_tau_track_s / c;
            float alpha = static_cast<float>(1.0 - std::exp(-dt / tau));
            current_val += alpha * (sample_val - current_val);
            last_valid_val = current_val;
        } else {
            time_lost_s += dt;
            float t_norm = std::clamp((static_cast<float>(time_lost_s) - hold_window_s) / (decay_window_s - hold_window_s), 0.0f, 1.0f);
            // Smoothstep Hermite S-curve: S(0) = 0, S(1) = 1, S'(0) = 0, S'(1) = 0
            float decay_weight = 1.0f - (t_norm * t_norm * (3.0f - 2.0f * t_norm));
            current_val = neutral_val + (last_valid_val - neutral_val) * decay_weight;
            if (std::abs(current_val - neutral_val) < min_cutoff_delta || t_norm >= 1.0f) {
                current_val = neutral_val;
            }
        }
        return current_val;
    }

    float get_value() const { return current_val; }
    float get_neutral_value() const { return neutral_val; }
    double get_time_lost_s() const { return time_lost_s; }
    bool is_holding() const { return time_lost_s > 0.0 && time_lost_s <= hold_window_s; }
    bool is_decaying() const { return time_lost_s > hold_window_s && current_val != neutral_val; }

    void set_hold_window(float s) { hold_window_s = s; }
    void set_decay_window(float s) { decay_window_s = s; }
    void set_base_tau(float s) { base_tau_track_s = s; }
    void set_min_cutoff_delta(float d) { min_cutoff_delta = d; }
    void set_neutral_value(float v) { neutral_val = v; }
};

} // namespace Gaze
