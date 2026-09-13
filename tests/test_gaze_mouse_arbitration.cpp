/**
 * @file test_gaze_mouse_arbitration.cpp
 * @brief Unit tests for multi-scale mouse stillness arbitration, anchor breakout, sliding-window velocity, and mutual exclusion.
 */

#include "doctest.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdint>

namespace {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
    Vec2() = default;
    Vec2(float p_x, float p_y) : x(p_x), y(p_y) {}
    Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    float length() const { return std::sqrt(x * x + y * y); }
};

struct MouseRingSample {
    uint64_t timestamp_usec = 0;
    Vec2 pos;
};

// State machine implementing the two-tiered stillness model
class TestMouseStillnessArbitrator {
public:
    enum State {
        STATE_ACTIVE,    // User is moving/clicking the physical mouse
        STATE_SETTLING,  // Mouse stopped moving, accumulating stillness time
        STATE_STILL      // Mouse has been still >= stillness_duration_sec, gaze has control
    };

    State state = STATE_STILL;

    static constexpr size_t RING_CAPACITY = 16;
    MouseRingSample ring_buffer[RING_CAPACITY];
    size_t ring_head = 0;
    size_t ring_count = 0;

    Vec2 anchor_pos = Vec2(-9999.0f, -9999.0f);
    bool has_anchor = false;

    float anchor_bubble_radius_px = 3.0f;
    float window_velocity_threshold_px_s = 15.0f;
    uint64_t window_duration_usec = 200000; // 200ms

    float stillness_timer = 0.0f;
    float stillness_duration_sec = 1.5f;

    bool is_mouse_still() const {
        return state == STATE_STILL;
    }

    bool is_mouse_active() const {
        return state != STATE_STILL;
    }

    void record_sample(uint64_t timestamp_usec, const Vec2& pos) {
        ring_buffer[ring_head] = { timestamp_usec, pos };
        ring_head = (ring_head + 1) % RING_CAPACITY;
        if (ring_count < RING_CAPACITY) {
            ring_count++;
        }
    }

    float compute_window_velocity(uint64_t current_time_usec, const Vec2& current_pos) const {
        if (ring_count < 2) return 0.0f;

        const MouseRingSample* oldest = nullptr;
        for (size_t i = 0; i < ring_count; ++i) {
            size_t idx = (ring_head + RING_CAPACITY - 1 - i) % RING_CAPACITY;
            const MouseRingSample& s = ring_buffer[idx];
            if (current_time_usec >= s.timestamp_usec && (current_time_usec - s.timestamp_usec) <= window_duration_usec) {
                oldest = &s;
            } else if (oldest != nullptr) {
                break;
            }
        }

        if (!oldest || oldest->timestamp_usec == current_time_usec) {
            return 0.0f;
        }

        float dt_sec = (float)(current_time_usec - oldest->timestamp_usec) / 1000000.0f;
        if (dt_sec < 0.001f) return 0.0f;

        float dist = (current_pos - oldest->pos).length();
        return dist / dt_sec;
    }

    bool has_user_interacted = false;

    float get_target_blend(bool camera_tracking_active, bool emulate_gaze_from_mouse) const {
        if (!emulate_gaze_from_mouse) return 0.0f;
        if (!has_user_interacted) {
            return 0.0f;
        }
        if (state != STATE_STILL) {
            return 1.0f;
        }
        if (camera_tracking_active) {
            return 0.0f;
        }
        return 1.0f;
    }

