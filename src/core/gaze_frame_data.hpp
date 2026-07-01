#pragma once
#include <vector>
#include <cstdint>
#include "math_defs.hpp"

namespace Gaze {

constexpr size_t EYE_CROP_SIZE = 60;
constexpr size_t EYE_CROP_CHANNELS = 3;
constexpr size_t EYE_CROP_BYTES = EYE_CROP_SIZE * EYE_CROP_SIZE * EYE_CROP_CHANNELS; // 10800 bytes

struct GazeFrameData {
    std::vector<uint8_t> camera_raw_bgr;
    int camera_width = 0;
    int camera_height = 0;
    double timestamp = 0.0;
    double camera_focal_length_px = -1.0;
    double camera_fov_degrees = DEFAULT_CAMERA_FOV_DEGREES;

    bool face_detected = false;
    bool gaze_success = false;

    GazeVector3 head_translation;
    GazeVector3 head_rotation;
    GazeVector3 gaze_origin;
    GazeVector3 gaze_direction;

    // Zero-copy pointers to Godot Image backing buffers
    uint8_t* left_eye_buffer = nullptr;
    uint8_t* right_eye_buffer = nullptr;
    uint8_t* full_crop_buffer = nullptr;
    size_t full_crop_bytes = 0;

    uint64_t face_rid_val = 0;
    uint64_t eye_rid_val = 0;

    // Backreference to Godot wrapper object (GazeFrame*)
    void* userdata = nullptr;

};

} // namespace Gaze
