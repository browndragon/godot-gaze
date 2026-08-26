#include "input_event_gaze.hpp"
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

    ADD_PROPERTY(PropertyInfo(Variant::INT, "window_id"), "set_window_id", "get_window_id");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "frame_id"), "set_frame_id", "get_frame_id");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "timestamp_usec"), "set_timestamp_usec", "get_timestamp_usec");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "left_eye_openness"), "set_left_eye_openness", "get_left_eye_openness");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "right_eye_openness"), "set_right_eye_openness", "get_right_eye_openness");
}

// --- InputEventGaze ---

InputEventGaze::InputEventGaze() {
    left_eye_openness = 1.0f;
    right_eye_openness = 1.0f;
    position = Vector2(0, 0);
    global_position = Vector2(0, 0);
    screen_position = Vector2(0, 0);
    relative = Vector2(0, 0);
    screen_relative = Vector2(0, 0);
    velocity = Vector2(0, 0);
    screen_velocity = Vector2(0, 0);
    head_transform = Transform3D();
    gaze_transform = Transform3D();
}

void InputEventGaze::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_position", "position"), &InputEventGaze::set_position);
    ClassDB::bind_method(D_METHOD("get_position"), &InputEventGaze::get_position);

    ClassDB::bind_method(D_METHOD("set_global_position", "global_position"), &InputEventGaze::set_global_position);
    ClassDB::bind_method(D_METHOD("get_global_position"), &InputEventGaze::get_global_position);

    ClassDB::bind_method(D_METHOD("set_screen_position", "screen_position"), &InputEventGaze::set_screen_position);
    ClassDB::bind_method(D_METHOD("get_screen_position"), &InputEventGaze::get_screen_position);

    ClassDB::bind_method(D_METHOD("set_relative", "relative"), &InputEventGaze::set_relative);
    ClassDB::bind_method(D_METHOD("get_relative"), &InputEventGaze::get_relative);

    ClassDB::bind_method(D_METHOD("set_screen_relative", "screen_relative"), &InputEventGaze::set_screen_relative);
    ClassDB::bind_method(D_METHOD("get_screen_relative"), &InputEventGaze::get_screen_relative);

    ClassDB::bind_method(D_METHOD("set_velocity", "velocity"), &InputEventGaze::set_velocity);
    ClassDB::bind_method(D_METHOD("get_velocity"), &InputEventGaze::get_velocity);

    ClassDB::bind_method(D_METHOD("set_screen_velocity", "screen_velocity"), &InputEventGaze::set_screen_velocity);
    ClassDB::bind_method(D_METHOD("get_screen_velocity"), &InputEventGaze::get_screen_velocity);

    ClassDB::bind_method(D_METHOD("set_head_transform", "head_transform"), &InputEventGaze::set_head_transform);
    ClassDB::bind_method(D_METHOD("get_head_transform"), &InputEventGaze::get_head_transform);

    ClassDB::bind_method(D_METHOD("set_gaze_transform", "gaze_transform"), &InputEventGaze::set_gaze_transform);
    ClassDB::bind_method(D_METHOD("get_gaze_transform"), &InputEventGaze::get_gaze_transform);

    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position"), "set_position", "get_position");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "global_position"), "set_global_position", "get_global_position");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "screen_position"), "set_screen_position", "get_screen_position");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "relative"), "set_relative", "get_relative");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "screen_relative"), "set_screen_relative", "get_screen_relative");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "velocity"), "set_velocity", "get_velocity");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "screen_velocity"), "set_screen_velocity", "get_screen_velocity");
    ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "head_transform"), "set_head_transform", "get_head_transform");
    ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "gaze_transform"), "set_gaze_transform", "get_gaze_transform");
}

String InputEventGaze::as_text() const {
    return String("InputEventGaze: pos=") + String(position) + ", vel=" + String(velocity) + ", open=(" + String::num(left_eye_openness, 2) + ", " + String::num(right_eye_openness, 2) + ")";
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

    ADD_PROPERTY(PropertyInfo(Variant::INT, "reason", PROPERTY_HINT_ENUM, "NoFaceDetected,Occluded,OutOfBounds,SensorInactive"), "set_reason", "get_reason");

    BIND_ENUM_CONSTANT(REASON_NO_FACE_DETECTED);
    BIND_ENUM_CONSTANT(REASON_OCCLUDED);
    BIND_ENUM_CONSTANT(REASON_OUT_OF_BOUNDS);
    BIND_ENUM_CONSTANT(REASON_SENSOR_INACTIVE);
}

String InputEventGazeMissing::as_text() const {
    return String("InputEventGazeMissing: reason=") + String::num_int64(reason);
}

} // namespace godot
