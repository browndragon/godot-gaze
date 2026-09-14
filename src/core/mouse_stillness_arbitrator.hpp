/**
 * @file mouse_stillness_arbitrator.hpp
 * @brief Zero-dependency multi-scale mouse stillness arbitrator and dual-control coordinator.
 */

#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace Gaze {

class MouseStillnessArbitrator {
public:
    struct Point2D {
        float x = 0.0f;
        float y = 0.0f;
        Point2D() = default;
        Point2D(float p_x, float p_y) : x(p_x), y(p_y) {}
        Point2D operator-(const Point2D& o) const { return Point2D(x - o.x, y - o.y); }
        Point2D operator+(const Point2D& o) const { return Point2D(x + o.x, y + o.y); }
        Point2D operator*(float s) const { return Point2D(x * s, y * s); }
        float length() const { return std::sqrt(x * x + y * y); }
    };

    enum State {
        STATE_ACTIVE,    // User is moving or clicking the physical mouse
        STATE_SETTLING,  // Physical mouse stopped moving; settling timer running
        STATE_STILL      // Mouse has been still >= stillness_duration_sec; gaze has control
    };

    State state = STATE_STILL;

    struct Sample {
        uint64_t timestamp_usec = 0;
        Point2D pos;
    };

    static constexpr size_t RING_CAPACITY = 16;
    Sample ring_buffer[RING_CAPACITY];
    size_t ring_head = 0;
    size_t ring_count = 0;

    Point2D anchor_pos = Point2D(-9999.0f, -9999.0f);
    bool has_anchor = false;

    float anchor_bubble_radius_px = 3.0f;
    float window_velocity_threshold_px_s = 15.0f;
    uint64_t window_duration_usec = 200000; // 200ms

    float stillness_timer = 0.0f;
    float stillness_duration_sec = 1.5f;
    bool has_user_interacted = false;

    MouseStillnessArbitrator() {
        reset();
    }

    void reset() {
        ring_head = 0;
        ring_count = 0;
        has_anchor = false;
        anchor_pos = Point2D(-9999.0f, -9999.0f);
        state = STATE_STILL;
        stillness_timer = stillness_duration_sec;
        has_user_interacted = false;
    }

    void record_sample(uint64_t timestamp_usec, const Point2D& pos) {
        ring_buffer[ring_head] = { timestamp_usec, pos };
        ring_head = (ring_head + 1) % RING_CAPACITY;
        if (ring_count < RING_CAPACITY) {
            ring_count++;
        }
    }

    float compute_window_velocity(uint64_t current_time_usec, const Point2D& current_pos) const {
        if (ring_count < 2) return 0.0f;

        const Sample* oldest = nullptr;
        for (size_t i = 0; i < ring_count; ++i) {
            size_t idx = (ring_head + RING_CAPACITY - 1 - i) % RING_CAPACITY;
            const Sample& s = ring_buffer[idx];
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

    bool is_mouse_still() const {
        return state == STATE_STILL;
    }

    bool is_mouse_active() const {
        return state != STATE_STILL;
    }

    float get_target_blend(bool camera_tracking_active, bool emulate_gaze_from_mouse) const {
        if (!emulate_gaze_from_mouse) return 0.0f;
        if (state == STATE_ACTIVE) {
            return 1.0f;
        }
        return 0.0f;
    }

    void update(
        double delta_sec,
        uint64_t timestamp_usec,
        const Point2D& screen_mouse_pos,
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
            if (mouse_clicked || dist_from_anchor >= anchor_bubble_radius_px) {
                has_user_interacted = true;
                state = STATE_ACTIVE;
                stillness_timer = 0.0f;
                anchor_pos = screen_mouse_pos;
            } else {
                stillness_timer += (float)delta_sec;
                if (stillness_timer >= stillness_duration_sec) {
                    state = STATE_STILL;
                }
            }
        } else {
            // In STATE_ACTIVE:
            if (!mouse_clicked && v_window < window_velocity_threshold_px_s) {
                state = STATE_SETTLING;
                anchor_pos = screen_mouse_pos;
                stillness_timer = (float)delta_sec;
                if (stillness_timer >= stillness_duration_sec) {
                    state = STATE_STILL;
                }
            } else {
                has_user_interacted = true;
                anchor_pos = screen_mouse_pos;
                stillness_timer = 0.0f;
            }
        }
    }

    float get_stillness_duration() const { return stillness_duration_sec; }
    void set_stillness_duration(float p_sec) { stillness_duration_sec = p_sec; }

    float get_anchor_bubble_radius_px() const { return anchor_bubble_radius_px; }
    void set_anchor_bubble_radius_px(float p_px) { anchor_bubble_radius_px = p_px; }

    float get_window_velocity_threshold_px_s() const { return window_velocity_threshold_px_s; }
    void set_window_velocity_threshold_px_s(float p_px_s) { window_velocity_threshold_px_s = p_px_s; }

    uint64_t get_window_duration_usec() const { return window_duration_usec; }
    void set_window_duration_usec(uint64_t p_usec) { window_duration_usec = p_usec; }

    float get_stillness_timer() const { return stillness_timer; }
    bool has_interacted() const { return has_user_interacted; }
};

} // namespace Gaze
