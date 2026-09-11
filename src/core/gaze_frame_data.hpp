#pragma once
#include <vector>
#include <cstdint>
#include "math_defs.hpp"
#include "face_pipeline.hpp"

namespace Gaze {

constexpr size_t EYE_CROP_SIZE = 60;
constexpr size_t EYE_CROP_CHANNELS = 3;
constexpr size_t EYE_CROP_BYTES = EYE_CROP_SIZE * EYE_CROP_SIZE * EYE_CROP_CHANNELS; // 10800 bytes

struct StageTimings {
    double capture_gpu_readback_ms = 0.0;
    double roll_prewarp_ms = 0.0;
    double face_yunet_ms = 0.0;
    double landmark_adas_ms = 0.0;
    double pnp_solve_ms = 0.0;
    double eye_crop_warp_ms = 0.0;
    double eye_state_ms = 0.0;
    double gaze_direction_ms = 0.0;
    double unroll_ms = 0.0;
    double total_pipeline_ms = 0.0;
};

struct GazeFrameData {
    std::vector<uint8_t> camera_raw_bgr;
    std::vector<uint8_t> internal_rotated_frame_bgr; // Pre-allocated scratch space for working hint-rolled buffer
    int camera_width = 0;
    int camera_height = 0;
    double timestamp = 0.0;
    double camera_focal_length_px = -1.0;
    double camera_fov_degrees = DEFAULT_CAMERA_FOV_DEGREES;

    float roll_hint_rad = 0.0f;
    bool auto_roll_enabled = true;
    GazeRect face_bbox;
    float face_score = 0.0f;
    float eye_box_sz = 0.0f;
    std::vector<SpacedVector2<Space::GodotCameraWorkingImagePixels>> internal_landmarks_working_px;
    EyeCrops eye_crops;

    bool face_detected = false;
    bool gaze_success = false;
    float left_eye_openness = 1.0f;
    float right_eye_openness = 1.0f;

    GodotFaceTransform3D head_transform;
    GodotCameraVector3 head_translation = GodotCameraVector3(0.0, 0.0, 0.0);
    SpacedVector3<Space::GodotCameraEuler> head_rotation = SpacedVector3<Space::GodotCameraEuler>(0.0, 0.0, 0.0);
    GodotCameraVector3 gaze_origin = GodotCameraVector3(0.0, 0.0, 0.0);
    GodotCameraVector3 gaze_direction = GodotCameraVector3(0.0, 0.0, 1.0);

    bool has_landmarks_2d = false;
    float landmarks_2d_px[35 * 2] = {0.0f};

    // Pointers to output crop image buffers
    uint8_t* left_eye_buffer = nullptr;
    uint8_t* right_eye_buffer = nullptr;
    uint8_t* full_crop_buffer = nullptr;
    size_t full_crop_bytes = 0;

    uint64_t face_context_id = 0;
    uint64_t eye_context_id = 0;
    uint64_t face_rid_val = 0; // Compatibility alias
    uint64_t eye_rid_val = 0;  // Compatibility alias

    // Generic opaque user context pointer
    void* userdata = nullptr;

    // Stage Telemetry Timings (wall-clock milliseconds)
    StageTimings timings;
};

} // namespace Gaze