    void update(
        double delta_sec,
        uint64_t timestamp_usec,
        const Vec2& screen_mouse_pos,
        bool mouse_clicked
    ) {
        if (!has_anchor) {
            anchor_pos = screen_mouse_pos;
            has_anchor = true;
            record_sample(timestamp_usec, screen_mouse_pos);
            state = STATE_STILL;
            stillness_timer = stillness_duration_sec;
            return;
        }

        record_sample(timestamp_usec, screen_mouse_pos);
        float dist_from_anchor = (screen_mouse_pos - anchor_pos).length();
        float v_window = compute_window_velocity(timestamp_usec, screen_mouse_pos);

        if (state == STATE_STILL || state == STATE_SETTLING) {
            // In STILL or SETTLING:
            // Check if user broke out of the anchor bubble or clicked a button:
            if (mouse_clicked || dist_from_anchor >= anchor_bubble_radius_px) {
                has_user_interacted = true;
                state = STATE_ACTIVE;
                stillness_timer = 0.0f;
                anchor_pos = screen_mouse_pos;
            } else {
                // Remained within bubble
                stillness_timer += (float)delta_sec;
                if (stillness_timer >= stillness_duration_sec) {
                    state = STATE_STILL;
                }
            }
        } else {
            // In STATE_ACTIVE:
            // Check if user stopped moving (velocity below threshold and no clicks):
            if (!mouse_clicked && v_window < window_velocity_threshold_px_s) {
                // User has settled!
                state = STATE_SETTLING;
                anchor_pos = screen_mouse_pos;
                stillness_timer = (float)delta_sec;
                if (stillness_timer >= stillness_duration_sec) {
                    state = STATE_STILL;
                }
            } else {
                // Still actively moving
                has_user_interacted = true;
                anchor_pos = screen_mouse_pos;
                stillness_timer = 0.0f;
            }
        }
    }
};

} // namespace

TEST_CASE("MouseStillnessArbitration: Instant Breakout via Anchor Bubble") {
    TestMouseStillnessArbitrator arb;
    uint64_t t = 1000000;
    arb.update(0.016, t, Vec2(500, 500), false);
    CHECK(arb.is_mouse_still() == true);
    CHECK(arb.stillness_timer >= 1.5f);

    // Micro-jitter inside bubble (1.0px < 3.0px)
    t += 16667;
    arb.update(0.016667, t, Vec2(501, 500), false);
    CHECK(arb.is_mouse_still() == true);

    // Step outside bubble (4.0px >= 3.0px) on a single frame
    t += 16667;
    arb.update(0.016667, t, Vec2(504, 500), false);
    
    // Instant breakout: frame 0 latency
    CHECK(arb.is_mouse_active() == true);
    CHECK(arb.is_mouse_still() == false);
    CHECK(arb.stillness_timer == doctest::Approx(0.0f));
}

TEST_CASE("MouseStillnessArbitration: Slow Creep Detection via Sliding Window") {
    TestMouseStillnessArbitrator arb;
    uint64_t t = 1000000;
    arb.update(0.016, t, Vec2(100, 100), false);
    CHECK(arb.is_mouse_still() == true);

    // Break stillness with an initial movement
    t += 16667;
    arb.update(0.016667, t, Vec2(110, 100), false);
    CHECK(arb.is_mouse_active() == true);

    // Simulate deliberate slow creep: 30 px/s at 100 Hz (0.30 px per 10ms frame)
    // A naive per-frame check (0.30 px < 1.5 px) would falsely settle here!
    // But 30 px/s over 200ms = 6.0 px displacement (velocity = 30 px/s >= 15 px/s).
    Vec2 curr = Vec2(110, 100);
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
    TestMouseStillnessArbitrator arb;
    uint64_t t = 1000000;
    Vec2 origin(300, 300);
    arb.update(0.016, t, origin, false);

    // High-DPI sensor jitter oscillating +/- 0.4px at 120 Hz
    for (int i = 0; i < 180; ++i) { // 1.5s total
        t += 8333; // ~120 Hz
        float jitter = (i % 2 == 0) ? 0.4f : -0.4f;
        arb.update(0.008333, t, Vec2(origin.x + jitter, origin.y), false);
    }

    // Jitter stayed within 3.0px bubble, velocity was near 0 net -> must remain still
    CHECK(arb.is_mouse_still() == true);
    CHECK(arb.stillness_timer >= 1.5f);
}

