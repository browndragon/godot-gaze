#include "input_event_gaze.hpp"
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include "gaze_server.hpp"

namespace godot {

// --- InputEventGazeBase ---

InputEventGazeBase::InputEventGazeBase() {
    frame_id = 0;
    timestamp_usec = 0;
    left_eye_openness = 0.0f;
    right_eye_openness = 0.0f;
}

void InputEventGazeBase::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_window_id", "id"), &InputEventGazeBase::set_window_id);
    ClassDB::bind_method(D_METHOD("get_window_id"), &InputEventGazeBase::get_window_id);

    ClassDB::bind_method(D_METHOD("set_frame_id", "frame_id"), &InputEventGazeBase::set_frame_id);
    ClassDB::bind_method(D_METHOD("get_frame_id"), &InputEventGazeBase::get_frame_id);

    ClassDB::bind_method(D_METHOD("set_timestamp_usec", "timestamp_usec"), &InputEventGazeBase::set_timestamp_usec);
    ClassDB::bind_method(D_METHOD("get_timestamp_usec"), &InputEventGazeBase::get_timestamp_usec);

    ClassDB::bind_method(D_METHOD("set_left_eye_openness", "openness"), &InputEventGazeBase::set_left_eye_openness);
    ClassDB::bind_method(D_METHOD("get_left_eye_openness"), &InputEventGazeBase::get_left_eye_openness);

    ClassDB::bind_method(D_METHOD("set_right_eye_openness", "openness"), &InputEventGazeBase::set_right_eye_openness);
    ClassDB::bind_method(D_METHOD("get_right_eye_openness"), &InputEventGazeBase::get_right_eye_openness);

    ClassDB::bind_method(D_METHOD("is_face_tracked"), &InputEventGazeBase::is_face_tracked);

    ClassDB::bind_method(D_METHOD("copy_from", "other"), &InputEventGazeBase::copy_from);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "window_id"), "set_window_id", "get_window_id");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "frame_id"), "set_frame_id", "get_frame_id");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "timestamp_usec"), "set_timestamp_usec", "get_timestamp_usec");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "left_eye_openness"), "set_left_eye_openness", "get_left_eye_openness");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "right_eye_openness"), "set_right_eye_openness", "get_right_eye_openness");
}

void InputEventGazeBase::copy_from(const Ref<InputEventGazeBase> &p_other) {
    if (p_other.is_valid()) {
        window_id = p_other->window_id;
        frame_id = p_other->frame_id;
        timestamp_usec = p_other->timestamp_usec;
        left_eye_openness = p_other->left_eye_openness;
        right_eye_openness = p_other->right_eye_openness;
    }
}

// --- InputEventGaze ---

InputEventGaze::InputEventGaze() {
    left_eye_openness = 1.0f;
    right_eye_openness = 1.0f;
    eye_gaze_position = Vector2(0, 0);
    nose_gaze_position = Vector2(0, 0);
    head_transform = Transform3D();
    eye_transform = Transform3D();
}

static Vector2 _clamp_to_viewport(const Vector2 &p_pos, const CanvasItem *p_local_to) {
    Vector2 vp_size(1152.0f, 648.0f);
    if (p_local_to && p_local_to->get_viewport()) {
        vp_size = p_local_to->get_viewport()->get_visible_rect().size;
    } else {
        SceneTree *st = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
        if (st && st->get_root()) {
            vp_size = st->get_root()->get_visible_rect().size;
        } else if (GazeServer::get_singleton()) {
            vp_size = GazeServer::get_singleton()->get_canonical_viewport_size();
        }
    }
    if (vp_size.x > 0.0f && vp_size.y > 0.0f) {
        return Vector2(
            std::clamp(p_pos.x, 0.0f, vp_size.x - 1e-4f),
            std::clamp(p_pos.y, 0.0f, vp_size.y - 1e-4f)
        );
    }
    return p_pos;
}

Vector2 InputEventGaze::get_eye_gaze(const CanvasItem *p_local_to, ClampingMode p_clamping) const {
    bool should_clamp = false;
    if (p_clamping == CLAMPING_CLAMPED) {
        should_clamp = true;
    } else if (p_clamping == CLAMPING_DEFAULT) {
        GazeServer *gs = GazeServer::get_singleton();
        should_clamp = gs ? gs->is_clamping_by_default() : true;
    }

    Vector2 pos = should_clamp ? _clamp_to_viewport(eye_gaze_position, p_local_to) : eye_gaze_position;

    if (!p_local_to) {
        return pos;
    }
    return p_local_to->get_global_transform_with_canvas().affine_inverse().xform(pos);
}

void InputEventGaze::set_eye_gaze(const Vector2 &p_pos) {
    eye_gaze_position = p_pos;
}

