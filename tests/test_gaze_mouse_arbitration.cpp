/**
 * @file test_gaze_mouse_arbitration.cpp
 * @brief Unit tests for multi-scale mouse stillness arbitration, anchor breakout, sliding-window velocity, and mutual exclusion.
 */

#include "doctest.h"
#include "mouse_stillness_arbitrator.hpp"
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdint>

using Point2D = Gaze::MouseStillnessArbitrator::Point2D;
using MouseStillnessArbitrator = Gaze::MouseStillnessArbitrator;


TEST_CASE("MouseStillnessArbitration: Instant Breakout via Anchor Bubble") {
    MouseStillnessArbitrator arb;
    uint64_t t = 1000000;
    arb.update(0.016, t, Point2D(500, 500), false);
    CHECK(arb.is_mouse_still() == true);
    CHECK(arb.stillness_timer >= 1.5f);

    // Micro-jitter inside bubble (1.0px < 3.0px)
    t += 16667;
    arb.update(0.016667, t, Point2D(501, 500), false);
    CHECK(arb.is_mouse_still() == true);

    // Step outside bubble (4.0px >= 3.0px) on a single frame
    t += 16667;
    arb.update(0.016667, t, Point2D(504, 500), false);
    
    // Instant breakout: frame 0 latency
    CHECK(arb.is_mouse_active() == true);
    CHECK(arb.is_mouse_still() == false);
    CHECK(arb.stillness_timer == doctest::Approx(0.0f));
}

TEST_CASE("MouseStillnessArbitration: Slow Creep Detection via Sliding Window") {
    MouseStillnessArbitrator arb;
    uint64_t t = 1000000;
    arb.update(0.016, t, Point2D(100, 100), false);
    CHECK(arb.is_mouse_still() == true);

    // Break stillness with an initial movement
    t += 16667;
    arb.update(0.016667, t, Point2D(110, 100), false);
    CHECK(arb.is_mouse_active() == true);

    // Simulate deliberate slow creep: 30 px/s at 100 Hz (0.30 px per 10ms frame)
    // A naive per-frame check (0.30 px < 1.5 px) would falsely settle here!
    // But 30 px/s over 200ms = 6.0 px displacement (velocity = 30 px/s >= 15 px/s).
    Point2D curr = Point2D(110, 100);
    for (int i = 0; i < 50; ++i) { // 500ms of continuous slow motion
        t += 10000; // 10ms
        curr.x += 0.30f; // 30 px/s
        arb.update(0.010, t, curr, false);

        // At each step, velocity in 200ms window maintains active state
        if (i >= 20) { // After 200ms window fills
            CHECK_MESSAGE(arb.is_mouse_active() == true, "Failed slow creep detection at step " << i);
            CHECK(arb.stillness_timer == doctest::Approx(0.0f));
        }
    }

    // Now stop mouse completely: allow 200ms for window velocity to flush, then 1.5s for stillness duration
    for (int i = 0; i < 180; ++i) { // 1.8s of no motion (0.2s flush + 1.6s stillness)
        t += 10000;
        arb.update(0.010, t, curr, false);
    }

    // Must now be still
    CHECK(arb.is_mouse_still() == true);
    CHECK(arb.stillness_timer >= 1.5f);
}

TEST_CASE("MouseStillnessArbitration: Sensor Noise Rejection") {
    MouseStillnessArbitrator arb;
    uint64_t t = 1000000;
    Point2D origin(300, 300);
    arb.update(0.016, t, origin, false);

    // High-DPI sensor jitter oscillating +/- 0.4px at 120 Hz
    for (int i = 0; i < 180; ++i) { // 1.5s total
        t += 8333; // ~120 Hz
        float jitter = (i % 2 == 0) ? 0.4f : -0.4f;
        arb.update(0.008333, t, Point2D(origin.x + jitter, origin.y), false);
    }

    // Jitter stayed within 3.0px bubble, velocity was near 0 net -> must remain still
    CHECK(arb.is_mouse_still() == true);
    CHECK(arb.stillness_timer >= 1.5f);
}

TEST_CASE("MouseStillnessArbitration: Click Breaks Stillness Immediately") {
    MouseStillnessArbitrator arb;
    uint64_t t = 1000000;
    arb.update(0.016, t, Point2D(200, 200), false);
    CHECK(arb.is_mouse_still() == true);

    // Mouse does not move at all, but left click occurs
    t += 16667;
    arb.update(0.016667, t, Point2D(200, 200), true); // mouse_clicked = true
    CHECK(arb.is_mouse_active() == true);
    CHECK(arb.stillness_timer == doctest::Approx(0.0f));
}

