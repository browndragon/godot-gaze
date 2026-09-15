#include "mouse_gaze_emulation.hpp"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/math.hpp>
#include <algorithm>

namespace godot {

MouseGazeEmulation::MouseGazeEmulation() {
    last_cam_head_xform = Transform3D(Basis(Vector3(-1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, -1)), Vector3(0, 0, -500.0f));
    last_cam_gaze_xform = Transform3D(Basis::looking_at(Vector3(0, 0, 1), Vector3(0, 1, 0)), Vector3(0, 0, -500.0f));
}

void MouseGazeEmulation::reset() {
    dwell_timer = 0.0f;
    blend_progress = 0.0f;
    has_last_mouse_pos = false;
    has_last_window_state = false;
    has_camera_data = false;
    arbitrator.reset();
}

void MouseGazeEmulation::notify_camera_event(const Ref<InputEventGazeBase>& p_cam_event) {
    if (p_cam_event.is_null()) return;
    InputEventGaze* gaze = Object::cast_to<InputEventGaze>(p_cam_event.ptr());
    if (gaze && gaze->is_face_tracked()) {
        last_cam_gaze_pos = gaze->get_eye_gaze();
        last_cam_head_xform = gaze->get_head_transform();
        last_cam_gaze_xform = gaze->get_eye_transform();
        last_cam_left_open = gaze->get_left_eye_openness();
        last_cam_right_open = gaze->get_right_eye_openness();
        has_camera_data = true;
    }
}

float MouseGazeEmulation::get_eased_blend_factor() const {
    float t = std::clamp(blend_progress, 0.0f, 1.0f);
    // Smoothstep: 3t^2 - 2t^3 (C1 continuous IN_OUT easing)
    return t * t * (3.0f - 2.0f * t);
}

void MouseGazeEmulation::update(
    double p_delta_sec,
    bool p_camera_tracking_active,
    bool p_face_detected,
    bool p_emulate_gaze_from_mouse,
    bool p_emulate_mouse_from_gaze,
    GazeDisplayServer* p_gds
) {
    if (!p_emulate_gaze_from_mouse && !p_emulate_mouse_from_gaze) {
        dwell_timer = 0.0f;
        blend_progress = 0.0f;
        arbitrator.reset();
        return;
    }

    Vector2 screen_mouse_pos = p_gds ? p_gds->mouse_get_position() : Vector2(0, 0);
    Vector2 window_pos = p_gds ? Vector2(p_gds->get_window_rect_pixels().position) : Vector2(0, 0);
    Vector2 mouse_pos = screen_mouse_pos - window_pos;

    bool mouse_clicked = p_gds ? (p_gds->mouse_get_button_state() != 0) : false;

    uint64_t now_usec = Time::get_singleton()->get_ticks_usec();

    arbitrator.update(
        p_delta_sec,
        now_usec,
        Gaze::MouseStillnessArbitrator::Point2D(screen_mouse_pos.x, screen_mouse_pos.y),
        mouse_clicked
    );

    last_mouse_pos = mouse_pos;
    last_screen_mouse_pos = screen_mouse_pos;
    last_window_pos = window_pos;
    has_last_mouse_pos = true;
    has_last_window_state = true;

    // Maintain backwards-compatible dwell_timer
    dwell_timer = (arbitrator.is_mouse_active()) ? (arbitrator.get_stillness_duration() - arbitrator.get_stillness_timer()) : 0.0f;

    // G <- M blend progress update:
    if (!p_emulate_gaze_from_mouse) {
        blend_progress = 0.0f;
        return;
    }

    // Determine target blend weight (1.0 = mouse, 0.0 = camera)
    float target_blend = arbitrator.get_target_blend(p_camera_tracking_active, p_emulate_gaze_from_mouse);

    // Step blend_progress towards target_blend
    float step = (transition_duration_sec > 0.001f) ? ((float)p_delta_sec / transition_duration_sec) : 1.0f;
    if (blend_progress < target_blend) {
        blend_progress = std::min(target_blend, blend_progress + step);
    } else if (blend_progress > target_blend) {
        blend_progress = std::max(target_blend, blend_progress - step);
    }
}


Ref<InputEventGazeBase> MouseGazeEmulation::synthesize_event(
    GazeDisplayServer* p_gds,
    const Ref<GazeDeviceProfile>& p_profile,
    const Ref<GazeEventFactory>& p_event_factory,
    uint64_t &r_frame_id,
    uint64_t &r_last_event_time_usec,
    Vector2 &r_last_gaze_pos,
    Vector2 &r_last_screen_pos
) {
    Vector2 mouse_pos = p_gds ? (p_gds->mouse_get_position() - Vector2(p_gds->get_window_rect_pixels().position)) : Vector2(0, 0);

    Ref<GazeDeviceProfile> profile = p_profile;
    if (!profile.is_valid()) {
        profile = GazeDeviceProfile::create_system_guess();
    }
    Vector2i log_sz = profile->get_logical_size_px();
    Vector2 phys_sz = profile->get_physical_size_mm();
    if (log_sz.x <= 0) log_sz.x = 1920;
    if (log_sz.y <= 0) log_sz.y = 1080;
    if (phys_sz.x <= 0.0) phys_sz.x = 345.0;
    if (phys_sz.y <= 0.0) phys_sz.y = 215.0;

    Vector3 cam_offset = profile->get_camera_offset_mm();

    float x_s = (mouse_pos.x / log_sz.x - 0.5f) * phys_sz.x;
    float y_s = (mouse_pos.y / log_sz.y - 0.5f) * phys_sz.y;

    Vector3 target_cam(-(x_s - cam_offset.x), -(y_s + cam_offset.y), 0.0f);
    Vector3 eye_origin_cam(0.0f, 0.0f, -500.0f);
    Vector3 gaze_dir_cam = (target_cam - eye_origin_cam).normalized();

    Transform3D mouse_head_xform(Basis(Vector3(-1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, -1)), eye_origin_cam);
    Transform3D mouse_gaze_xform(Basis::looking_at(gaze_dir_cam, Vector3(0, 1, 0)), eye_origin_cam);

    float ease_factor = get_eased_blend_factor();

    godot::SceneTree *st = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    Vector2 mouse_canvas = mouse_pos;
    if (st && st->get_root()) {
        mouse_canvas = st->get_root()->get_final_transform().affine_inverse().xform(mouse_pos);
    }

    Vector2 blended_canvas_pos = mouse_canvas;
    Transform3D blended_head = mouse_head_xform;
    Transform3D blended_gaze = mouse_gaze_xform;
    float blended_left_open = 1.0f;
    float blended_right_open = 1.0f;

    if (has_camera_data && last_cam_gaze_pos != Vector2(0, 0) && ease_factor < 0.999f) {
        blended_canvas_pos = last_cam_gaze_pos.lerp(mouse_canvas, ease_factor);
        blended_head = last_cam_head_xform.interpolate_with(mouse_head_xform, ease_factor);
        blended_gaze = last_cam_gaze_xform.interpolate_with(mouse_gaze_xform, ease_factor);
        blended_left_open = Math::lerp(last_cam_left_open, 1.0f, ease_factor);
        blended_right_open = Math::lerp(last_cam_right_open, 1.0f, ease_factor);
    }

    Ref<InputEventGaze> event;
    if (p_event_factory.is_valid()) {
        Ref<InputEventGazeBase> created;
        if (p_event_factory->get_script().get_type() != Variant::NIL) {
            created = p_event_factory->call("create_gaze_event");
        } else {
            created = p_event_factory->create_gaze_event();
        }
        if (created.is_valid()) {
            event = Object::cast_to<InputEventGaze>(created.ptr());
        }
    }
    if (event.is_null()) {
        event.instantiate();
    }

    uint64_t now_usec = Time::get_singleton()->get_ticks_usec();
    float dt = (r_last_event_time_usec > 0 && now_usec > r_last_event_time_usec) ? (float)(now_usec - r_last_event_time_usec) / 1000000.0f : 0.016667f;
    if (dt < 0.0001f) dt = 0.0001f;

    Vector2 screen_pos = mouse_pos;
    if (p_gds) screen_pos += Vector2(p_gds->get_window_rect_pixels().position);

    r_last_gaze_pos = blended_canvas_pos;
    r_last_screen_pos = screen_pos;

    event->set_frame_id(++r_frame_id);
    event->set_timestamp_usec(now_usec);
    event->set_eye_gaze(blended_canvas_pos);
    event->set_nose_gaze(blended_canvas_pos);
    event->set_left_eye_openness(blended_left_open);
    event->set_right_eye_openness(blended_right_open);
    event->set_head_transform(blended_head);
    event->set_eye_transform(blended_gaze);

    return event;
}

} // namespace godot
