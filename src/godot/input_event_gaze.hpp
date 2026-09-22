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

    /**
     * @brief Sets the sequential video pipeline frame identifier.
     * @param p_id Monotonically increasing frame counter.
     */
    void set_frame_id(uint64_t p_id) { frame_id = p_id; }

    /**
     * @brief Retrieves the sequential video pipeline frame identifier.
     * @return uint64_t Frame counter.
     */
    uint64_t get_frame_id() const { return frame_id; }

    /**
     * @brief Sets the event creation timestamp in microseconds (from Time::get_ticks_usec()).
     * @param p_time Timestamp in microseconds.
     */
    void set_timestamp_usec(uint64_t p_time) { timestamp_usec = p_time; }

    /**
     * @brief Retrieves the event creation timestamp in microseconds.
     * @return uint64_t Timestamp in microseconds.
     */
    uint64_t get_timestamp_usec() const { return timestamp_usec; }

    /**
     * @brief Sets the left eye openness estimate [0.0 = fully closed, 1.0 = fully open].
     * @param p_val Openness value between 0.0 and 1.0.
     */
    void set_left_eye_openness(float p_val) { left_eye_openness = p_val; }

    /**
     * @brief Retrieves the left eye openness estimate [0.0 = fully closed, 1.0 = fully open].
     * @return float Openness value between 0.0 and 1.0.
     */
    float get_left_eye_openness() const { return left_eye_openness; }

    /**
     * @brief Sets the right eye openness estimate [0.0 = fully closed, 1.0 = fully open].
     * @param p_val Openness value between 0.0 and 1.0.
     */
    void set_right_eye_openness(float p_val) { right_eye_openness = p_val; }

    /**
     * @brief Retrieves the right eye openness estimate [0.0 = fully closed, 1.0 = fully open].
     * @return float Openness value between 0.0 and 1.0.
     */
    float get_right_eye_openness() const { return right_eye_openness; }

    /**
     * @brief Indicates whether valid face tracking data is present in this event.
     * @return bool True if face is tracked; false for missing or lost tracking events.
     */
    virtual bool is_face_tracked() const { return false; }

    void copy_from(const Ref<InputEventGazeBase> &p_other);
};

/**
 * @class InputEventGaze
 * @brief Input event emitted when face and eye gaze tracking is actively locked.
 *
 * Provides localized 2D gaze coordinates via get_eye_gaze() and get_nose_gaze(),
 * as well as 3D head pose and eye gaze ray properties in Godot camera space.
 */
class InputEventGaze : public InputEventGazeBase {
    GDCLASS(InputEventGaze, InputEventGazeBase);

public:
    enum ClampingMode {
        CLAMPING_FREE = 0,
        CLAMPING_CLAMPED = 1,
        CLAMPING_DEFAULT = 2,
    };

private:
    Vector2 eye_gaze_position;
    Vector2 nose_gaze_position;

    Transform3D head_transform;
    Transform3D eye_transform;

protected:
    static void _bind_methods();

public:
    InputEventGaze();
    virtual ~InputEventGaze() = default;

    virtual bool is_face_tracked() const override { return true; }

    /**
     * @brief Retrieves the 2D eye gaze coordinates on the viewport canvas or relative to a specified CanvasItem.
     *
     * @param p_local_to Optional CanvasItem (e.g. Node2D or Control). If null (default), returns the position in
     *                   canonical root Viewport Canvas space (pixels). If provided, transforms the position into
     *                   the local coordinate space of that CanvasItem.
     * @param p_clamping Viewport boundary clamping behavior (CLAMPING_FREE, CLAMPING_CLAMPED, or CLAMPING_DEFAULT).
     * @return Vector2 Gaze position in canvas space or local coordinates.
     */
    Vector2 get_eye_gaze(const CanvasItem *p_local_to = nullptr, ClampingMode p_clamping = CLAMPING_DEFAULT) const;
    Vector2 _get_eye_gaze_property() const { return get_eye_gaze(); }

    /**
     * @brief Sets the 2D eye gaze position in canonical root Viewport Canvas coordinates.
     * @param p_pos Position in root Viewport Canvas coordinates.
     */
    void set_eye_gaze(const Vector2 &p_pos);

    /**
     * @brief Retrieves the 2D nose gaze coordinates (nose-forward ray projected to the display) on the canvas or relative to a CanvasItem.
     *
     * Represents the forward projection of the head pose onto the screen surface.
     *
     * @param p_local_to Optional CanvasItem (e.g. Node2D or Control). If null (default), returns the position in
     *                   canonical root Viewport Canvas space (pixels). If provided, transforms the position into
     *                   the local coordinate space of that CanvasItem.
     * @param p_clamping Viewport boundary clamping behavior (CLAMPING_FREE, CLAMPING_CLAMPED, or CLAMPING_DEFAULT).
     * @return Vector2 Nose gaze projection in canvas space or local coordinates.
     */
    Vector2 get_nose_gaze(const CanvasItem *p_local_to = nullptr, ClampingMode p_clamping = CLAMPING_DEFAULT) const;

    /**
     * @brief Sets the 2D nose gaze position in canonical root Viewport Canvas coordinates.
     * @param p_pos Position in root Viewport Canvas coordinates.
     */
    void set_nose_gaze(const Vector2 &p_pos);

    /**
     * @brief Sets the underlying 3D head transform in Godot camera space.
     * @param p_xform 3D transform of the head.
     */
    void set_head_transform(const Transform3D &p_xform) { head_transform = p_xform; }

    /**
     * @brief Retrieves the underlying 3D head transform in Godot camera space.
     * @return Transform3D Head transform.
     */
    Transform3D get_head_transform() const { return head_transform; }

    /**
     * @brief Sets the underlying 3D eye gaze transform in Godot camera space.
     * @param p_xform 3D transform of the eye gaze.
     */
    void set_eye_transform(const Transform3D &p_xform) { eye_transform = p_xform; }

    /**
     * @brief Retrieves the underlying 3D eye gaze transform in Godot camera space.
     * @return Transform3D Eye gaze transform.
     */
    Transform3D get_eye_transform() const { return eye_transform; }

    /**
     * @brief Backward-compatibility alias for get_eye_transform().
     */
    Transform3D get_gaze_transform() const { return eye_transform; }

    /**
     * @brief Backward-compatibility alias for set_eye_transform().
     */
    void set_gaze_transform(const Transform3D &p_xform) { eye_transform = p_xform; }

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

    /**
     * @brief Sets the reason why gaze tracking was not achieved for this frame.
     * @param p_reason MissingReason enum value.
     */
    void set_reason(MissingReason p_reason) { reason = p_reason; }

    /**
     * @brief Retrieves the reason why gaze tracking was not achieved for this frame.
     * @return MissingReason Enum value indicating the cause.
     */
    MissingReason get_reason() const { return reason; }

    void copy_from(const Ref<InputEventGazeMissing> &p_other);

    String as_text() const;
};

} // namespace godot

VARIANT_ENUM_CAST(godot::InputEventGaze::ClampingMode);
VARIANT_ENUM_CAST(godot::InputEventGazeMissing::MissingReason);
