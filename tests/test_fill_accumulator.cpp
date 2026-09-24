/**
 * @file test_fill_accumulator.cpp
 * @brief Unit tests for 1D potential FillAccumulator and signal separation bounds.
 */

#include "doctest.h"
#include "fill_accumulator.hpp"

using FillAccumulator = Gaze::FillAccumulator;

TEST_CASE("FillAccumulator: Linear Fill Rate and Completion") {
    FillAccumulator acc;
    acc.set_fill_sec(0.50);  // 0.5s to fill
    acc.set_drain_sec(0.25); // 0.25s to drain

    CHECK(acc.get_fill() == doctest::Approx(0.0));
    CHECK(acc.is_filled() == false);
    CHECK(acc.is_empty() == true);

    // Feed error <= 0.0 for 0.25 seconds (halfway)
    double dt = 0.05;
    for (int i = 0; i < 5; ++i) {
        acc.update(dt, 0.0);
    }

    CHECK(acc.get_fill() == doctest::Approx(0.50).epsilon(0.01));
    CHECK(acc.is_filled() == false);
    CHECK(acc.is_empty() == false);

    // Feed error <= 0.0 for another 0.25 seconds (reaches 1.0)
    for (int i = 0; i < 5; ++i) {
        acc.update(dt, 0.0);
    }

    CHECK(acc.get_fill() == doctest::Approx(1.0).epsilon(0.01));
    CHECK(acc.is_filled() == true);

    // Further on-target updates stay clamped at 1.0
    acc.update(0.10, 0.0);
    CHECK(acc.get_fill() == doctest::Approx(1.0));
}

TEST_CASE("FillAccumulator: Draining and Distance Scaling") {
    FillAccumulator acc;
    acc.set_fill_sec(0.50);
    acc.set_drain_sec(0.20);
    acc.set_characteristic_error(100.0);

    // Charge fully
    for (int i = 0; i < 10; ++i) {
        acc.update(0.05, 0.0);
    }
    CHECK(acc.is_filled() == true);

    // Feed off-target error = 100.0 (one unit outside)
    // dF/dt = -(1 + error / D0) / T_drain = -(1 + 1) / 0.20 = -10.0 / s
    // In 0.05s, should drain 0.50
    acc.update(0.05, 100.0);
    CHECK(acc.get_fill() <= 0.60);

    // Another 0.05s drains to 0.0
    acc.update(0.05, 100.0);
    CHECK(acc.get_fill() == doctest::Approx(0.0));
    CHECK(acc.is_empty() == true);

    // Further drain stays clamped at 0.0
    acc.update(0.05, 200.0);
    CHECK(acc.get_fill() == doctest::Approx(0.0));
    CHECK(acc.is_empty() == true);
}

TEST_CASE("FillAccumulator: Signal Separation Invariant (Delta >= 0.50)") {
    FillAccumulator acc;
    acc.set_fill_sec(0.40);
    acc.set_drain_sec(0.20);

    // Phase 1: On-target fixation for 0.35s
    for (int i = 0; i < 7; ++i) {
        acc.update(0.05, 0.0);
    }
    double on_target_fill = acc.get_fill();
    CHECK_MESSAGE(on_target_fill >= 0.70, "On-target fill must achieve >= 0.70 domain bound");

    // Phase 2: Saccade / lookaway (error = 300px) for 0.15s
    for (int i = 0; i < 3; ++i) {
        acc.update(0.05, 300.0);
    }
    double off_target_fill = acc.get_fill();
    CHECK_MESSAGE(off_target_fill <= 0.20, "Off-target fill must drop to <= 0.20 domain bound");

    // Signal Separation Guarantee: Delta >= 0.50
    double delta = on_target_fill - off_target_fill;
    CHECK_MESSAGE(delta >= 0.50, "Signal separation between on-target and off-target states must be >= 0.50");
}

TEST_CASE("FillAccumulator: 2nd-Order Momentum Preserves Fill During Micro-Saccade") {
    FillAccumulator acc;
    acc.set_fill_rate(1.6); // ~0.625s to fill
    acc.set_drain_rate(2.0);
    acc.set_fill_acceleration(32.0);
    acc.set_drain_acceleration(32.0);

    // Charge up over 0.4s (24 frames at 60 Hz)
    double dt = 1.0 / 60.0;
    for (int i = 0; i < 24; ++i) {
        acc.process_error_delta(0.0, dt);
    }
    double fill_before = acc.get_fill();
    CHECK(fill_before >= 0.40);
    CHECK(acc.get_velocity() > 0.0);

    // 1-frame micro-saccade (16.6ms with error = 50px)
    acc.process_error_delta(50.0, dt);
    // Momentum must prevent immediate draining during 1-frame micro-saccade
    CHECK(acc.get_fill() >= fill_before);
    CHECK(acc.get_velocity() > 0.0);

    // Gaze returns immediately on target
    acc.process_error_delta(0.0, dt);
    CHECK(acc.get_fill() > fill_before);

    // Sustained lookaway (0.8s) decelerates through 0 and drains completely to 0.0
    for (int i = 0; i < 48; ++i) {
        acc.process_error_delta(200.0, dt);
    }
    CHECK(acc.get_fill() == doctest::Approx(0.0));
    CHECK(acc.is_empty() == true);
    CHECK(acc.get_velocity() == doctest::Approx(0.0));
}

TEST_CASE("FillAccumulator: Fuzzy Distance Penalty with error_doubling_distance") {
    // Two accumulators with error_doubling_distance = 100.0
    FillAccumulator acc_near;
    acc_near.set_error_doubling_distance(100.0);
    acc_near.set_fill_rate(2.0);
    acc_near.set_drain_rate(2.0);
    acc_near.set_fill_acceleration(100.0);
    acc_near.set_drain_acceleration(100.0);

    FillAccumulator acc_far = acc_near;

    // Both charge to 1.0
    for (int i = 0; i < 30; ++i) {
        acc_near.process_dwell_delta(true, 0.02);
        acc_far.process_dwell_delta(true, 0.02);
    }
    CHECK(acc_near.is_filled() == true);
    CHECK(acc_far.is_filled() == true);

    // Discharge for 0.15s:
    // acc_near has near-miss tremor: error = 10px (penalty = 1 + 10/100 = 1.1x)
    // acc_far has intentional saccade: error = 300px (penalty = 1 + 300/100 = 4.0x)
    for (int i = 0; i < 8; ++i) {
        acc_near.process_error_delta(10.0, 0.02);
        acc_far.process_error_delta(300.0, 0.02);
    }

    // Far saccade must drain significantly faster than near-miss tremor
    CHECK(acc_near.get_fill() > acc_far.get_fill());
    CHECK(acc_near.get_fill() >= 0.50); // Near still has substantial fill
    CHECK(acc_far.get_fill() <= 0.20);  // Far has mostly drained
}