TEST_CASE("MouseStillnessArbitration: Dual Emulation Mutual Exclusion") {
    // Model the arbitration between G <- M and M <- G
    bool emulate_gaze_from_mouse = true;
    bool emulate_mouse_from_gaze = true;
    bool camera_face_tracked = true;

    MouseStillnessArbitrator arb;
    uint64_t t = 1000000;
    arb.update(0.016, t, Point2D(100, 100), false); // Initialized still

    auto evaluate_dispatch = [&](bool mouse_still, bool face_tracked) {
        bool emit_gaze_from_mouse = false;
        bool emit_mouse_from_gaze = false;

        // Level 0: Physical Mouse
        if (!mouse_still) {
            // Physical mouse is active:
            if (emulate_gaze_from_mouse) {
                emit_gaze_from_mouse = true;
            }
            // M <- G MUST be strictly suppressed during physical mouse activity
            emit_mouse_from_gaze = false;
        } else {
            // Physical mouse is still:
            emit_gaze_from_mouse = false;
            // M <- G can ONLY run if camera gaze is tracked (NOT from emulated gaze)
            if (emulate_mouse_from_gaze && face_tracked) {
                emit_mouse_from_gaze = true;
            }
        }
        return std::make_pair(emit_gaze_from_mouse, emit_mouse_from_gaze);
    };

    // State A: Mouse is moving, face is tracked
    t += 16667;
    arb.update(0.016667, t, Point2D(120, 100), false); // Moves 20px
    auto res_a = evaluate_dispatch(arb.is_mouse_still(), camera_face_tracked);
    CHECK(res_a.first == true);   // G <- M active
    CHECK(res_a.second == false); // M <- G strictly suppressed!

    // State B: Mouse is still for 1.8s, camera face tracked
    for (int i = 0; i < 180; ++i) {
        t += 10000;
        arb.update(0.010, t, Point2D(120, 100), false);
    }
    CHECK(arb.is_mouse_still() == true);
    auto res_b = evaluate_dispatch(arb.is_mouse_still(), camera_face_tracked);
    CHECK(res_b.first == false);  // G <- M released
    CHECK(res_b.second == true);  // M <- G active from camera gaze

    // State C: Mouse is still, but camera face is NOT tracked
    auto res_c = evaluate_dispatch(arb.is_mouse_still(), false);
    CHECK(res_c.first == false);
    CHECK(res_c.second == false); // M <- G stays dormant without camera face
}

TEST_CASE("MouseStillnessArbitration: Cold Boot and Tailing Off Settling") {
    MouseStillnessArbitrator arb;
    uint64_t t = 1000000;

    // 1. Cold boot at resting position (e.g. upper right corner 1400, 50)
    arb.update(0.016, t, Point2D(1400, 50), false);
    CHECK(arb.is_mouse_still() == true);
    CHECK(arb.has_user_interacted == false);

    // On cold boot, physical mouse MUST NOT steal gaze control under ANY circumstance:
    CHECK(arb.get_target_blend(true, true) == doctest::Approx(0.0f));  // Camera active
    CHECK(arb.get_target_blend(false, true) == doctest::Approx(0.0f)); // Camera inactive (offline dev)
    CHECK(arb.get_target_blend(true, false) == doctest::Approx(0.0f)); // Emulation off

    // 2. User moves physical mouse by 20px (nontrivial event breakout)
    t += 16667;
    arb.update(0.016667, t, Point2D(1420, 50), false);
    CHECK(arb.is_mouse_active() == true);
    CHECK(arb.has_user_interacted == true);
    CHECK(arb.get_target_blend(true, true) == doctest::Approx(1.0f));  // G <- M takes over

    // 3. User stops moving physical mouse (sliding window flushes moving sample over 200ms)
    for (int i = 0; i < 15; ++i) {
        t += 16667;
        arb.update(0.016667, t, Point2D(1420, 50), false);
    }
    CHECK(arb.state == MouseStillnessArbitrator::STATE_SETTLING);

    // During tailing off period (e.g. at 0.5s elapsed < 1.5s):
    for (int i = 0; i < 30; ++i) {
        t += 16667;
        arb.update(0.016667, t, Point2D(1420, 50), false);
    }
    CHECK(arb.state == MouseStillnessArbitrator::STATE_SETTLING);
    CHECK(arb.is_mouse_active() == true); // Active or settling suppresses gaze-to-mouse
    // Mouse has stopped moving: target_blend MUST be 0.0f (still mouse does not override gaze)
    CHECK(arb.get_target_blend(true, true) == doctest::Approx(0.0f));

    // 4. Tailing off period expires (stillness duration >= 1.5s)
    for (int i = 0; i < 70; ++i) {
        t += 16667;
        arb.update(0.016667, t, Point2D(1420, 50), false);
    }
    CHECK(arb.state == MouseStillnessArbitrator::STATE_STILL);
    CHECK(arb.is_mouse_still() == true);

    // When still: mouse does not override gaze
    CHECK(arb.get_target_blend(true, true) == doctest::Approx(0.0f));
    CHECK(arb.get_target_blend(false, true) == doctest::Approx(0.0f));
}
