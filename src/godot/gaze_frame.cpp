#include "gaze_frame.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void GazeFrame::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_left_eye_crop"), &GazeFrame::get_left_eye_crop);
    ClassDB::bind_method(D_METHOD("get_right_eye_crop"), &GazeFrame::get_right_eye_crop);
    ClassDB::bind_method(D_METHOD("get_full_crop"), &GazeFrame::get_full_crop);

    ClassDB::bind_method(D_METHOD("get_camera_size"), &GazeFrame::get_camera_size);
    ClassDB::bind_method(D_METHOD("get_timestamp"), &GazeFrame::get_timestamp);
    ClassDB::bind_method(D_METHOD("get_face_detected"), &GazeFrame::get_face_detected);
    ClassDB::bind_method(D_METHOD("get_gaze_success"), &GazeFrame::get_gaze_success);
    ClassDB::bind_method(D_METHOD("get_head_translation"), &GazeFrame::get_head_translation);
    ClassDB::bind_method(D_METHOD("get_head_rotation"), &GazeFrame::get_head_rotation);
    ClassDB::bind_method(D_METHOD("get_gaze_origin"), &GazeFrame::get_gaze_origin);
    ClassDB::bind_method(D_METHOD("get_gaze_direction"), &GazeFrame::get_gaze_direction);

    // Only register primitive properties to avoid ClassDB "Instantiated Image used as default value" warnings
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "camera_size"), "", "get_camera_size");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "timestamp"), "", "get_timestamp");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "face_detected"), "", "get_face_detected");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gaze_success"), "", "get_gaze_success");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "head_translation"), "", "get_head_translation");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "head_rotation"), "", "get_head_rotation");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "gaze_origin"), "", "get_gaze_origin");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "gaze_direction"), "", "get_gaze_direction");
}

GazeFrame::GazeFrame() {
    initialize_buffers();
}

GazeFrame::~GazeFrame() {}

void GazeFrame::initialize_buffers() {
    left_eye_bytes.resize(10800);
    right_eye_bytes.resize(10800);
    full_crop_bytes.resize(160 * 128 * 3);

    // Initialize images using dummy/separate arrays so COW is not triggered on the real buffers
    PackedByteArray dummy_bytes;
    dummy_bytes.resize(10800);
    left_eye_image = Image::create_from_data(60, 60, false, Image::FORMAT_RGB8, dummy_bytes);
    right_eye_image = Image::create_from_data(60, 60, false, Image::FORMAT_RGB8, dummy_bytes);

    PackedByteArray dummy_full;
    dummy_full.resize(160 * 128 * 3);
    full_crop_image = Image::create_from_data(160, 128, false, Image::FORMAT_RGB8, dummy_full);
}

void GazeFrame::resize_full_crop(int width, int height) {
    int required_size = width * height * 3;
    if (full_crop_bytes.size() != required_size) {
        full_crop_bytes.resize(required_size);
    }
}

void GazeFrame::post_process() {
    left_eye_image = Image::create_from_data(60, 60, false, Image::FORMAT_RGB8, left_eye_bytes);
    right_eye_image = Image::create_from_data(60, 60, false, Image::FORMAT_RGB8, right_eye_bytes);

    int full_width = 160;
    int full_height = 128;
    if (full_crop_bytes.size() > 0) {
        full_height = full_crop_bytes.size() / (full_width * 3);
    }
    full_crop_image = Image::create_from_data(full_width, full_height, false, Image::FORMAT_RGB8, full_crop_bytes);
}

Ref<Image> GazeFrame::get_left_eye_crop() const {
    return left_eye_image;
}

Ref<Image> GazeFrame::get_right_eye_crop() const {
    return right_eye_image;
}

Ref<Image> GazeFrame::get_full_crop() const {
    return full_crop_image;
}

} // namespace godot
