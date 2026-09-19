/**
 * @file fill_accumulator.hpp
 * @brief Zero-dependency 1D potential FillAccumulator with caller-provided error metrics.
 */

#pragma once

#include <algorithm>

namespace Gaze {

class FillAccumulator {
private:
    double fill = 0.0;
    double fill_sec = 0.50;
    double drain_sec = 0.25;
    double characteristic_error = 100.0;

public:
    FillAccumulator() = default;

    FillAccumulator(double p_fill_sec, double p_drain_sec, double p_char_error = 100.0)
        : fill_sec(p_fill_sec), drain_sec(p_drain_sec), characteristic_error(p_char_error) {}

    void reset() {
        fill = 0.0;
    }

    void set_fill(double p_fill) {
        fill = std::max(0.0, std::min(1.0, p_fill));
    }

    double get_fill() const {
        return fill;
    }

    bool is_filled() const {
        return fill >= 0.9999;
    }

    bool is_empty() const {
        return fill <= 0.0001;
    }

    void set_fill_sec(double p_sec) {
        fill_sec = std::max(0.001, p_sec);
    }

    double get_fill_sec() const {
        return fill_sec;
    }

    void set_drain_sec(double p_sec) {
        drain_sec = std::max(0.001, p_sec);
    }

    double get_drain_sec() const {
        return drain_sec;
    }

    void set_characteristic_error(double p_error) {
        characteristic_error = std::max(1.0, p_error);
    }

    double get_characteristic_error() const {
        return characteristic_error;
    }

    void update(double dt, double error) {
        if (dt <= 0.0) return;

        if (error <= 0.0) {
            // On-target: linear fill rate (0 -> 1 in fill_sec)
            fill += dt / fill_sec;
        } else {
            // Off-target: drain scaled by error
            double u = error / characteristic_error;
            double rate = (1.0 + u) / drain_sec;
            fill -= dt * rate;
        }

        fill = std::max(0.0, std::min(1.0, fill));
    }
};

} // namespace Gaze