TEST_CASE("MouseStillnessArbitration: Click Breaks Stillness Immediately") {
    TestMouseStillnessArbitrator arb;
    uint64_t t = 1000000;
    arb.update(0.016, t, Vec2(200, 200), false);
    CHECK(arb.is_mouse_still() == true);

    // Mouse does not move at all, but left click occurs
    t += 16667;
    arb.update(0.016667, t, Vec2(200, 200), true); // mouse_clicked = true
    CHECK(arb.is_mouse_active() == true);
    CHECK(arb.stillness_timer == doctest::Approx(0.0f));
}

TEST_CASE("MouseStillnessArbitration: Dual Emulation Mutual Exclusion") {
    // Model the arbitration between G <- M and M <- G
    bool emulate_gaze_from_mouse = true;
    bool emulate_mouse_from_gaze = true;
    bool camera_face_tracked = true;

    TestMouseStillnessArbitrator arb;
    uint64_t t = 1000000;
    arb.update(0.016, t, Vec2(100, 100), false); // Initialized still

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
    arb.update(0.016667, t, Vec2(120, 100), false); // Moves 20px
    auto res_a = evaluate_dispatch(arb.is_mouse_still(), camera_face_tracked);
    CHECK(res_a.first == true);   // G <- M active
    CHECK(res_a.second == false); // M <- G strictly suppressed!

    // State B: Mouse is still for 1.8s, camera face tracked
    for (int i = 0; i < 180; ++i) {
        t += 10000;
        arb.update(0.010, t, Vec2(120, 100), false);
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
    TestMouseStillnessArbitrator arb;
    uint64_t t = 1000000;

    // 1. Cold boot at resting position (e.g. upper right corner 1400, 50)
    arb.update(0.016, t, Vec2(1400, 50), false);
    CHECK(arb.is_mouse_still() == true);
    CHECK(arb.has_user_interacted == false);

    // On cold boot, physical mouse MUST NOT steal gaze control under ANY circumstance:
    CHECK(arb.get_target_blend(true, true) == doctest::Approx(0.0f));  // Camera active
    CHECK(arb.get_target_blend(false, true) == doctest::Approx(0.0f)); // Camera inactive (offline dev)
    CHECK(arb.get_target_blend(true, false) == doctest::Approx(0.0f)); // Emulation off

    // 2. User moves physical mouse by 20px (nontrivial event breakout)
    t += 16667;
    arb.update(0.016667, t, Vec2(1420, 50), false);
    CHECK(arb.is_mouse_active() == true);
    CHECK(arb.has_user_interacted == true);
    CHECK(arb.get_target_blend(true, true) == doctest::Approx(1.0f));  // G <- M takes over

    // 3. User stops moving physical mouse (sliding window flushes moving sample over 200ms)
    for (int i = 0; i < 15; ++i) {
        t += 16667;
        arb.update(0.016667, t, Vec2(1420, 50), false);
    }
    CHECK(arb.state == TestMouseStillnessArbitrator::STATE_SETTLING);

    // During tailing off period (e.g. at 0.5s elapsed < 1.5s):
    for (int i = 0; i < 30; ++i) {
        t += 16667;
        arb.update(0.016667, t, Vec2(1420, 50), false);
    }
    CHECK(arb.state == TestMouseStillnessArbitrator::STATE_SETTLING);
    CHECK(arb.is_mouse_active() == true); // Active or settling suppresses gaze
    CHECK(arb.get_target_blend(true, true) == doctest::Approx(1.0f)); // Mouse continues holding gaze

    // 4. Tailing off period expires (stillness duration >= 1.5s)
    for (int i = 0; i < 70; ++i) {
        t += 16667;
        arb.update(0.016667, t, Vec2(1420, 50), false);
    }
    CHECK(arb.state == TestMouseStillnessArbitrator::STATE_STILL);
    CHECK(arb.is_mouse_still() == true);

    // When still:
    // With camera active: yields 100% back to camera gaze!
    CHECK(arb.get_target_blend(true, true) == doctest::Approx(0.0f));
    // Without camera (dev mode after interaction): holds last mouse position
    CHECK(arb.get_target_blend(false, true) == doctest::Approx(1.0f));
}
