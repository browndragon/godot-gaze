/**
 * @file ort_yunet_detector.cpp
 * @brief Native ONNX Runtime YuNet 5-Keypoint Face Detector Implementation
 */
#include "ort_yunet_detector.hpp"
#include "platform_ort.hpp"
#include "../core/cpu_image_warper.hpp"
#include "../core/pnp_solver.hpp"
#include "../core/log.hpp"
#include "../core/face_model_geometry.hpp"
#include "ort_gaze_model.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <cstring>

namespace Gaze
{
    static void bilinear_resize_direct(
        const unsigned char *src, int src_w, int src_h,
        unsigned char *dst, int dst_w, int dst_h)
    {
        if (dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0) return;

        float scale_x = static_cast<float>(src_w) / dst_w;
        float scale_y = static_cast<float>(src_h) / dst_h;

        for (int y = 0; y < dst_h; ++y)
        {
            float src_y = (y + 0.5f) * scale_y - 0.5f;
            src_y = std::max(0.0f, std::min(src_y, static_cast<float>(src_h - 1)));
            int y0 = static_cast<int>(std::floor(src_y));
            int y1 = std::min(y0 + 1, src_h - 1);
            float dy = src_y - y0;
            for (int x = 0; x < dst_w; ++x)
            {
                float src_x = (x + 0.5f) * scale_x - 0.5f;
                src_x = std::max(0.0f, std::min(src_x, static_cast<float>(src_w - 1)));
                int x0 = static_cast<int>(std::floor(src_x));
                int x1 = std::min(x0 + 1, src_w - 1);
                float dx = src_x - x0;
                for (int c = 0; c < 3; ++c)
                {
                    float corners[4] = {
                        static_cast<float>(src[(y0 * src_w + x0) * 3 + c]),
                        static_cast<float>(src[(y0 * src_w + x1) * 3 + c]),
                        static_cast<float>(src[(y1 * src_w + x0) * 3 + c]),
                        static_cast<float>(src[(y1 * src_w + x1) * 3 + c])
                    };
                    float val = (1.0f - dx) * (1.0f - dy) * corners[0] +
                                dx * (1.0f - dy) * corners[1] +
                                (1.0f - dx) * dy * corners[2] +
                                dx * dy * corners[3];
                    dst[(y * dst_w + x) * 3 + c] =
                        static_cast<unsigned char>(std::max(0.0f, std::min(255.0f, val)));
                }
            }
        }
    }

    static void rotate_image_bgr(const unsigned char *src, int w, int h, unsigned char *dst, float angle_rad)
    {
        float cos_a = std::cos(angle_rad);
        float sin_a = std::sin(angle_rad);
        float cx = w / 2.0f;
        float cy = h / 2.0f;

        for (int y = 0; y < h; ++y)
        {
            float dy = y - cy;
            for (int x = 0; x < w; ++x)
            {
                float dx = x - cx;
                float src_x = cx + dx * cos_a + dy * sin_a;
                float src_y = cy - dx * sin_a + dy * cos_a;

                int dst_idx = (y * w + x) * 3;
                if (src_x >= 0.0f && src_x < w - 1.0f && src_y >= 0.0f && src_y < h - 1.0f)
                {
                    int x0 = static_cast<int>(std::floor(src_x));
                    int y0 = static_cast<int>(std::floor(src_y));
                    int x1 = x0 + 1;
                    int y1 = y0 + 1;
                    float tx = src_x - x0;
                    float ty = src_y - y0;

                    for (int c = 0; c < 3; ++c)
                    {
                        float p00 = src[(y0 * w + x0) * 3 + c];
                        float p10 = src[(y0 * w + x1) * 3 + c];
                        float p01 = src[(y1 * w + x0) * 3 + c];
                        float p11 = src[(y1 * w + x1) * 3 + c];

                        float val = (1.0f - tx) * (1.0f - ty) * p00 +
                                    tx * (1.0f - ty) * p10 +
                                    (1.0f - tx) * ty * p01 +
                                    tx * ty * p11;
                        dst[dst_idx + c] = static_cast<unsigned char>(std::max(0.0f, std::min(255.0f, val)));
                    }
                }
                else
                {
                    dst[dst_idx + 0] = 0;
                    dst[dst_idx + 1] = 0;
                    dst[dst_idx + 2] = 0;
                }
            }
        }
    }

