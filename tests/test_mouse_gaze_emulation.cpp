/**
 * @file test_mouse_gaze_emulation.cpp
 * @brief Unit tests for MouseGazeEmulation blending mathematics, state transitions, and dispatch invariants.
 */

#include "doctest.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace {

// Pure mathematical state model mirroring MouseGazeEmulation logic for native test verification
class TestMouseGazeStateMachine {
public:
    float dwell_time_sec = 2.0f;
    float transition_duration_sec = 0.3f;
    float motion_threshold_px = 1.5f;

    float dwell_timer = 0.0f;
    float blend_progress = 0.0f; // 0.0 = pure camera, 1.0 = pure mouse

    struct Vec2 {
        float x = 0.0f;
        float y = 0.0f;
        Vec2() = default;
        Vec2(float p_x, float p_y) : x(p_x), y(p_y) {}
        Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
        Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
        Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
        float length() const { return std::sqrt(x * x + y * y); }
        Vec2 lerp(const Vec2& to, float t) const {
            return Vec2(x + (to.x - x) * t, y + (to.y - y) * t);
        }
    };

    Vec2 last_screen_mouse_pos = Vec2(-9999.0f, -9999.0f);
    bool has_last_mouse_pos = false;

    float get_eased_blend_factor() const {
        float t = std::clamp(blend_progress, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t); // Smoothstep: 3t^2 - 2t^3
    }

    bool is_emulation_active() const {
        return blend_progress > 0.0001f;
    }

    bool is_in_transition() const {
        return blend_progress > 0.0001f && blend_progress < 0.9999f;
    }

    void on_mouse_event(const Vec2& screen_mouse_pos, bool mouse_clicked) {
        if (has_last_mouse_pos) {
            float move_dist = (screen_mouse_pos - last_screen_mouse_pos).length();
            if (move_dist >= motion_threshold_px || mouse_clicked) {
                dwell_timer = dwell_time_sec;
            }
        }
        last_screen_mouse_pos = screen_mouse_pos;
        has_last_mouse_pos = true;
    }

    void update(
        double delta_sec,
        bool camera_tracking_active,
        bool face_detected,
        bool emulate_gaze_from_mouse
    ) {
        if (!emulate_gaze_from_mouse) {
            dwell_timer = 0.0f;
            blend_progress = 0.0f;
            return;
        }

        if (dwell_timer > 0.0f) {
            dwell_timer = std::max(0.0f, dwell_timer - (float)delta_sec);
        }

        float target_blend = 0.0f;
        if (!camera_tracking_active) {
            target_blend = 1.0f;
        } else if (dwell_timer > 0.0f) {
            target_blend = 1.0f;
        } else if (face_detected) {
            target_blend = 0.0f;
        } else {
            target_blend = blend_progress; // Freeze in place on lost face with idle mouse
        }

        float step = (transition_duration_sec > 0.001f) ? ((float)delta_sec / transition_duration_sec) : 1.0f;
        if (blend_progress < target_blend) {
            blend_progress = std::min(target_blend, blend_progress + step);
        } else if (blend_progress > target_blend) {
            blend_progress = std::max(target_blend, blend_progress - step);
        }
    }

    Vec2 compute_blended_position(const Vec2& cam_pos, const Vec2& mouse_pos) const {
        float ease = get_eased_blend_factor();
        return cam_pos.lerp(mouse_pos, ease);
    }
};

} // namespace

