#pragma once

#include <godot_cpp/classes/input_event_action.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class CanvasItem;

/**
 * @class InputEventGazeBase
 * @brief Base class for all gaze-related input events in Godot.
 */
class InputEventGazeBase : public InputEventAction {
    GDCLASS(InputEventGazeBase, InputEventAction);

protected:
    int64_t window_id = 0;
    uint64_t frame_id = 0;
    uint64_t timestamp_usec = 0;
    float left_eye_openness = 0.0f;
    float right_eye_openness = 0.0f;

    static void _bind_methods();

public:
    InputEventGazeBase();
    virtual ~InputEventGazeBase() = default;

    void set_window_id(int64_t p_id) { window_id = p_id; }
    int64_t get_window_id() const { return window_id; }

    void set_frame_id(uint64_t p_id) { frame_id = p_id; }
    uint64_t get_frame_id() const { return frame_id; }

    void set_timestamp_usec(uint64_t p_time) { timestamp_usec = p_time; }
    uint64_t get_timestamp_usec() const { return timestamp_usec; }

    void set_left_eye_openness(float p_val) { left_eye_openness = p_val; }
    float get_left_eye_openness() const { return left_eye_openness; }

    void set_right_eye_openness(float p_val) { right_eye_openness = p_val; }
    float get_right_eye_openness() const { return right_eye_openness; }

    virtual bool is_face_tracked() const { return false; }

    void copy_from(const Ref<InputEventGazeBase> &p_other);
};

/**
 * @class InputEventGaze
 * @brief Input event emitted when face and eye gaze tracking is actively locked.
 */
class InputEventGaze : public InputEventGazeBase {
    GDCLASS(InputEventGaze, InputEventGazeBase);

    Vector2 eye_gaze_position;
    Vector2 nose_gaze_position;

    Transform3D head_transform;
    Transform3D gaze_transform;

protected:
    static void _bind_methods();

public:
    InputEventGaze();
    virtual ~InputEventGaze() = default;

    virtual bool is_face_tracked() const override { return true; }

    Vector2 get_eye_gaze(const CanvasItem *p_local_to = nullptr) const;
    void set_eye_gaze(const Vector2 &p_pos);

    Vector2 get_nose_gaze(const CanvasItem *p_local_to = nullptr) const;
    void set_nose_gaze(const Vector2 &p_pos);

    Transform3D get_head_pose() const { return head_transform; }
    void set_head_pose(const Transform3D &p_pose) { head_transform = p_pose; }

    Vector3 get_eye_origin() const { return gaze_transform.origin; }
    Vector3 get_eye_direction() const { return -gaze_transform.basis.get_column(2); }

    void set_head_transform(const Transform3D &p_xform) { head_transform = p_xform; }
    Transform3D get_head_transform() const { return head_transform; }

    void set_gaze_transform(const Transform3D &p_xform) { gaze_transform = p_xform; }
    Transform3D get_gaze_transform() const { return gaze_transform; }

    void copy_from(const Ref<InputEventGaze> &p_other);

    String as_text() const;
};

/**
 * @class InputEventGazeMissing
 * @brief Input event emitted when face or eye tracking is lost or during full blink.
 */
class InputEventGazeMissing : public InputEventGazeBase {
    GDCLASS(InputEventGazeMissing, InputEventGazeBase);

public:
    enum MissingReason {
        REASON_NO_FACE_DETECTED = 0,
        REASON_OCCLUDED = 1,
        REASON_OUT_OF_BOUNDS = 2,
        REASON_SENSOR_INACTIVE = 3
    };

private:
    MissingReason reason = REASON_NO_FACE_DETECTED;

protected:
    static void _bind_methods();

public:
    InputEventGazeMissing();
    virtual ~InputEventGazeMissing() = default;

    virtual bool is_face_tracked() const override { return false; }

    void set_reason(MissingReason p_reason) { reason = p_reason; }
    MissingReason get_reason() const { return reason; }

    void copy_from(const Ref<InputEventGazeMissing> &p_other);

    String as_text() const;
};

} // namespace godot

VARIANT_ENUM_CAST(godot::InputEventGazeMissing::MissingReason);
