/**
 * @file face_pipeline.hpp
 * @brief Face detection, landmarking, and normalizer interface (Layer 2)
 *
 * Defines the abstract interface and data structures for detecting faces,
 * calculating facial landmarks, estimating head pose transformations, and
 * extracting normalized roll/scale eye crop buffers.
 */
#pragma once

#include <memory>
#include <cstdint>
#include "camera_interface.hpp"
#include "math_defs.hpp"
#include "pipeline_config.hpp"

namespace Gaze
{

    class ImageWarper
    {
    public:
        virtual ~ImageWarper() = default;
        
        // Warps a patch around the eye center using bilinear interpolation on the CPU,
        // and converts the source (RGB/BGR/Gray) image data to a normalized BGR output buffer.
        virtual bool warp(
            const uint8_t *src_data,
            int src_width,
            int src_height,
            int src_channels,
            const GazeVector2 &eye_center,
            double angle_deg,
            double scale,
            uint8_t *out_bgr_buffer) = 0;

        virtual void submit_and_sync() {}
        virtual void increment_frame_number() {}
    };

    struct EyeCrops
    {
        static constexpr int EYE_CROP_WIDTH = 60;
        static constexpr int EYE_CROP_HEIGHT = 60;
        static constexpr int EYE_CROP_CHANNELS = 3;
        static constexpr int EYE_CROP_SIZE = EYE_CROP_WIDTH * EYE_CROP_HEIGHT * EYE_CROP_CHANNELS; // 10800 bytes

        bool face_detected = false;
        GazePoint landmarks[5];

        // Estimated head pose vectors relative to camera (in mm and radians)
        GazeVector3 head_pose_rotation;    // Rotational angles (pitch, yaw, roll)
        GazeVector3 head_pose_translation; // Position of the head

        // Left and right eye crop buffers (60x60 px, 3-channel BGR)
        uint8_t left_eye_data[EYE_CROP_SIZE] = {0};
        uint8_t right_eye_data[EYE_CROP_SIZE] = {0};

        // 3D coordinates of left and right eye centers in camera space (in mm)
        // Used to calculate Z distance based on IPD (Interpupillary Distance)
        GazeVector3 left_eye_center_cam;
        GazeVector3 right_eye_center_cam;

        // TODO: Does it make sense to have both a texture_handle, rid_val, _AND_ our own inline storage buffer? I'd think if we're using handles, they'd provide the storage location? IDK.
        uint64_t left_eye_texture_handle = 0;
        uint64_t right_eye_texture_handle = 0;
        uint64_t left_eye_texture_rid_val = 0;
        uint64_t right_eye_texture_rid_val = 0;
    };

    class FacePipeline
    {
    protected:
        std::shared_ptr<ImageWarper> warper;

    public:
        virtual ~FacePipeline() = default;

        // Load models (e.g. YuNet face detector) and initialize pipeline
        virtual bool initialize() = 0;

        // Process frame, detect faces, estimate head pose, and crop eyes
        virtual bool process_frame(const Frame &frame, EyeCrops &out_crops) = 0;

        // Configure camera focal length
        virtual void set_camera_focal_length_px(double f) {}

        // Configure camera horizontal field of view in degrees
        virtual void set_camera_fov_degrees(double fov) {}

        // Configure pipeline settings
        virtual void set_config(const PipelineConfig &config) {}

        virtual void set_image_warper(std::shared_ptr<ImageWarper> p_warper) { warper = p_warper; }
        virtual std::shared_ptr<ImageWarper> get_image_warper() const { return warper; }
    };

} // namespace Gaze
