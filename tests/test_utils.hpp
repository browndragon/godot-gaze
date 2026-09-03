#pragma once

#include "math_defs.hpp"
#include "stb_image.h"

#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace GazeTest
{

    struct TestImage
    {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> data;
        bool valid() const { return !data.empty() && width > 0 && height > 0; }
    };

    inline bool file_exists(const std::string &filename)
    {
        std::ifstream f(filename.c_str());
        return f.good();
    }

    inline std::string resolve_test_path(const std::string &filepath)
    {
        if (file_exists(filepath)) return filepath;
        std::string fallback1 = "../" + filepath;
        if (file_exists(fallback1)) return fallback1;
        std::string fallback2 = "../../" + filepath;
        if (file_exists(fallback2)) return fallback2;
        return filepath;
    }

    inline TestImage load_test_image(const std::string &filepath)
    {
        TestImage img;
        std::string resolved = resolve_test_path(filepath);
        int w = 0, h = 0, c = 0;
        unsigned char *data = stbi_load(resolved.c_str(), &w, &h, &c, 3);
        if (!data) return img;

        img.width = w;
        img.height = h;
        img.data.resize(w * h * 3);
        for (int i = 0; i < w * h; ++i)
        {
            img.data[i * 3 + 0] = data[i * 3 + 2]; // B
            img.data[i * 3 + 1] = data[i * 3 + 1]; // G
            img.data[i * 3 + 2] = data[i * 3 + 0]; // R
        }
        stbi_image_free(data);
        return img;
    }

    inline void extract_dense_eye_crops_60x60(
        const uint8_t *frame_bgr, int width, int height,
        const std::vector<Gaze::GodotCameraImageVector2> &landmarks_35,
        uint8_t *out_right_crop_bgr, uint8_t *out_left_crop_bgr,
        float scale_factor = 1.5f)
    {
        // In OpenVINO ADAS 35-point landmarks:
        // pts 0..1 = Image Left Eye (Anatomical Right Eye)
        // pts 2..3 = Image Right Eye (Anatomical Left Eye)
        float r_cx = (landmarks_35[0].x + landmarks_35[1].x) * 0.5f;
        float r_cy = (landmarks_35[0].y + landmarks_35[1].y) * 0.5f;
        float r_dx = landmarks_35[0].x - landmarks_35[1].x;
        float r_dy = landmarks_35[0].y - landmarks_35[1].y;
        float r_w = std::sqrt(r_dx * r_dx + r_dy * r_dy);

        float l_cx = (landmarks_35[2].x + landmarks_35[3].x) * 0.5f;
        float l_cy = (landmarks_35[2].y + landmarks_35[3].y) * 0.5f;
        float l_dx = landmarks_35[2].x - landmarks_35[3].x;
        float l_dy = landmarks_35[2].y - landmarks_35[3].y;
        float l_w = std::sqrt(l_dx * l_dx + l_dy * l_dy);

        float r_box_s = std::max(20.0f, r_w * scale_factor);
        float l_box_s = std::max(20.0f, l_w * scale_factor);

        Gaze::crop_and_resize_bgr(
            frame_bgr, width, height,
            r_cx - r_box_s * 0.5f, r_cy - r_box_s * 0.5f, r_box_s, r_box_s,
            out_right_crop_bgr, 60, 60
        );

        Gaze::crop_and_resize_bgr(
            frame_bgr, width, height,
            l_cx - l_box_s * 0.5f, l_cy - l_box_s * 0.5f, l_box_s, l_box_s,
            out_left_crop_bgr, 60, 60
        );
    }

} // namespace GazeTest
