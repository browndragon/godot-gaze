/**
 * @file test_damped_spring.cpp
 * @brief Unit tests for 2nd-order DampedSpring physics and frame-rate invariance.
 */

#include "doctest.h"
#include "damped_spring.hpp"
#include <cmath>

using DampedSpring = Gaze::DampedSpring;
using Point2D = Gaze::DampedSpring::Point2D;

TEST_CASE("DampedSpring: Critical Damping Step Response and Zero Overshoot") {
    DampedSpring spring;
    spring.set_response_sec(0.30);
    spring.set_damping_ratio(1.0); // Critically damped

    Point2D initial_pos(0.0f, 0.0f);
    Point2D target_pos(100.0f, 0.0f);
    spring.reset(initial_pos);

    // Simulate 30 Hz step response for 0.6 seconds (2 * response_sec)
    double dt = 1.0 / 30.0;
    double time = 0.0;

    double pos_at_half_resp = 0.0;
    double pos_at_resp = 0.0;
    double max_pos = 0.0;

    while (time <= 0.60) {
        time += dt;
        spring.update(dt, target_pos);
        Point2D current = spring.get_position();

        if (current.x > max_pos) {
            max_pos = current.x;
        }

        if (std::abs(time - 0.15) < 0.02 && pos_at_half_resp == 0.0) {
            pos_at_half_resp = current.x;
        }
        if (std::abs(time - 0.30) < 0.02 && pos_at_resp == 0.0) {
            pos_at_resp = current.x;
        }
    }

    // Mathematical Invariant 1: At t = response_sec, should have covered >= 85% of distance
    CHECK(pos_at_resp >= 85.0);
    CHECK(pos_at_resp <= 95.0);

    // Mathematical Invariant 2: At t = 0.5 * response_sec, should have covered ~55-65%
    CHECK(pos_at_half_resp >= 50.0);
    CHECK(pos_at_half_resp <= 70.0);

    // Mathematical Invariant 3: Strictly ZERO overshoot under critical damping (max_pos <= target)
    CHECK(max_pos <= 100.001);
}

TEST_CASE("DampedSpring: Underdamped Overshoot at zeta = 0.5") {
    DampedSpring spring;
    spring.set_response_sec(0.30);
    spring.set_damping_ratio(0.5); // Underdamped

    Point2D initial_pos(0.0f, 0.0f);
    Point2D target_pos(100.0f, 0.0f);
    spring.reset(initial_pos);

    double dt = 1.0 / 60.0;
    double max_pos = 0.0;

    for (int i = 0; i < 60; ++i) { // 1.0s
        spring.update(dt, target_pos);
        Point2D current = spring.get_position();
        if (current.x > max_pos) {
            max_pos = current.x;
        }
    }

    // Underdamped at zeta = 0.5 MUST overshoot 100.0 by approximately 12% - 20%
    CHECK(max_pos >= 112.0);
    CHECK(max_pos <= 122.0);
}

TEST_CASE("DampedSpring: Frame-Rate Invariance (30 Hz vs 120 Hz)") {
    DampedSpring spring_30hz;
    spring_30hz.set_response_sec(0.30);
    spring_30hz.set_damping_ratio(1.0);
    spring_30hz.reset(Point2D(0, 0));

    DampedSpring spring_120hz;
    spring_120hz.set_response_sec(0.30);
    spring_120hz.set_damping_ratio(1.0);
    spring_120hz.reset(Point2D(0, 0));

    Point2D target(200.0f, -150.0f);

    // Simulate 0.3 seconds at 30 Hz (9 steps of 0.0333s)
    double dt_30 = 0.30 / 9.0;
    for (int i = 0; i < 9; ++i) {
        spring_30hz.update(dt_30, target);
    }

    // Simulate 0.3 seconds at 120 Hz (36 steps of 0.00833s)
    double dt_120 = 0.30 / 36.0;
    for (int i = 0; i < 36; ++i) {
        spring_120hz.update(dt_120, target);
    }

    // Closed-form analytic integration must produce nearly identical endpoints regardless of step size
    Point2D pos_30 = spring_30hz.get_position();
    Point2D pos_120 = spring_120hz.get_position();

    CHECK(pos_30.x == doctest::Approx(pos_120.x).epsilon(0.01));
    CHECK(pos_30.y == doctest::Approx(pos_120.y).epsilon(0.01));
}
