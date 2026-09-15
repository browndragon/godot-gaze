/**
 * @file blink_state_machine.hpp
 * @brief State machine for blink detection and synthetic click coordination.
 * 
 * Enforces the invariant that loss of tracking is never a blink, and initial
 * cold boot or unconfirmed eye states cannot trigger clicks. A click strictly
 * requires transitioning from actively tracked open eyes to closed eyes.
 */

#pragma once

#include <cstdint>

namespace Gaze {

class BlinkStateMachine {
public:
    enum State {
        STATE_UNTRACKED,      // Cold boot, tracking lost, or unconfirmed. CANNOT click.
        STATE_EYES_OPEN,      // Eyes actively tracked and confirmed open.
        STATE_CLICK_PRESSED   // Eyes transitioned to closed. Mouse button down.
    };

    enum Action {
        ACTION_NONE,
        ACTION_BUTTON_DOWN,
        ACTION_BUTTON_UP
    };

    State state = STATE_UNTRACKED;
    float open_threshold = 0.35f;
    float close_threshold = 0.25f;

    BlinkStateMachine() = default;

    void reset(Action &out_action) {
        if (state == STATE_CLICK_PRESSED) {
            out_action = ACTION_BUTTON_UP;
        } else {
            out_action = ACTION_NONE;
        }
        state = STATE_UNTRACKED;
    }

    Action update(bool face_detected, float left_open, float right_open, bool physical_mouse_active) {
        // If physical mouse becomes active or face tracking is lost while button is down:
        // guaranteed release invariant!
        if (state == STATE_CLICK_PRESSED && (physical_mouse_active || !face_detected)) {
            state = STATE_UNTRACKED;
            return ACTION_BUTTON_UP;
        }

        if (!face_detected || physical_mouse_active) {
            state = STATE_UNTRACKED;
            return ACTION_NONE;
        }

        bool eyes_open = (left_open >= open_threshold && right_open >= open_threshold);
        bool eyes_closed = (left_open < close_threshold && right_open < close_threshold);

        switch (state) {
            case STATE_UNTRACKED:
                if (eyes_open) {
                    state = STATE_EYES_OPEN;
                }
                // If eyes_closed or indeterminate, stay UNTRACKED. Never click!
                return ACTION_NONE;

            case STATE_EYES_OPEN:
                if (eyes_closed) {
                    state = STATE_CLICK_PRESSED;
                    return ACTION_BUTTON_DOWN;
                }
                return ACTION_NONE;

            case STATE_CLICK_PRESSED:
                if (eyes_open) {
                    state = STATE_EYES_OPEN;
                    return ACTION_BUTTON_UP;
                }
                return ACTION_NONE;
        }

        return ACTION_NONE;
    }

    bool is_button_down() const { return state == STATE_CLICK_PRESSED; }
    State get_state() const { return state; }
    void set_open_threshold(float p_th) { open_threshold = p_th; }
    float get_open_threshold() const { return open_threshold; }
    void set_close_threshold(float p_th) { close_threshold = p_th; }
    float get_close_threshold() const { return close_threshold; }
};

} // namespace Gaze
