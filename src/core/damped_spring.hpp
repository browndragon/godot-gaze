/**
 * @file damped_spring.hpp
 * @brief Zero-dependency 2nd-order damped harmonic oscillator with exact closed-form analytic integration.
 */

#pragma once

#include <cmath>
#include <algorithm>

namespace Gaze {

class DampedSpring {
public:
    struct Point2D {
        float x = 0.0f;
        float y = 0.0f;
        Point2D() = default;
        Point2D(float p_x, float p_y) : x(p_x), y(p_y) {}
        Point2D operator+(const Point2D& o) const { return Point2D(x + o.x, y + o.y); }
        Point2D operator-(const Point2D& o) const { return Point2D(x - o.x, y - o.y); }
        Point2D operator*(float s) const { return Point2D(x * s, y * s); }
        float length() const { return std::sqrt(x * x + y * y); }
    };

private:
    Point2D pos = Point2D(0.0f, 0.0f);
    Point2D vel = Point2D(0.0f, 0.0f);
    double response_sec = 0.30;
    double damping_ratio = 1.0;
    bool initialized = false;

public:
    DampedSpring() = default;

    DampedSpring(double p_response_sec, double p_damping_ratio = 1.0)
        : response_sec(p_response_sec), damping_ratio(p_damping_ratio) {}

    void reset(const Point2D& p_pos, const Point2D& p_vel = Point2D(0.0f, 0.0f)) {
        pos = p_pos;
        vel = p_vel;
        initialized = true;
    }

    void set_response_sec(double p_sec) {
        response_sec = std::max(0.001, p_sec);
    }

    double get_response_sec() const {
        return response_sec;
    }

    void set_damping_ratio(double p_zeta) {
        damping_ratio = std::max(0.0, p_zeta);
    }

    double get_damping_ratio() const {
        return damping_ratio;
    }

    Point2D get_position() const {
        return pos;
    }

    Point2D get_velocity() const {
        return vel;
    }

    bool is_initialized() const {
        return initialized;
    }

    void update(double dt, const Point2D& target_pos) {
        if (!initialized) {
            reset(target_pos);
            return;
        }

        if (dt <= 0.0) {
            return;
        }

        // Natural frequency: omega_n = 4.0 / response_sec (91% settled at response_sec for zeta=1.0)
        double omega_n = 4.0 / response_sec;
        double zeta = damping_ratio;

        Point2D d0 = pos - target_pos; // Initial displacement from target
        Point2D v0 = vel;

        if (std::abs(zeta - 1.0) < 1e-4) {
            // -------------------------------------------------------------
            // Case 1: Critically Damped (zeta == 1.0)
            // -------------------------------------------------------------
            double exp_term = std::exp(-omega_n * dt);
            double c1 = 1.0 + omega_n * dt;

            pos.x = target_pos.x + (float)(exp_term * (d0.x * c1 + v0.x * dt));
            pos.y = target_pos.y + (float)(exp_term * (d0.y * c1 + v0.y * dt));

            vel.x = (float)(exp_term * (v0.x * (1.0 - omega_n * dt) - d0.x * (omega_n * omega_n * dt)));
            vel.y = (float)(exp_term * (v0.y * (1.0 - omega_n * dt) - d0.y * (omega_n * omega_n * dt)));
        } else if (zeta < 1.0) {
            // -------------------------------------------------------------
            // Case 2: Underdamped (0 <= zeta < 1.0)
            // -------------------------------------------------------------
            double alpha = zeta * omega_n;
            double omega_d = omega_n * std::sqrt(1.0 - zeta * zeta);
            double exp_term = std::exp(-alpha * dt);
            double cos_term = std::cos(omega_d * dt);
            double sin_term = std::sin(omega_d * dt);

            double c_x = (v0.x + alpha * d0.x) / omega_d;
            double c_y = (v0.y + alpha * d0.y) / omega_d;

            pos.x = target_pos.x + (float)(exp_term * (d0.x * cos_term + c_x * sin_term));
            pos.y = target_pos.y + (float)(exp_term * (d0.y * cos_term + c_y * sin_term));

            double v_coeff_sin_x = (alpha * v0.x + omega_n * omega_n * d0.x) / omega_d;
            double v_coeff_sin_y = (alpha * v0.y + omega_n * omega_n * d0.y) / omega_d;

            vel.x = (float)(exp_term * (v0.x * cos_term - v_coeff_sin_x * sin_term));
            vel.y = (float)(exp_term * (v0.y * cos_term - v_coeff_sin_y * sin_term));
        } else {
            // -------------------------------------------------------------
            // Case 3: Overdamped (zeta > 1.0)
            // -------------------------------------------------------------
            double alpha = zeta * omega_n;
            double beta = omega_n * std::sqrt(zeta * zeta - 1.0);
            double exp_term = std::exp(-alpha * dt);
            double cosh_term = std::cosh(beta * dt);
            double sinh_term = std::sinh(beta * dt);

            double c_x = (v0.x + alpha * d0.x) / beta;
            double c_y = (v0.y + alpha * d0.y) / beta;

            pos.x = target_pos.x + (float)(exp_term * (d0.x * cosh_term + c_x * sinh_term));
            pos.y = target_pos.y + (float)(exp_term * (d0.y * cosh_term + c_y * sinh_term));

            double v_coeff_sinh_x = (alpha * v0.x + omega_n * omega_n * d0.x) / beta;
            double v_coeff_sinh_y = (alpha * v0.y + omega_n * omega_n * d0.y) / beta;

            vel.x = (float)(exp_term * (v0.x * cosh_term - v_coeff_sinh_x * sinh_term));
            vel.y = (float)(exp_term * (v0.y * cosh_term - v_coeff_sinh_y * sinh_term));
        }
    }
};

} // namespace Gaze
