#ifndef GAZE_MOUSE_GAZE_EMULATION_HPP
#define GAZE_MOUSE_GAZE_EMULATION_HPP

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "input_event_gaze.hpp"
#include "gaze_device_profile.hpp"
#include "gaze_event_factory.hpp"

namespace godot {

class MouseGazeEmulation {
private:
    float dwell_time_sec = 2.0f;
    float transition_duration_sec = 0.3f;
    float motion_threshold_px = 1.5f;

    float dwell_timer = 0.0f;
    float blend_progress = 0.0f; // 0.0 = pure camera gaze, 1.0 = pure mouse gaze

    Vector2 last_mouse_pos = Vector2(-9999.0, -9999.0);
    Vector2 last_screen_mouse_pos = Vector2(-9999.0, -9999.0);
    Vector2 last_window_pos = Vector2(-9999.0, -9999.0);
    DisplayServer::WindowMode last_window_mode = DisplayServer::WINDOW_MODE_WINDOWED;
    bool has_last_mouse_pos = false;
    bool has_last_window_state = false;

    Vector2 last_cam_gaze_pos = Vector2(0, 0);
    Transform3D last_cam_head_xform;
    Transform3D last_cam_gaze_xform;
    float last_cam_left_open = 1.0f;
    float last_cam_right_open = 1.0f;
    bool has_camera_data = false;

public:
    MouseGazeEmulation();
    ~MouseGazeEmulation() = default;

    void set_dwell_time(float p_sec) { dwell_time_sec = p_sec; }
    float get_dwell_time() const { return dwell_time_sec; }

    void set_transition_duration(float p_sec) { transition_duration_sec = p_sec; }
    float get_transition_duration() const { return transition_duration_sec; }

    void set_motion_threshold(float p_px) { motion_threshold_px = p_px; }
    float get_motion_threshold() const { return motion_threshold_px; }

    void notify_camera_event(const Ref<InputEventGazeBase>& p_cam_event);

    void update(
        double p_delta_sec,
        bool p_camera_tracking_active,
        bool p_face_detected,
        bool p_emulate_gaze_from_mouse,
        DisplayServer* p_ds
    );

    bool is_emulation_active() const {
        return blend_progress > 0.001f;
    }

    bool is_in_transition() const {
        return blend_progress > 0.0001f && blend_progress < 0.9999f;
    }

    float get_dwell_timer() const { return dwell_timer; }
    float get_blend_progress() const { return blend_progress; }
    float get_eased_blend_factor() const;

    Ref<InputEventGazeBase> synthesize_event(
        DisplayServer* p_ds,
        const Ref<GazeDeviceProfile>& p_profile,
        const Ref<GazeEventFactory>& p_event_factory,
        uint64_t &r_frame_id,
        uint64_t &r_last_event_time_usec,
        Vector2 &r_last_gaze_pos,
        Vector2 &r_last_screen_pos
    );

    void reset();
};

} // namespace godot

#endif // GAZE_MOUSE_GAZE_EMULATION_HPP
