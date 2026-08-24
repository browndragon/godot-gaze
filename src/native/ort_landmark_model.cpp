/**
 * @file ort_landmark_model.cpp
 * @brief ONNX Runtime Intel ADAS 35-Point Facial Landmark Regressor Implementation
 */
#include "ort_landmark_model.hpp"
#include "platform_ort.hpp"
#include "../core/log.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Gaze
{


    ORTLandmarkModel::ORTLandmarkModel(const std::string &p_model_path)
        : model_path(p_model_path), load_from_buffer(false), memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    {
    }

    ORTLandmarkModel::ORTLandmarkModel(const std::vector<uint8_t> &buffer)
        : model_buffer(buffer), load_from_buffer(true), memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    {
    }

    bool ORTLandmarkModel::initialize()
    {
        try
        {
            session_options.SetIntraOpNumThreads(1);
            session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

            if (load_from_buffer)
            {
                if (model_buffer.empty())
                {
                    log_error("ORTLandmarkModelInitFailed", "reason", "Buffer is empty");
                    return false;
                }
                session = platform_create_ort_session(get_ort_env(), model_buffer, &session_options);
            }
            else
            {
                if (model_path.empty())
                {
                    log_error("ORTLandmarkModelInitFailed", "reason", "Model path is empty");
                    return false;
                }
                session = platform_create_ort_session(get_ort_env(), model_path, &session_options);
            }

            if (!session)
            {
                log_error("ORTLandmarkModelSessionNull");
                return false;
            }

            log_info("ORTLandmarkModelInitSuccess");
            return true;
        }
        catch (const std::exception &e)
        {
            log_error("ORTLandmarkModelInitException", "what", e.what());
            return false;
        }
    }

    void ORTLandmarkModel::preprocess_face_crop(const uint8_t *raw_crop_bgr, float *out_buffer)
    {
        // 60x60 BGR format [1, 3, 60, 60] with raw float pixel intensities [0.0, 255.0]
        constexpr int width = 60;
        constexpr int height = 60;
        constexpr int plane_size = width * height;

        float *b_plane = out_buffer;
        float *g_plane = out_buffer + plane_size;
        float *r_plane = out_buffer + 2 * plane_size;

        for (int i = 0; i < plane_size; ++i)
        {
            b_plane[i] = static_cast<float>(raw_crop_bgr[i * 3 + 0]);
            g_plane[i] = static_cast<float>(raw_crop_bgr[i * 3 + 1]);
            r_plane[i] = static_cast<float>(raw_crop_bgr[i * 3 + 2]);
        }
    }

    bool ORTLandmarkModel::extract_landmarks_norm(const uint8_t *raw_crop_bgr, std::vector<GazeVector2> &out_landmarks_norm)
    {
        out_landmarks_norm.clear();
        if (!session || !raw_crop_bgr)
        {
            return false;
        }

        try
        {
            std::vector<float> input_tensor_data(3 * 60 * 60);
            preprocess_face_crop(raw_crop_bgr, input_tensor_data.data());

            std::vector<int64_t> input_shape = {1, 3, 60, 60};
            Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
                memory_info, input_tensor_data.data(), input_tensor_data.size(),
                input_shape.data(), input_shape.size());

            auto output_tensors = session->Run(
                Ort::RunOptions{nullptr},
                input_names.data(), &input_tensor, 1,
                output_names.data(), output_names.size());

            if (output_tensors.empty())
            {
                return false;
            }

            const float *raw_output = output_tensors[0].GetTensorData<float>();
            out_landmarks_norm.resize(35);
            for (int i = 0; i < 35; ++i)
            {
                out_landmarks_norm[i] = GazeVector2(raw_output[i * 2 + 0], raw_output[i * 2 + 1]);
            }

            return true;
        }
        catch (const std::exception &e)
        {
            log_error("ORTLandmarkModelRunException", "what", e.what());
            return false;
        }
    }

    static GazeRect adjust_bounding_box(const GazeRect &bbox)
    {
        float bx = bbox.x - 0.067f * bbox.width;
        float by = bbox.y - 0.028f * bbox.height;
        float bw = bbox.width * 1.15f;
        float bh = bbox.height * 1.13f;

        if (bw < bh)
        {
            float dx = bh - bw;
            bx -= dx * 0.5f;
            bw = bh;
        }
        else
        {
            float dy = bw - bh;
            by -= dy * 0.5f;
            bh = bw;
        }
        return GazeRect(bx, by, bw, bh);
    }

    bool ORTLandmarkModel::extract_landmarks(const uint8_t *src_data, int img_w, int img_h, const GazeRect &face_bbox, std::vector<GazeVector2> &out_landmarks_px, float roll_hint_rad)
    {
        out_landmarks_px.clear();
        if (!src_data || img_w <= 0 || img_h <= 0 || face_bbox.width < 20.0f || face_bbox.height < 20.0f)
        {
            return false;
        }

        float aspect_ratio = face_bbox.width / face_bbox.height;
        if (aspect_ratio < 0.3f || aspect_ratio > 3.0f)
        {
            return false;
        }

        const unsigned char *working_data = src_data;
        std::vector<unsigned char> working_buffer;
        GazeRect working_bbox = face_bbox;

        if (std::abs(roll_hint_rad) > 1e-4f)
        {
            working_buffer.resize(img_w * img_h * 3);
            rotate_image_bgr(src_data, img_w, img_h, working_buffer.data(), -roll_hint_rad);
            working_data = working_buffer.data();

            float cx = face_bbox.x + face_bbox.width * 0.5f;
            float cy = face_bbox.y + face_bbox.height * 0.5f;
            GazeVector2 rot_center = rotate_point_back(GazeVector2(cx, cy), roll_hint_rad, img_w, img_h);
            working_bbox = GazeRect(rot_center.x - face_bbox.width * 0.5f, rot_center.y - face_bbox.height * 0.5f, face_bbox.width, face_bbox.height);
        }

        GazeRect adj_box = adjust_bounding_box(working_bbox);

        // Crop adjusted face bounding box to 60x60 BGR
        std::vector<uint8_t> crop_60(60 * 60 * 3);
        crop_and_resize_bgr(
            working_data, img_w, img_h,
            adj_box.x, adj_box.y, adj_box.width, adj_box.height,
            crop_60.data(), 60, 60);

        std::vector<GazeVector2> landmarks_norm;
        if (!extract_landmarks_norm(crop_60.data(), landmarks_norm))
        {
            return false;
        }

        out_landmarks_px.resize(35);
        for (size_t i = 0; i < 35; ++i)
        {
            float px_x = adj_box.x + landmarks_norm[i].x * adj_box.width;
            float px_y = adj_box.y + landmarks_norm[i].y * adj_box.height;
            GazeVector2 pt(px_x, px_y);
            if (std::abs(roll_hint_rad) > 1e-4f)
            {
                pt = rotate_point_back(pt, -roll_hint_rad, img_w, img_h);
            }
            out_landmarks_px[i] = pt;
        }

        return true;
    }

} // namespace Gaze
