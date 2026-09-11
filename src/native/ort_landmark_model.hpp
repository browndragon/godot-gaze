/**
 * @file ort_landmark_model.hpp
 * @brief ONNX Runtime Intel ADAS 35-Point Facial Landmark Regressor Implementation
 */
#pragma once

#include "../core/camera_interface.hpp"
#include "../core/math_defs.hpp"
#include "ort_gaze_model.hpp"
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <vector>
#include <string>

namespace Gaze
{
    class ORTLandmarkModel
    {
    private:
        std::string model_path;
        std::vector<uint8_t> model_buffer;
        bool load_from_buffer = false;

        Ort::SessionOptions session_options;
        std::unique_ptr<Ort::Session> session;
        Ort::MemoryInfo memory_info{nullptr};

        const std::vector<const char *> input_names = {"data"};
        const std::vector<const char *> output_names = {"align_fc3/sink_port_0"};

        void preprocess_face_crop(const uint8_t *raw_crop_bgr, float *out_buffer);

        // Preallocated zero-churn buffers
        std::vector<uint8_t> cached_crop_60;
        std::vector<float> cached_input_tensor;

    public:
        ORTLandmarkModel(const std::string &model_path);
        ORTLandmarkModel(const std::vector<uint8_t> &buffer);
        ~ORTLandmarkModel() = default;

        bool initialize();

        /**
         * @brief Extracts 35 facial landmarks from a 60x60 BGR face crop.
         * @param raw_crop_bgr Pointer to 60x60x3 BGR pixel data.
         * @param out_landmarks_norm Vector of 35 normalized (x, y) coordinates in [0, 1] relative to the face crop.
         * @return True if inference succeeded.
         */
        bool extract_landmarks_norm(const uint8_t *raw_crop_bgr, std::vector<GodotCameraImageVector2> &out_landmarks_norm);

        /**
         * @brief Crops the face ROI from the full image, resizes to 60x60, and extracts 35 landmarks mapped back to original image space.
         * @param src_data Full frame BGR image buffer.
         * @param img_w Image width.
         * @param img_h Image height.
         * @param face_bbox Face bounding box (x, y, w, h).
         * @param out_landmarks_px Vector of 35 (x, y) coordinates in pixel space.
         * @param roll_hint_rad Optional head roll angle in radians to counter-rotate before cropping.
         * @return True if successful.
         */
        bool extract_landmarks(const uint8_t *src_data, int img_w, int img_h, const GazeRect &face_bbox, std::vector<GodotCameraImageVector2> &out_landmarks_px, float roll_hint_rad = 0.0f);
        bool extract_landmarks(const uint8_t *src_data, int img_w, int img_h, const GazeRect &face_bbox, std::vector<SpacedVector2<Space::GodotCameraWorkingImagePixels>> &out_landmarks_px, float roll_hint_rad = 0.0f);
        bool extract_landmarks_working_space(const uint8_t *src_data, int img_w, int img_h, const GazeRect &working_face_bbox, std::vector<SpacedVector2<Space::GodotCameraWorkingImagePixels>> &out_landmarks_working_px, float roll_hint_rad);
    };

} // namespace Gaze
