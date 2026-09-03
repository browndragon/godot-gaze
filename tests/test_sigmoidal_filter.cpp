/**
 * @file test_sigmoidal_filter.cpp
 * @brief Unit tests for generic SigmoidalFilter
 */

#include "doctest.h"
#include "sigmoidal_filter.hpp"
#include <cmath>

using namespace Gaze;

TEST_CASE("SigmoidalFilter: Confidence-Scaled Tracking Convergence") {
    // Filter with base tau = 0.025s (25ms), neutral = 0.0
    SigmoidalFilter filter(0.0f, 0.060f, 0.250f, 0.025f, 0.01f);

    // Initial state
    CHECK(filter.get_value() == doctest::Approx(0.0f));

    // Test 1: Step input to 100.0 with high confidence (1.0) at 60 FPS (dt = 16.67ms)
    // alpha = 1 - exp(-0.01667 / 0.025) = 1 - exp(-0.6667) = 1 - 0.5134 = ~0.4866
    double t = 0.0;
    double dt = 1.0 / 60.0;

    float v1 = filter.update(true, 100.0f, 1.0f, t);
    CHECK(v1 == doctest::Approx(48.66f).epsilon(0.05f));

    // After 5 frames (~83ms), high confidence reaches ~97% of target
    for (int i = 0; i < 4; ++i) {
        t += dt;
        v1 = filter.update(true, 100.0f, 1.0f, t);
    }
    CHECK(v1 > 96.0f);

    // Test 2: Low confidence (0.25) converges slower
    SigmoidalFilter filter_low(0.0f, 0.060f, 0.250f, 0.025f, 0.01f);
    // tau_eff = 0.025 / 0.25 = 0.10s (100ms)
    // alpha = 1 - exp(-0.01667 / 0.10) = 1 - exp(-0.1667) = ~0.1535
    t = 0.0;
    float v_low = filter_low.update(true, 100.0f, 0.25f, t);
    CHECK(v_low == doctest::Approx(15.35f).epsilon(0.05f));

    // After 5 frames with low confidence, response is ~55%, noticeably smoother than high confidence
    for (int i = 0; i < 4; ++i) {
        t += dt;
        v_low = filter_low.update(true, 100.0f, 0.25f, t);
    }
    CHECK(v_low < 65.0f);
    CHECK(v_low > 50.0f);
}

TEST_CASE("SigmoidalFilter: Grace Period Hold & Smoothstep S-Curve Decay") {
    // Hold window = 60ms, Decay window = 250ms, neutral = 0.0
    SigmoidalFilter filter(0.0f, 0.060f, 0.250f, 0.025f, 0.01f);

    // Warm up filter to 50.0
    double t = 1.0;
    double dt = 1.0 / 60.0; // ~16.67ms
    for (int i = 0; i < 20; ++i) {
        t += dt;
        filter.update(true, 50.0f, 1.0f, t);
    }
    REQUIRE(filter.get_value() == doctest::Approx(50.0f).epsilon(0.01f));

    // Step 1: Single-frame drop (t = +16.7ms into dropout)
    t += dt;
    float val_1frame = filter.update(false, 0.0f, 0.0f, t);
    CHECK(filter.is_holding() == true);
    CHECK(filter.is_decaying() == false);
    // MUST hold 100% of the value during the hold window
    CHECK(val_1frame == doctest::Approx(50.0f));

    // Step 2: 3-frame drop (t = +50ms into dropout, still within 60ms hold window)
    t += dt;
    filter.update(false, 0.0f, 0.0f, t);
    t += dt;
    float val_3frame = filter.update(false, 0.0f, 0.0f, t);
    CHECK(filter.is_holding() == true);
    CHECK(val_3frame == doctest::Approx(50.0f));

    // Step 3: Transition past hold window into smoothstep decay (e.g., t = +155ms into dropout)
    // t_norm = (0.155 - 0.060) / (0.250 - 0.060) = 0.095 / 0.190 = 0.50
    // S(0.5) = 3*(0.25) - 2*(0.125) = 0.75 - 0.25 = 0.50
    // decay_weight = 1.0 - 0.50 = 0.50
    // expected value = 50.0 * 0.50 = 25.0
    t = 1.0 + (20 * dt) + 0.155;
    float val_mid = filter.update(false, 0.0f, 0.0f, t);
    CHECK(filter.is_holding() == false);
    CHECK(filter.is_decaying() == true);
    CHECK(val_mid == doctest::Approx(25.0f).epsilon(0.05f));

    // Step 4: Beyond decay window (t >= 250ms into dropout)
    t = 1.0 + (20 * dt) + 0.300;
    float val_end = filter.update(false, 0.0f, 0.0f, t);
    CHECK(filter.is_holding() == false);
    CHECK(val_end == doctest::Approx(0.0f));
}

TEST_CASE("SigmoidalFilter: Timestamp Jumps & Discontinuity Handling") {
    SigmoidalFilter filter(0.0f, 0.060f, 0.250f, 0.025f, 0.01f);

    // Warm up
    filter.update(true, 40.0f, 1.0f, 1.0);
    filter.update(true, 40.0f, 1.0f, 1.05);
    CHECK(filter.get_value() > 30.0f);

    // Sudden 2.0s backgrounding pause while face is lost
    float val_paused = filter.update(false, 0.0f, 0.0f, 3.05);
    CHECK(val_paused == doctest::Approx(0.0f));
    CHECK_FALSE(std::isnan(val_paused));
    CHECK_FALSE(std::isinf(val_paused));

    // Reset clears state cleanly
    filter.update(true, 60.0f, 1.0f, 4.0);
    CHECK(filter.get_value() > 20.0f);
    filter.reset(0.0f);
    CHECK(filter.get_value() == doctest::Approx(0.0f));
    CHECK(filter.get_time_lost_s() == doctest::Approx(0.0));
}