    static GazeVector2 rotate_point_back(const GazeVector2 &pt, float angle_rad, int w, int h)
    {
        if (std::abs(angle_rad) < 1e-4f) return pt;
        float inv_angle = -angle_rad;
        float cos_a = std::cos(inv_angle);
        float sin_a = std::sin(inv_angle);
        float cx = w / 2.0f;
        float cy = h / 2.0f;
        float dx = pt.x - cx;
        float dy = pt.y - cy;
        return GazeVector2(cx + dx * cos_a - dy * sin_a, cy + dx * sin_a + dy * cos_a);
    }

    ORTYuNetDetector::ORTYuNetDetector(const std::string &p_model_path, float score_thresh, float nms_thresh)
        : model_path(p_model_path), score_threshold(score_thresh), nms_threshold(nms_thresh),
          memory_info(Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU))
    {
    }

    ORTYuNetDetector::ORTYuNetDetector(const std::vector<uint8_t> &p_model_buffer, float score_thresh, float nms_thresh)
        : model_buffer(p_model_buffer), load_from_buffer(true), score_threshold(score_thresh), nms_threshold(nms_thresh),
          memory_info(Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU))
    {
    }

    bool ORTYuNetDetector::initialize()
    {
        try
        {
            get_ort_env();
            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(1);
            session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

            if (load_from_buffer)
            {
                log_info("ORTYuNetDetectorInitBuffer", "size", (int)model_buffer.size());
                session = platform_create_ort_session(get_ort_env(), model_buffer, &session_options);
            }
            else
            {
                log_info("ORTYuNetDetectorInitPath", "model_path", model_path);
                session = platform_create_ort_session(get_ort_env(), model_path, &session_options);
            }

            if (!session)
            {
                log_error("ORTYuNetDetectorSessionNull");
                return false;
            }

            try
            {
                auto type_info = session->GetInputTypeInfo(0);
                auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
                auto shape = tensor_info.GetShape();
                if (shape.size() == 4)
                {
                    if (shape[1] == 3)
                    {
                        is_nhwc = false;
                        if (shape[2] > 0) input_height = static_cast<int>(shape[2]);
                        if (shape[3] > 0) input_width = static_cast<int>(shape[3]);
                    }
                    else if (shape[3] == 3)
                    {
                        is_nhwc = true;
                        if (shape[1] > 0) input_height = static_cast<int>(shape[1]);
                        if (shape[2] > 0) input_width = static_cast<int>(shape[2]);
                    }
                }
                log_info("ORTYuNetDetectorInputShape", "width", input_width, "height", input_height, "is_nhwc", is_nhwc);
            }
            catch (...) {}
        }
        catch (const std::exception &e)
        {
            log_error("ORTYuNetDetectorInitException", "what", e.what());
            return false;
        }
        log_info("ORTYuNetDetectorInitSuccess");
        return true;
    }

    void ORTYuNetDetector::set_config(const PipelineConfig &p_config)
    {
        config = p_config;
    }

    std::vector<GazeVector3> ORTYuNetDetector::get_canonical_godot_model_points() const
    {
        return FaceModelGeometry::get_model_points();
    }

    std::vector<ORTYuNetDetector::Anchor> ORTYuNetDetector::generate_anchors(int width, int height)
    {
        std::vector<Anchor> local_anchors;
        std::vector<int> strides = {8, 16, 32};

        for (size_t s = 0; s < strides.size(); ++s)
        {
            int stride = strides[s];
            int feature_w = std::ceil(static_cast<float>(width) / stride);
            int feature_h = std::ceil(static_cast<float>(height) / stride);

            for (int i = 0; i < feature_h; ++i)
            {
                for (int j = 0; j < feature_w; ++j)
                {
                    Anchor anchor;
                    anchor.cx = static_cast<float>(j) * stride;
                    anchor.cy = static_cast<float>(i) * stride;
                    anchor.stride_x = static_cast<float>(stride);
                    anchor.stride_y = static_cast<float>(stride);
                    local_anchors.push_back(anchor);
                }
            }
        }
        return local_anchors;
    }

    bool ORTYuNetDetector::process_frame_single_pass(const Frame &frame, YuNetResult &out_result, float roll_deg)
    {
        out_result = YuNetResult();
        if (!session || frame.data == nullptr || frame.width <= 0 || frame.height <= 0)
        {
            return false;
        }

        int width = frame.width;
        int height = frame.height;
        int model_w = input_width;
        int model_h = input_height;

        float roll_rad = roll_deg * (3.141592653589793f / 180.0f);
        std::vector<unsigned char> frame_bgr;
        const unsigned char *src_data = frame.data;

        if (std::abs(roll_deg) > 1e-3f)
        {
            frame_bgr.resize(width * height * 3);
            rotate_image_bgr(frame.data, width, height, frame_bgr.data(), -roll_rad);
            src_data = frame_bgr.data();
        }

        float scale = std::min(static_cast<float>(model_w) / width, static_cast<float>(model_h) / height);
        float new_w = static_cast<float>(width) * scale;
        float new_h = static_cast<float>(height) * scale;
        float pad_x = (model_w - new_w) / 2.0f;
        float pad_y = (model_h - new_h) / 2.0f;

        int int_new_w = static_cast<int>(std::round(new_w));
        int int_new_h = static_cast<int>(std::round(new_h));
        int int_pad_x = static_cast<int>(std::round(pad_x));
        int int_pad_y = static_cast<int>(std::round(pad_y));

        std::vector<Anchor> anchors = generate_anchors(model_w, model_h);

        std::vector<float> input_tensor_data(1 * 3 * model_h * model_w, 0.0f);
        std::vector<unsigned char> resized_bgr(int_new_w * int_new_h * 3, 0);
        bilinear_resize_direct(src_data, width, height, resized_bgr.data(), int_new_w, int_new_h);

        std::vector<unsigned char> padded_bgr(model_w * model_h * 3, 0);
        for (int y = 0; y < int_new_h; ++y)
        {
            int dst_y = y + int_pad_y;
            if (dst_y >= model_h) continue;
            for (int x = 0; x < int_new_w; ++x)
            {
                int dst_x = x + int_pad_x;
                if (dst_x >= model_w) continue;
                int src_idx = (y * int_new_w + x) * 3;
                int dst_idx = (dst_y * model_w + dst_x) * 3;
                padded_bgr[dst_idx + 0] = resized_bgr[src_idx + 0];
                padded_bgr[dst_idx + 1] = resized_bgr[src_idx + 1];
                padded_bgr[dst_idx + 2] = resized_bgr[src_idx + 2];
            }
        }

        int channel_size = model_h * model_w;
        if (is_nhwc)
        {
            for (int i = 0; i < channel_size * 3; ++i)
            {
                input_tensor_data[i] = static_cast<float>(padded_bgr[i]);
            }
        }
        else
        {
            for (int y = 0; y < model_h; ++y)
            {
                for (int x = 0; x < model_w; ++x)
                {
                    int pixel_idx = y * model_w + x;
                    input_tensor_data[pixel_idx] = static_cast<float>(padded_bgr[pixel_idx * 3 + 0]);
                    input_tensor_data[channel_size + pixel_idx] = static_cast<float>(padded_bgr[pixel_idx * 3 + 1]);
                    input_tensor_data[2 * channel_size + pixel_idx] = static_cast<float>(padded_bgr[pixel_idx * 3 + 2]);
                }
            }
        }

        std::vector<int64_t> input_shape = is_nhwc ? std::vector<int64_t>{1, model_h, model_w, 3} : std::vector<int64_t>{1, 3, model_h, model_w};
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, input_tensor_data.data(), input_tensor_data.size(),
            input_shape.data(), input_shape.size());

        try
        {
            std::vector<Ort::Value> inputs;
            inputs.push_back(std::move(input_tensor));

            auto outputs = session->Run(
                Ort::RunOptions{nullptr},
                input_names.data(),
                inputs.data(),
                inputs.size(),
                output_names.data(),
                output_names.size());

            if (outputs.size() < 12)
            {
                return false;
            }

            std::vector<GazeRect> candidate_bboxes;
            std::vector<float> candidate_scores;
            std::vector<std::vector<GazeVector2>> candidate_landmarks;

            size_t anchor_offset = 0;
            std::vector<int> strides = {8, 16, 32};

            for (size_t s = 0; s < strides.size(); ++s)
            {
                Ort::Value &cls_tensor = outputs[s];
                Ort::Value &obj_tensor = outputs[3 + s];
                Ort::Value &bbox_tensor = outputs[6 + s];
                Ort::Value &kps_tensor = outputs[9 + s];

                auto cls_shape = cls_tensor.GetTensorTypeAndShapeInfo().GetShape();
                size_t num_anchors_s = cls_shape[1];

                float *cls_data = cls_tensor.GetTensorMutableData<float>();
                float *obj_data = obj_tensor.GetTensorMutableData<float>();
                float *bbox_data = bbox_tensor.GetTensorMutableData<float>();
                float *kps_data = kps_tensor.GetTensorMutableData<float>();

                for (size_t idx = 0; idx < num_anchors_s; ++idx)
                {
                    float score = cls_data[idx] * obj_data[idx];
                    if (score > score_threshold)
                    {
                        size_t global_idx = anchor_offset + idx;
                        if (global_idx >= anchors.size()) continue;

                        const auto &anchor = anchors[global_idx];

                        float cx = (bbox_data[idx * 4 + 0] * anchor.stride_x + anchor.cx);
                        float cy = (bbox_data[idx * 4 + 1] * anchor.stride_y + anchor.cy);
                        float w = std::exp(bbox_data[idx * 4 + 2]) * anchor.stride_x;
                        float h = std::exp(bbox_data[idx * 4 + 3]) * anchor.stride_y;

                        float x_left = cx - w / 2.0f;
                        float y_top = cy - h / 2.0f;

                        std::vector<GazeVector2> ldm(5);
                        for (int j = 0; j < 5; ++j)
                        {
                            float kx = (kps_data[idx * 10 + j * 2 + 0] * anchor.stride_x + anchor.cx);
                            float ky = (kps_data[idx * 10 + j * 2 + 1] * anchor.stride_y + anchor.cy);

                            float rot_kx = (kx - pad_x) / scale;
                            float rot_ky = (ky - pad_y) / scale;

                            ldm[j] = rotate_point_back(GazeVector2(rot_kx, rot_ky), -roll_rad, width, height);
                        }

                        float orig_x = (x_left - pad_x) / scale;
                        float orig_y = (y_top - pad_y) / scale;
                        float orig_w = w / scale;
                        float orig_h = h / scale;

                        candidate_bboxes.push_back(GazeRect(orig_x, orig_y, orig_w, orig_h));
                        candidate_scores.push_back(score);
                        candidate_landmarks.push_back(ldm);
                    }
                }
                anchor_offset += num_anchors_s;
            }

            if (candidate_bboxes.empty())
            {
                return false;
            }

            std::vector<size_t> indices(candidate_scores.size());
            std::iota(indices.begin(), indices.end(), 0);
            std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b)
                      { return candidate_scores[a] > candidate_scores[b]; });

            std::vector<bool> suppressed(candidate_scores.size(), false);
            int best_idx = -1;

            for (size_t i = 0; i < indices.size(); ++i)
            {
                size_t idx = indices[i];
                if (suppressed[idx]) continue;

                if (best_idx < 0) best_idx = static_cast<int>(idx);

                for (size_t j = i + 1; j < indices.size(); ++j)
                {
                    size_t idx2 = indices[j];
                    if (suppressed[idx2]) continue;

                    GazeRect r1 = candidate_bboxes[idx];
                    GazeRect r2 = candidate_bboxes[idx2];

                    float inter_x1 = std::max(r1.x, r2.x);
                    float inter_y1 = std::max(r1.y, r2.y);
                    float inter_x2 = std::min(r1.x + r1.width, r2.x + r2.width);
                    float inter_y2 = std::min(r1.y + r1.height, r2.y + r2.height);
                    float inter_w = std::max(0.0f, inter_x2 - inter_x1);
                    float inter_h = std::max(0.0f, inter_y2 - inter_y1);
                    float inter_area = inter_w * inter_h;

                    float union_area = r1.area() + r2.area() - inter_area;
                    float iou = (union_area > 0.0f) ? (inter_area / union_area) : 0.0f;

                    if (iou > nms_threshold)
                    {
                        suppressed[idx2] = true;
                    }
                }
            }

            if (best_idx < 0) return false;

            out_result.face_detected = true;
            out_result.score = candidate_scores[best_idx];
            out_result.roi_x = candidate_bboxes[best_idx].x;
            out_result.roi_y = candidate_bboxes[best_idx].y;
            out_result.roi_w = candidate_bboxes[best_idx].width;
            out_result.roi_h = candidate_bboxes[best_idx].height;

            const auto &ldm = candidate_landmarks[best_idx];
            out_result.right_eye_px = ldm[0];
            out_result.left_eye_px = ldm[1];
            out_result.nose_tip_px = ldm[2];
            out_result.mouth_right_px = ldm[3];
            out_result.mouth_left_px = ldm[4];

            std::vector<GazeVector3> model_pts = get_canonical_godot_model_points();
            std::vector<GazeVector2> img_pts = {
                out_result.nose_tip_px,
                out_result.right_eye_px,
                out_result.left_eye_px,
                out_result.mouth_right_px,
                out_result.mouth_left_px
            };

            double focal = std::max(width, height) * 1.5;
            double cx = width / 2.0;
            double cy = height / 2.0;

            GazeVector3 rvec(0, 0, 0);
            GazeVector3 tvec(0, 0, 600.0);
            solve_pnp_lm(model_pts, img_pts, focal, focal, cx, cy, rvec, tvec, false);

            out_result.head_pose.pitch_rad = static_cast<float>(rvec.x);
            out_result.head_pose.yaw_rad = static_cast<float>(rvec.y);
            out_result.head_pose.roll_rad = static_cast<float>(rvec.z);
            out_result.head_pose.trans_x_mm = static_cast<float>(tvec.x);
            out_result.head_pose.trans_y_mm = static_cast<float>(tvec.y);
            out_result.head_pose.trans_z_mm = static_cast<float>(tvec.z);

            crop_and_resize_bgr_to_rgb(
                frame.data, width, height,
                out_result.left_eye_px.x - 30.0f, out_result.left_eye_px.y - 30.0f, 60.0f, 60.0f,
                out_result.left_eye_crop, 60, 60
            );

            crop_and_resize_bgr_to_rgb(
                frame.data, width, height,
                out_result.right_eye_px.x - 30.0f, out_result.right_eye_px.y - 30.0f, 60.0f, 60.0f,
                out_result.right_eye_crop, 60, 60
            );

            return true;
        }
        catch (const std::exception &e)
        {
            log_error("ORTYuNetDetectorProcessException", "what", e.what());
            return false;
        }
    }

    bool ORTYuNetDetector::process_frame(const Frame &frame, YuNetResult &out_result, float roll_hint_rad)
    {
        float hint_deg = roll_hint_rad * (180.0f / 3.141592653589793f);
        return process_frame_single_pass(frame, out_result, hint_deg);
    }

} // namespace Gaze
