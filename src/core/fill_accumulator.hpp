/**
 * @file fill_accumulator.hpp
 * @brief Zero-dependency 2D/1D potential FillAccumulator with 2nd-order momentum and distance-scaled drain.
 */

#pragma once

#include <algorithm>

namespace Gaze {

class FillAccumulator {
private:
    double fill = 0.0;
    double velocity = 0.0;
    double fill_rate = 2.0;                    // Units/second (default: 0.5s to fill)
    double drain_rate = 4.0;                   // Units/second (default: 0.25s to drain)
    double fill_acceleration = 1000.0;         // Units/second^2 (default: fast/linear ramp)
    double drain_acceleration = 1000.0;        // Units/second^2
    double error_doubling_distance = 100.0;    // Distance in pixels where drain penalty doubles

public:
    FillAccumulator() = default;

    FillAccumulator(double p_fill_sec, double p_drain_sec, double p_error_dist = 100.0)
        : error_doubling_distance(p_error_dist) {
        set_fill_sec(p_fill_sec);
        set_drain_sec(p_drain_sec);
    }

    void reset() {
        fill = 0.0;
        velocity = 0.0;
    }

    void set_fill(double p_fill) {
        fill = std::max(0.0, std::min(1.0, p_fill));
        if (fill <= 0.0 && velocity < 0.0) velocity = 0.0;
        if (fill >= 1.0 && velocity > 0.0) velocity = 0.0;
    }

    double get_fill() const {
        return fill;
    }

    double get_velocity() const {
        return velocity;
    }

    void set_velocity(double p_vel) {
        velocity = p_vel;
    }

    bool is_filled() const {
        return fill >= 0.9999;
    }

    bool is_empty() const {
        return fill <= 0.0001;
    }

    void set_fill_rate(double p_rate) {
        fill_rate = std::max(0.001, p_rate);
    }

    double get_fill_rate() const {
        return fill_rate;
    }

    void set_drain_rate(double p_rate) {
        drain_rate = std::max(0.001, p_rate);
    }

    double get_drain_rate() const {
        return drain_rate;
    }

    void set_fill_sec(double p_sec) {
        fill_rate = 1.0 / std::max(0.001, p_sec);
    }

    double get_fill_sec() const {
        return 1.0 / fill_rate;
    }

    void set_drain_sec(double p_sec) {
        drain_rate = 1.0 / std::max(0.001, p_sec);
    }

    double get_drain_sec() const {
        return 1.0 / drain_rate;
    }

    void set_fill_acceleration(double p_accel) {
        fill_acceleration = std::max(0.001, p_accel);
    }

    double get_fill_acceleration() const {
        return fill_acceleration;
    }

    void set_drain_acceleration(double p_accel) {
        drain_acceleration = std::max(0.001, p_accel);
    }

    double get_drain_acceleration() const {
        return drain_acceleration;
    }

    void set_error_doubling_distance(double p_dist) {
        error_doubling_distance = std::max(0.0, p_dist);
    }

    double get_error_doubling_distance() const {
        return error_doubling_distance;
    }

    void set_characteristic_error(double p_error) {
        set_error_doubling_distance(p_error);
    }

    double get_characteristic_error() const {
        return get_error_doubling_distance();
    }

    void process_error_delta(double error, double delta) {
        if (delta <= 0.0) return;

        if (error <= 0.0) {
            velocity = std::min(fill_rate, velocity + fill_acceleration * delta);
        } else {
            double penalty = 1.0;
            if (error_doubling_distance > 0.0) {
                penalty += (error / error_doubling_distance);
            }
            double target_drain_vel = -drain_rate * penalty;
            double effective_drain_accel = drain_acceleration * penalty;
            velocity = std::max(target_drain_vel, velocity - effective_drain_accel * delta);
        }

        fill += velocity * delta;
        fill = std::max(0.0, std::min(1.0, fill));

        if (fill >= 1.0 && velocity > 0.0) {
            velocity = 0.0;
        } else if (fill <= 0.0 && velocity < 0.0) {
            velocity = 0.0;
        }
    }

    void process_dwell_delta(bool is_active, double delta) {
        if (delta <= 0.0) return;

        if (is_active) {
            velocity = std::min(fill_rate, velocity + fill_acceleration * delta);
        } else {
            velocity = std::max(-drain_rate, velocity - drain_acceleration * delta);
        }

        fill += velocity * delta;
        fill = std::max(0.0, std::min(1.0, fill));

        if (fill >= 1.0 && velocity > 0.0) {
            velocity = 0.0;
        } else if (fill <= 0.0 && velocity < 0.0) {
            velocity = 0.0;
        }
    }

    void update(double dt, double error) {
        process_error_delta(error, dt);
    }
};

} // namespace Gaze
