#pragma once
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector2i.hpp>

namespace godot {

class GazeFrame : public Object {
    GDCLASS(GazeFrame, Object);

private:
    PackedByteArray left_eye_bytes;
    PackedByteArray right_eye_bytes;
    PackedByteArray full_crop_bytes;

    Ref<Image> left_eye_image;
    Ref<Image> right_eye_image;
    Ref<Image> full_crop_image;

    Vector2i camera_size;
    double timestamp = 0.0;
    bool face_detected = false;
    bool gaze_success = false;

    Vector3 head_translation;
    Vector3 head_rotation;
    Vector3 gaze_origin;
    Vector3 gaze_direction;

protected:
    static void _bind_methods();

public:
    GazeFrame();
    virtual ~GazeFrame();

    void initialize_buffers();
    void post_process();


    // Getters and setters for properties
    Ref<Image> get_left_eye_crop() const;
    Ref<Image> get_right_eye_crop() const;
    Ref<Image> get_full_crop() const;


    uint8_t* get_left_eye_buffer_ptr() { return left_eye_bytes.ptrw(); }
    uint8_t* get_right_eye_buffer_ptr() { return right_eye_bytes.ptrw(); }
    uint8_t* get_full_crop_buffer_ptr() { return full_crop_bytes.ptrw(); }
    size_t get_full_crop_bytes_size() const { return full_crop_bytes.size(); }

    void resize_full_crop(int width, int height);

    void set_camera_size(Vector2i size) { camera_size = size; }
    Vector2i get_camera_size() const { return camera_size; }

    void set_timestamp(double t) { timestamp = t; }
    double get_timestamp() const { return timestamp; }

    void set_face_detected(bool d) { face_detected = d; }
    bool get_face_detected() const { return face_detected; }

    void set_gaze_success(bool s) { gaze_success = s; }
    bool get_gaze_success() const { return gaze_success; }

    void set_head_translation(Vector3 t) { head_translation = t; }
    Vector3 get_head_translation() const { return head_translation; }

    void set_head_rotation(Vector3 r) { head_rotation = r; }
    Vector3 get_head_rotation() const { return head_rotation; }

    void set_gaze_origin(Vector3 o) { gaze_origin = o; }
    Vector3 get_gaze_origin() const { return gaze_origin; }

    void set_gaze_direction(Vector3 d) { gaze_direction = d; }
    Vector3 get_gaze_direction() const { return gaze_direction; }
};

} // namespace godot