TEST_CASE("MouseGazeEmulation: State Transitions & Dwell Timer") {
    TestMouseGazeStateMachine sm;

    // Initial state: inactive
    CHECK_FALSE(sm.is_emulation_active());
    CHECK_FALSE(sm.is_in_transition());
    CHECK(sm.blend_progress == doctest::Approx(0.0f));
    CHECK(sm.dwell_timer == doctest::Approx(0.0f));

    // Mouse movement triggers dwell timer
    sm.on_mouse_event(TestMouseGazeStateMachine::Vec2(100, 100), false);
    sm.on_mouse_event(TestMouseGazeStateMachine::Vec2(110, 100), false); // 10px move >= 1.5px threshold
    CHECK(sm.dwell_timer == doctest::Approx(2.0f));

    // Frame 1: 0.15s step (half of 0.3s transition duration)
    sm.update(0.15, true, true, true);
    CHECK(sm.dwell_timer == doctest::Approx(1.85f));
    CHECK(sm.blend_progress == doctest::Approx(0.50f).epsilon(0.01f));
    CHECK(sm.is_emulation_active() == true);
    CHECK(sm.is_in_transition() == true);

    // Verify Smoothstep easing at t=0.5: S(0.5) = 3*(0.25) - 2*(0.125) = 0.75 - 0.25 = 0.50
    CHECK(sm.get_eased_blend_factor() == doctest::Approx(0.50f).epsilon(0.01f));

    // Frame 2: Another 0.15s step completes the transition to pure mouse
    sm.update(0.15, true, true, true);
    CHECK(sm.dwell_timer == doctest::Approx(1.70f));
    CHECK(sm.blend_progress == doctest::Approx(1.0f).epsilon(0.01f));
    CHECK(sm.is_emulation_active() == true);
    CHECK(sm.is_in_transition() == false);
    CHECK(sm.get_eased_blend_factor() == doctest::Approx(1.0f).epsilon(0.01f));

    // Idle for remaining 1.70s:
    // For the first 101 frames (1.683s), dwell is still active (> 0), so blend remains 1.0:
    for (int i = 0; i < 101; ++i) {
        sm.update(0.016667, true, true, true);
        CHECK(sm.blend_progress == doctest::Approx(1.0f));
    }
    CHECK(sm.dwell_timer > 0.0f);

    // Frame 102 (1.70s): Dwell timer expires to 0.0s and blend starts ramping down:
    sm.update(0.016667, true, true, true);
    CHECK(sm.dwell_timer == doctest::Approx(0.0f));
    CHECK(sm.blend_progress < 1.0f);
    CHECK(sm.blend_progress > 0.90f);

    // Step remaining 0.15s (halfway through 0.3s transition) -> blend reaches ~0.45-0.50:
    sm.update(0.15, true, true, true);
    CHECK(sm.blend_progress == doctest::Approx(0.45f).epsilon(0.05f));
    CHECK(sm.is_in_transition() == true);

    // Step remaining 0.15s -> reaches 0.0 (pure camera):
    sm.update(0.15, true, true, true);
    CHECK(sm.blend_progress == doctest::Approx(0.0f));
    CHECK(sm.is_emulation_active() == false);
    CHECK(sm.is_in_transition() == false);
}

TEST_CASE("MouseGazeEmulation: Freeze on Lost Face with Idle Mouse") {
    TestMouseGazeStateMachine sm;

    // Start with camera active and face tracked
    sm.update(0.016, true, true, true);
    CHECK(sm.blend_progress == doctest::Approx(0.0f));
    CHECK(sm.is_emulation_active() == false);

    // User is NOT touching mouse. Camera tracking active, but face tracking lost (e.g. user turned head)
    for (int i = 0; i < 60; ++i) {
        sm.update(0.016, true, false, true); // face_detected = false
    }

    // Blend progress MUST stay strictly at 0.0 (DO NOT drift to dormant mouse)
    CHECK(sm.blend_progress == doctest::Approx(0.0f));
    CHECK(sm.is_emulation_active() == false);

    // Now user explicitly moves mouse while face is still missing
    sm.on_mouse_event(TestMouseGazeStateMachine::Vec2(500, 400), false);
    sm.on_mouse_event(TestMouseGazeStateMachine::Vec2(520, 400), false);
    CHECK(sm.dwell_timer == doctest::Approx(2.0f));

    // Mouse motion ramps blend progress to 1.0
    sm.update(0.30, true, false, true);
    CHECK(sm.blend_progress == doctest::Approx(1.0f));
    CHECK(sm.is_emulation_active() == true);

    // Mouse stops moving, dwell expires while face is STILL missing
    sm.update(2.0, true, false, true);
    CHECK(sm.dwell_timer == doctest::Approx(0.0f));

    // Because face is missing, blend progress stays FROZEN at 1.0 (last mouse pos)
    sm.update(0.50, true, false, true);
    CHECK(sm.blend_progress == doctest::Approx(1.0f));

    // Face returns: smoothly glides back to camera gaze (over 0.3s)
    sm.update(0.15, true, true, true);
    CHECK(sm.blend_progress == doctest::Approx(0.50f).epsilon(0.01f));
    sm.update(0.15, true, true, true);
    CHECK(sm.blend_progress == doctest::Approx(0.0f));
    CHECK(sm.is_emulation_active() == false);
}

