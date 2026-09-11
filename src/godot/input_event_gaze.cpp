#include "input_event_gaze.hpp"
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

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
    gaze_transform = Transform3D();
}

Vector2 InputEventGaze::get_eye_gaze(const CanvasItem *p_local_to) const {
    if (!p_local_to) {
        return eye_gaze_position;
    }
    return p_local_to->get_global_transform_with_canvas().affine_inverse().xform(eye_gaze_position);
}

void InputEventGaze::set_eye_gaze(const Vector2 &p_pos) {
    eye_gaze_position = p_pos;
}

Vector2 InputEventGaze::get_nose_gaze(const CanvasItem *p_local_to) const {
    if (!p_local_to) {
        return nose_gaze_position;
    }
    return p_local_to->get_global_transform_with_canvas().affine_inverse().xform(nose_gaze_position);
}

void InputEventGaze::set_nose_gaze(const Vector2 &p_pos) {
    nose_gaze_position = p_pos;
}

void InputEventGaze::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_eye_gaze", "local_to"), &InputEventGaze::get_eye_gaze, DEFVAL(nullptr));
    ClassDB::bind_method(D_METHOD("set_eye_gaze", "eye_gaze"), &InputEventGaze::set_eye_gaze);

    ClassDB::bind_method(D_METHOD("get_nose_gaze", "local_to"), &InputEventGaze::get_nose_gaze, DEFVAL(nullptr));
    ClassDB::bind_method(D_METHOD("set_nose_gaze", "nose_gaze"), &InputEventGaze::set_nose_gaze);

    ClassDB::bind_method(D_METHOD("get_head_pose"), &InputEventGaze::get_head_pose);
    ClassDB::bind_method(D_METHOD("set_head_pose", "head_pose"), &InputEventGaze::set_head_pose);

    ClassDB::bind_method(D_METHOD("get_eye_origin"), &InputEventGaze::get_eye_origin);
    ClassDB::bind_method(D_METHOD("get_eye_direction"), &InputEventGaze::get_eye_direction);

    ClassDB::bind_method(D_METHOD("set_head_transform", "head_transform"), &InputEventGaze::set_head_transform);
    ClassDB::bind_method(D_METHOD("get_head_transform"), &InputEventGaze::get_head_transform);

    ClassDB::bind_method(D_METHOD("set_gaze_transform", "gaze_transform"), &InputEventGaze::set_gaze_transform);
    ClassDB::bind_method(D_METHOD("get_gaze_transform"), &InputEventGaze::get_gaze_transform);

    ClassDB::bind_method(D_METHOD("copy_from", "other"), &InputEventGaze::copy_from);

    ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "head_pose"), "set_head_pose", "get_head_pose");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "eye_origin"), "", "get_eye_origin");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "eye_direction"), "", "get_eye_direction");
    ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "head_transform"), "set_head_transform", "get_head_transform");
    ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "gaze_transform"), "set_gaze_transform", "get_gaze_transform");
}

void InputEventGaze::copy_from(const Ref<InputEventGaze> &p_other) {
    if (p_other.is_valid()) {
        InputEventGazeBase::copy_from(p_other);
        eye_gaze_position = p_other->eye_gaze_position;
        nose_gaze_position = p_other->nose_gaze_position;
        head_transform = p_other->head_transform;
        gaze_transform = p_other->gaze_transform;
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