Vector2 InputEventGaze::get_nose_gaze(const CanvasItem *p_local_to, ClampingMode p_clamping) const {
    bool should_clamp = false;
    if (p_clamping == CLAMPING_CLAMPED) {
        should_clamp = true;
    } else if (p_clamping == CLAMPING_DEFAULT) {
        GazeServer *gs = GazeServer::get_singleton();
        should_clamp = gs ? gs->is_clamping_by_default() : true;
    }

    Vector2 pos = should_clamp ? _clamp_to_viewport(nose_gaze_position, p_local_to) : nose_gaze_position;

    if (!p_local_to) {
        return pos;
    }
    return p_local_to->get_global_transform_with_canvas().affine_inverse().xform(pos);
}

void InputEventGaze::set_nose_gaze(const Vector2 &p_pos) {
    nose_gaze_position = p_pos;
}

void InputEventGaze::_bind_methods() {
    BIND_ENUM_CONSTANT(CLAMPING_FREE);
    BIND_ENUM_CONSTANT(CLAMPING_CLAMPED);
    BIND_ENUM_CONSTANT(CLAMPING_DEFAULT);

    ClassDB::bind_method(D_METHOD("get_eye_gaze", "local_to", "clamping"), &InputEventGaze::get_eye_gaze, DEFVAL(nullptr), DEFVAL(CLAMPING_DEFAULT));
    ClassDB::bind_method(D_METHOD("set_eye_gaze", "eye_gaze"), &InputEventGaze::set_eye_gaze);

    ClassDB::bind_method(D_METHOD("get_nose_gaze", "local_to", "clamping"), &InputEventGaze::get_nose_gaze, DEFVAL(nullptr), DEFVAL(CLAMPING_DEFAULT));
    ClassDB::bind_method(D_METHOD("set_nose_gaze", "nose_gaze"), &InputEventGaze::set_nose_gaze);

    ClassDB::bind_method(D_METHOD("set_head_transform", "head_transform"), &InputEventGaze::set_head_transform);
    ClassDB::bind_method(D_METHOD("get_head_transform"), &InputEventGaze::get_head_transform);

    ClassDB::bind_method(D_METHOD("set_eye_transform", "eye_transform"), &InputEventGaze::set_eye_transform);
    ClassDB::bind_method(D_METHOD("get_eye_transform"), &InputEventGaze::get_eye_transform);

    ClassDB::bind_method(D_METHOD("copy_from", "other"), &InputEventGaze::copy_from);

    ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "head_transform"), "set_head_transform", "get_head_transform");
    ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "eye_transform"), "set_eye_transform", "get_eye_transform");
}

void InputEventGaze::copy_from(const Ref<InputEventGaze> &p_other) {
    if (p_other.is_valid()) {
        InputEventGazeBase::copy_from(p_other);
        eye_gaze_position = p_other->eye_gaze_position;
        nose_gaze_position = p_other->nose_gaze_position;
        head_transform = p_other->head_transform;
        eye_transform = p_other->eye_transform;
    }
}

String InputEventGaze::as_text() const {
    return String("InputEventGaze: eye=") + String(eye_gaze_position) + ", nose=" + String(nose_gaze_position) + ", open=(" + String::num(left_eye_openness, 2) + ", " + String::num(right_eye_openness, 2) + ")";
}

// --- InputEventGazeMissing ---

InputEventGazeMissing::InputEventGazeMissing() {
    left_eye_openness = 0.0f;
    right_eye_openness = 0.0f;
    reason = REASON_NO_FACE_DETECTED;
}

void InputEventGazeMissing::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_reason", "reason"), &InputEventGazeMissing::set_reason);
    ClassDB::bind_method(D_METHOD("get_reason"), &InputEventGazeMissing::get_reason);
    ClassDB::bind_method(D_METHOD("copy_from", "other"), &InputEventGazeMissing::copy_from);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "reason", PROPERTY_HINT_ENUM, "NoFaceDetected,Occluded,OutOfBounds,SensorInactive"), "set_reason", "get_reason");

    BIND_ENUM_CONSTANT(REASON_NO_FACE_DETECTED);
    BIND_ENUM_CONSTANT(REASON_OCCLUDED);
    BIND_ENUM_CONSTANT(REASON_OUT_OF_BOUNDS);
    BIND_ENUM_CONSTANT(REASON_SENSOR_INACTIVE);
}

void InputEventGazeMissing::copy_from(const Ref<InputEventGazeMissing> &p_other) {
    if (p_other.is_valid()) {
        InputEventGazeBase::copy_from(p_other);
        reason = p_other->reason;
    }
}

String InputEventGazeMissing::as_text() const {
    return String("InputEventGazeMissing: reason=") + String::num_int64(reason);
}

} // namespace godot