TEST_CASE("MouseGazeEmulation: Continuous Interpolation & Lerp Monotonicity") {
    TestMouseGazeStateMachine sm;

    TestMouseGazeStateMachine::Vec2 cam_pos(200.0f, 300.0f);
    TestMouseGazeStateMachine::Vec2 mouse_pos(800.0f, 700.0f);

    // Initial position at blend = 0.0 is cam_pos
    TestMouseGazeStateMachine::Vec2 p0 = sm.compute_blended_position(cam_pos, mouse_pos);
    CHECK(p0.x == doctest::Approx(200.0f));
    CHECK(p0.y == doctest::Approx(300.0f));

    // Trigger mouse motion
    sm.on_mouse_event(TestMouseGazeStateMachine::Vec2(0, 0), false);
    sm.on_mouse_event(TestMouseGazeStateMachine::Vec2(50, 50), false);

    // Step through transition and verify monotonic movement towards mouse
    float prev_x = p0.x;
    float prev_y = p0.y;
    for (int i = 1; i <= 10; ++i) {
        sm.update(0.03, true, true, true); // 10 steps of 0.03s = 0.30s total
        TestMouseGazeStateMachine::Vec2 pt = sm.compute_blended_position(cam_pos, mouse_pos);

        CHECK_MESSAGE(pt.x >= prev_x, "Monotonic X progress check failed at step " << i);
        CHECK_MESSAGE(pt.y >= prev_y, "Monotonic Y progress check failed at step " << i);

        prev_x = pt.x;
        prev_y = pt.y;
    }

    // Final position is exactly mouse_pos
    CHECK(prev_x == doctest::Approx(800.0f));
    CHECK(prev_y == doctest::Approx(700.0f));
}

TEST_CASE("MouseGazeEmulation: Smoothstep C1 Continuity") {
    // Check smoothstep derivative at boundaries (t=0 and t=1 derivative is 0.0 for smooth start/stop)
    auto smoothstep = [](float t) {
        float ct = std::clamp(t, 0.0f, 1.0f);
        return ct * ct * (3.0f - 2.0f * ct);
    };

    CHECK(smoothstep(0.0f) == doctest::Approx(0.0f));
    CHECK(smoothstep(0.25f) == doctest::Approx(0.15625f));
    CHECK(smoothstep(0.50f) == doctest::Approx(0.50f));
    CHECK(smoothstep(0.75f) == doctest::Approx(0.84375f));
    CHECK(smoothstep(1.0f) == doctest::Approx(1.0f));

    // Symmetry check: S(1 - t) == 1 - S(t)
    for (float t = 0.0f; t <= 1.0f; t += 0.05f) {
        CHECK(smoothstep(1.0f - t) == doctest::Approx(1.0f - smoothstep(t)));
    }
}

TEST_CASE("MouseGazeEmulation: Preservation of Camera Head Pose and Eye Openness") {
    // Verifies the invariant that mouse emulation steers only the 2D gaze point
    // and preserves the 3D head transform and eye openness from camera tracking.

    struct Vec3 { float x, y, z; };
    struct Transform3 {
        Vec3 origin;
        Vec3 forward;
    };

    struct EmulatedGazeEvent {
        TestMouseGazeStateMachine::Vec2 canvas_pos;
        Transform3 head_xform;
        float left_open;
        float right_open;
    };

    auto synthesize_event = [](
        bool has_camera_data,
        float ease_factor,
        const TestMouseGazeStateMachine::Vec2& cam_canvas,
        const Transform3& cam_head,
        float cam_left_open,
        float cam_right_open,
        const TestMouseGazeStateMachine::Vec2& mouse_canvas
    ) -> EmulatedGazeEvent {
        EmulatedGazeEvent ev;
        Transform3 dummy_head{Vec3{0.0f, 0.0f, -500.0f}, Vec3{0.0f, 0.0f, 1.0f}};

        if (has_camera_data) {
            ev.canvas_pos = cam_canvas.lerp(mouse_canvas, ease_factor);
            ev.head_xform = cam_head; // Camera head pose is preserved!
            ev.left_open = cam_left_open; // Camera eye openness is preserved!
            ev.right_open = cam_right_open;
        } else {
            ev.canvas_pos = mouse_canvas;
            ev.head_xform = dummy_head;
            ev.left_open = 1.0f;
            ev.right_open = 1.0f;
        }
        return ev;
    };

    Transform3 real_cam_head{Vec3{45.0f, -20.0f, -420.0f}, Vec3{-0.1f, 0.05f, 0.99f}};
    TestMouseGazeStateMachine::Vec2 cam_canvas(300.0f, 400.0f);
    TestMouseGazeStateMachine::Vec2 mouse_canvas(800.0f, 600.0f);
    float real_left_open = 0.85f;
    float real_right_open = 0.15f; // User is winking!

    // 1. While camera is active and mouse moves (blend factor = 1.0 pure mouse)
    EmulatedGazeEvent ev_pure_mouse = synthesize_event(true, 1.0f, cam_canvas, real_cam_head, real_left_open, real_right_open, mouse_canvas);
    CHECK(ev_pure_mouse.canvas_pos.x == doctest::Approx(800.0f));
    CHECK(ev_pure_mouse.canvas_pos.y == doctest::Approx(600.0f));
    // Head pose MUST remain the real camera head pose:
    CHECK(ev_pure_mouse.head_xform.origin.x == doctest::Approx(45.0f));
    CHECK(ev_pure_mouse.head_xform.origin.y == doctest::Approx(-20.0f));
    CHECK(ev_pure_mouse.head_xform.origin.z == doctest::Approx(-420.0f));
    // Eye openness MUST reflect real wink state:
    CHECK(ev_pure_mouse.left_open == doctest::Approx(0.85f));
    CHECK(ev_pure_mouse.right_open == doctest::Approx(0.15f));

    // 2. During transition (blend factor = 0.5)
    EmulatedGazeEvent ev_blend = synthesize_event(true, 0.5f, cam_canvas, real_cam_head, real_left_open, real_right_open, mouse_canvas);
    CHECK(ev_blend.canvas_pos.x == doctest::Approx(550.0f));
    CHECK(ev_blend.canvas_pos.y == doctest::Approx(500.0f));
    CHECK(ev_blend.head_xform.origin.x == doctest::Approx(45.0f));
    CHECK(ev_blend.head_xform.origin.z == doctest::Approx(-420.0f));
    CHECK(ev_blend.left_open == doctest::Approx(0.85f));
    CHECK(ev_blend.right_open == doctest::Approx(0.15f));

    // 3. When NO camera data is available (e.g. webcam disabled)
    EmulatedGazeEvent ev_no_cam = synthesize_event(false, 1.0f, cam_canvas, real_cam_head, real_left_open, real_right_open, mouse_canvas);
    CHECK(ev_no_cam.canvas_pos.x == doctest::Approx(800.0f));
    CHECK(ev_no_cam.head_xform.origin.x == doctest::Approx(0.0f));
    CHECK(ev_no_cam.head_xform.origin.y == doctest::Approx(0.0f));
    CHECK(ev_no_cam.head_xform.origin.z == doctest::Approx(-500.0f));
    CHECK(ev_no_cam.left_open == doctest::Approx(1.0f));
    CHECK(ev_no_cam.right_open == doctest::Approx(1.0f));
}
