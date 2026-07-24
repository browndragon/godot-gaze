#include "ort_yunet_pipeline.hpp"
#include "ort_gaze_model.hpp"
#include "log.hpp"
#include "face_model_geometry.hpp"
#include "cpu_image_warper.hpp"
#include "pnp_solver.hpp"
#include "platform_ort.hpp"
// OpenCV removed
#include <cmath>
#include <numeric>
#include <algorithm>

namespace Gaze
{

    // TODO: Move to a math file with math-specific tests, this isn't specific to this pipeline.
    static void bilinear_resize_letterbox(
        const unsigned char *src, int src_w, int src_h,
        unsigned char *dst, int dst_w, int dst_h,
        int pad_x, int pad_y, int new_w, int new_h)
    {
        std::memset(dst, 0, dst_w * dst_h * 3);
        for (int y = 0; y < new_h; ++y)
        {
            float src_y = (y + 0.5f) * (static_cast<float>(src_h) / new_h) - 0.5f;
            src_y = std::max(0.0f, std::min(src_y, static_cast<float>(src_h - 1)));
            int y0 = static_cast<int>(std::floor(src_y));
            int y1 = std::min(y0 + 1, src_h - 1);
            float dy = src_y - y0;
            for (int x = 0; x < new_w; ++x)
            {
                float src_x = (x + 0.5f) * (static_cast<float>(src_w) / new_w) - 0.5f;
                src_x = std::max(0.0f, std::min(src_x, static_cast<float>(src_w - 1)));
                int x0 = static_cast<int>(std::floor(src_x));
                int x1 = std::min(x0 + 1, src_w - 1);
                float dx = src_x - x0;
                for (int c = 0; c < 3; ++c)
                {
                    float corners[4] = {
                        static_cast<float>(src[(y0 * src_w + x0) * 3 + c]), // Top-Left
                        static_cast<float>(src[(y0 * src_w + x1) * 3 + c]), // Top-Right
                        static_cast<float>(src[(y1 * src_w + x0) * 3 + c]), // Bottom-Left
                        static_cast<float>(src[(y1 * src_w + x1) * 3 + c])  // Bottom-Right
                    };
                    float val = (1.0f - dx) * (1.0f - dy) * corners[0] +
                                dx * (1.0f - dy) * corners[1] +
                                (1.0f - dx) * dy * corners[2] +
                                dx * dy * corners[3];
                    dst[((y + pad_y) * dst_w + (x + pad_x)) * 3 + c] =
                        static_cast<unsigned char>(std::max(0.0f, std::min(255.0f, val)));
                }
            }
        }
    }

    ORTYuNetPipeline::ORTYuNetPipeline(const std::string &yunet_model_path, float score_thresh, float nms_thresh, int k)
        : model_path(yunet_model_path), score_threshold(score_thresh), nms_threshold(nms_thresh), top_k(k) {}

    ORTYuNetPipeline::ORTYuNetPipeline(const std::vector<uint8_t> &buffer, float score_thresh, float nms_thresh, int k)
        : model_buffer(buffer), load_from_buffer(true), score_threshold(score_thresh), nms_threshold(nms_thresh), top_k(k) {}

    bool ORTYuNetPipeline::initialize()
    {
        try
        {
            get_ort_env();
            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(1);
            session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

            if (load_from_buffer)
            {
                log_info("ORTYuNetPipelineInitAttemptBuffer", "size", (int)model_buffer.size());
                session = platform_create_ort_session(get_ort_env(), model_buffer, &session_options);
            }
            else
            {
                log_info("ORTYuNetPipelineInitAttempt", "model_path", model_path);
                session = platform_create_ort_session(get_ort_env(), model_path, &session_options);
            }

            if (!session)
            {
                log_error("ORTYuNetPipelineSessionNull");
                return false;
            }
        }
        catch (...)
        {
            log_warning("ORTYuNetPipelineEPFailed", "reason", "EP initialization threw exception, falling back to CPU");
            try
            {
                Ort::SessionOptions session_options;
                session_options.SetIntraOpNumThreads(1);
                session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);
                if (load_from_buffer)
                {
                    session = platform_create_ort_session(get_ort_env(), model_buffer, &session_options);
                }
                else
                {
                    session = platform_create_ort_session(get_ort_env(), model_path, &session_options);
                }
                if (!session)
                {
                    log_error("ORTYuNetPipelineSessionNullCPU");
                    return false;
                }
            }
            catch (const std::exception &e)
            {
                log_error("ORTYuNetPipelineCPUFailed", "what", e.what());
                return false;
            }
            catch (...)
            {
                log_error("ORTYuNetPipelineCPUFailedUnknown");
                return false;
            }
        }
        log_info("ORTYuNetPipelineInitSuccess");
        return true;
    }

    std::vector<ORTYuNetPipeline::Anchor> ORTYuNetPipeline::generate_anchors(int width, int height)
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

    // TODO: Overall, there is too much logic inside of process frame. Break up into helper/phase pipeline-y functions which accomplish each of the tasks they need to accomplish, perhaps in terms of a struct to passalong data?
    //       Unit testing each phase wouldn't be bad either, just saying! :)
    bool ORTYuNetPipeline::process_frame(const Frame &frame, EyeCrops &out_crops)
    {
        // 1. Guard against empty/uninitialized frames or null session
        if (!session || frame.data == nullptr || frame.width <= 0 || frame.height <= 0)
        {
            log_warning("ORTYuNetPipelineInvalidFrame",
                        "width", frame.width,
                        "height", frame.height,
                        "has_data", (frame.data != nullptr));
            out_crops.face_detected = false;
            return false;
        }

        int width = frame.width;
        int height = frame.height;
        int model_w = INFERENCE_WIDTH;
        int model_h = INFERENCE_HEIGHT;

        // Compute scale and padding offsets for letterboxing (preserving aspect ratio)
        float scale = std::min(static_cast<float>(model_w) / width, static_cast<float>(model_h) / height);
        int new_w = static_cast<int>(width * scale);
        int new_h = static_cast<int>(height * scale);
        int pad_x = (model_w - new_w) / 2;
        int pad_y = (model_h - new_h) / 2;

        // 2. Local anchor generation
        std::vector<Anchor> anchors = generate_anchors(model_w, model_h);

        std::vector<float> input_tensor_data(1 * 3 * model_h * model_w);
        std::vector<unsigned char> resized_bgr(model_w * model_h * 3);
        bilinear_resize_letterbox(frame.data, width, height, resized_bgr.data(), model_w, model_h, pad_x, pad_y, new_w, new_h);

        // Apply roll compensation rotation if tracking
        double roll_angle_to_apply = 0.0;
        bool use_roll = tracking_state.is_tracking && (std::abs(tracking_state.last_roll_rad) > 1e-4);
        if (use_roll)
        {
            roll_angle_to_apply = tracking_state.last_roll_rad;
            std::vector<unsigned char> rotated_bgr(model_w * model_h * 3);
            rotate_image_bgr(resized_bgr.data(), model_w, model_h, rotated_bgr.data(), -roll_angle_to_apply);
            resized_bgr = std::move(rotated_bgr);
        }

        int channel_size = model_h * model_w;
        for (int y = 0; y < model_h; ++y)
        {
            for (int x = 0; x < model_w; ++x)
            {
                int src_idx = (y * model_w + x) * 3;
                int dst_idx = y * model_w + x;
                input_tensor_data[dst_idx] = static_cast<float>(resized_bgr[src_idx + 0]);                    // B
                input_tensor_data[channel_size + dst_idx] = static_cast<float>(resized_bgr[src_idx + 1]);     // G
                input_tensor_data[2 * channel_size + dst_idx] = static_cast<float>(resized_bgr[src_idx + 2]); // R
            }
        }

        auto memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
        std::vector<int64_t> input_shape = {1, 3, model_h, model_w};
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, input_tensor_data.data(), input_tensor_data.size(),
            input_shape.data(), input_shape.size());

        // 5. Model Inference
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

            // 6. Shape & Output Count Verification
            if (outputs.size() < 12)
            {
                log_error("YuNetPipelineOutputCountMismatch", "expected", 12, "actual", (int)outputs.size());
                out_crops.face_detected = false;
                return false;
            }

            // 7. Multi-scale stride decoding setup
            std::vector<GazeRect> candidate_bboxes;
            std::vector<float> candidate_scores;
            std::vector<std::vector<GazePoint>> candidate_landmarks;

            auto sigmoid = [](float x)
            {
                return 1.0f / (1.0f + std::exp(-x));
            };

            size_t anchor_offset = 0;
            std::vector<int> strides = {8, 16, 32};

            for (size_t s = 0; s < strides.size(); ++s)
            {
                // Retrieve outputs for stride s
                Ort::Value &cls_tensor = outputs[s];
                Ort::Value &obj_tensor = outputs[3 + s];
                Ort::Value &bbox_tensor = outputs[6 + s];
                Ort::Value &kps_tensor = outputs[9 + s];

                auto cls_shape = cls_tensor.GetTensorTypeAndShapeInfo().GetShape();
                auto obj_shape = obj_tensor.GetTensorTypeAndShapeInfo().GetShape();
                auto bbox_shape = bbox_tensor.GetTensorTypeAndShapeInfo().GetShape();
                auto kps_shape = kps_tensor.GetTensorTypeAndShapeInfo().GetShape();

                // Validate tensor shape dimensions
                if (cls_shape.size() != 3 || obj_shape.size() != 3 || bbox_shape.size() != 3 || kps_shape.size() != 3)
                {
                    log_error("YuNetPipelineTensorRankError", "stride", strides[s]);
                    out_crops.face_detected = false;
                    return false;
                }

                size_t num_anchors_s = cls_shape[1];
                if (obj_shape[1] != num_anchors_s || bbox_shape[1] != num_anchors_s || kps_shape[1] != num_anchors_s)
                {
                    log_error("YuNetPipelineAnchorCountMismatch", "stride", strides[s]);
                    out_crops.face_detected = false;
                    return false;
                }

                if (cls_shape[2] != 1 || obj_shape[2] != 1 || bbox_shape[2] != 4 || kps_shape[2] != 10)
                {
                    log_error("YuNetPipelineChannelMismatch", "stride", strides[s]);
                    out_crops.face_detected = false;
                    return false;
                }

                float *cls_data = cls_tensor.GetTensorMutableData<float>();
                float *obj_data = obj_tensor.GetTensorMutableData<float>();
                float *bbox_data = bbox_tensor.GetTensorMutableData<float>();
                float *kps_data = kps_tensor.GetTensorMutableData<float>();

                for (size_t idx = 0; idx < num_anchors_s; ++idx)
                {
                    float cls_score = cls_data[idx];
                    float obj_score = obj_data[idx];
                    float score = cls_score * obj_score;

                    if (score > score_threshold)
                    {
                        size_t global_idx = anchor_offset + idx;
                        if (global_idx >= anchors.size())
                        {
                            continue; // Bounds check protection
                        }
                        const auto &anchor = anchors[global_idx];

                        // Bounding box regression decoding
                        float cx = (bbox_data[idx * 4 + 0] * anchor.stride_x + anchor.cx);
                        float cy = (bbox_data[idx * 4 + 1] * anchor.stride_y + anchor.cy);
                        float w = std::exp(bbox_data[idx * 4 + 2]) * anchor.stride_x;
                        float h = std::exp(bbox_data[idx * 4 + 3]) * anchor.stride_y;

                        float x_left = cx - w / 2.0f;
                        float y_top = cy - h / 2.0f;

                        // Landmark keypoints decoding
                        std::vector<GazePoint> ldm(5);
                        for (int j = 0; j < 5; ++j)
                        {
                            ldm[j].x = (kps_data[idx * 10 + j * 2 + 0] * anchor.stride_x + anchor.cx);
                            ldm[j].y = (kps_data[idx * 10 + j * 2 + 1] * anchor.stride_y + anchor.cy);
                        }

                        candidate_bboxes.push_back(GazeRect(x_left, y_top, w, h));
                        candidate_scores.push_back(score);
                        candidate_landmarks.push_back(ldm);
                    }
                }
                anchor_offset += num_anchors_s;
            }

            if (candidate_bboxes.empty())
            {
                out_crops.face_detected = false;
                return false;
            }

            // 8. Non-Maximum Suppression (NMS) based on Intersection over Union (IoU).
            // This algorithm aligns exactly with OpenCV's standard cv::dnn::NMSBoxes logic:
            // - Candidate bounding boxes are sorted in descending order of confidence scores.
            // - Starting with the highest scoring box, we compute the Intersection over Union (IoU) 
            //   with all lower-scoring, unsuppressed boxes.
            // - Any candidate box with an overlap ratio (IoU) exceeding nms_threshold is suppressed 
            //   (discarded) as it likely represents a duplicate detection of the same face.
            std::vector<int> keep;
            std::vector<size_t> indices(candidate_scores.size());
            std::iota(indices.begin(), indices.end(), 0);
            std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b)
                      { return candidate_scores[a] > candidate_scores[b]; });

            std::vector<bool> suppressed(candidate_scores.size(), false);
            for (size_t i = 0; i < indices.size(); ++i)
            {
                size_t idx = indices[i];
                if (suppressed[idx])
                    continue;
                keep.push_back(idx);

                for (size_t j = i + 1; j < indices.size(); ++j)
                {
                    size_t idx2 = indices[j];
                    if (suppressed[idx2])
                        continue;

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

            if (keep.empty())
            {
                tracking_state.missing_frames_count++;
                if (tracking_state.missing_frames_count > 5)
                {
                    tracking_state.reset();
                }
                if (use_roll && tracking_state.missing_frames_count > 5)
                {
                    return process_frame(frame, out_crops);
                }
                out_crops.face_detected = false;
                return false;
            }

            // 9. Process Best Detection Match
            int best_idx = keep[0];
            out_crops.face_detected = true;

            const auto &model_ldm = candidate_landmarks[best_idx];

            // Map landmarks back to unrotated 640x640 space
            std::vector<GazePoint> unrotated_ldm(5);
            if (use_roll)
            {
                double cos_a = std::cos(roll_angle_to_apply);
                double sin_a = std::sin(roll_angle_to_apply);
                double cx = model_w / 2.0;
                double cy = model_h / 2.0;
                for (int i = 0; i < 5; ++i)
                {
                    double dx = model_ldm[i].x - cx;
                    double dy = model_ldm[i].y - cy;
                    unrotated_ldm[i].x = cx + dx * cos_a - dy * sin_a;
                    unrotated_ldm[i].y = cy + dx * sin_a + dy * cos_a;
                }
            }
            else
            {
                unrotated_ldm = model_ldm;
            }

            // Scale landmarks back to original frame scale (reversing letterboxing)
            std::vector<GazePoint> final_ldm(5);
            for (int i = 0; i < 5; ++i)
            {
                final_ldm[i].x = (unrotated_ldm[i].x - pad_x) / scale;
                final_ldm[i].y = (unrotated_ldm[i].y - pad_y) / scale;
            }

            for (int i = 0; i < 5; ++i)
            {
                out_crops.landmarks[i] = final_ldm[i];
            }

            // Update tracking state for next frame
            double roll_dx = final_ldm[1].x - final_ldm[0].x;
            double roll_dy = final_ldm[1].y - final_ldm[0].y;
            double roll_angle_rad = std::atan2(roll_dy, roll_dx);

            log_info(3, "ORTYuNetPipelineLandmarks",
                     "x0", final_ldm[0].x, "y0", final_ldm[0].y,
                     "x1", final_ldm[1].x, "y1", final_ldm[1].y,
                     "x2", final_ldm[2].x, "y2", final_ldm[2].y,
                     "x3", final_ldm[3].x, "y3", final_ldm[3].y,
                     "x4", final_ldm[4].x, "y4", final_ldm[4].y);

            // Initialize warper to CPU default if none is set
            if (!warper)
            {
                warper = std::make_shared<CPUImageWarper>();
            }

            if (frame.data != nullptr)
            {
                crop_eye(frame, final_ldm.data(), true, out_crops.left_eye_data);
                crop_eye(frame, final_ldm.data(), false, out_crops.right_eye_data);
            }
            else
            {
                log_error("YuNetPipelineFailed", "reason", "CPU frame data is null");
                out_crops.face_detected = false;
                return false;
            }

            // 3D face model points setup (Standardized to YuNet native landmarks order: Right Eye, Left Eye, Nose, Right Mouth, Left Mouth)
            std::vector<GazeVector3> model_points = {
                GazeVector3(-FaceModelGeometry::EYE_X, FaceModelGeometry::EYE_Y, FaceModelGeometry::EYE_Z),       // 0: Right Eye (Image Left)
                GazeVector3(FaceModelGeometry::EYE_X, FaceModelGeometry::EYE_Y, FaceModelGeometry::EYE_Z),        // 1: Left Eye (Image Right)
                GazeVector3(0.0, config.nose_y, config.nose_z),                                                   // 2: Nose Tip
                GazeVector3(-FaceModelGeometry::MOUTH_X, FaceModelGeometry::MOUTH_Y, FaceModelGeometry::MOUTH_Z), // 3: Right Mouth (Image Left)
                GazeVector3(FaceModelGeometry::MOUTH_X, FaceModelGeometry::MOUTH_Y, FaceModelGeometry::MOUTH_Z)   // 4: Left Mouth (Image Right)
            };

            std::vector<GazeVector2> image_points(5);
            for (int i = 0; i < 5; ++i)
            {
                image_points[i] = GazeVector2(final_ldm[i].x, final_ldm[i].y);
            }

            // SolvePnP Camera Parameters
            double cx = width / 2.0;
            double cy = height / 2.0;
            double fx = (camera_focal_length_px > 0.0) ? camera_focal_length_px : get_focal_length_px(width, camera_fov_degrees);

            GazeVector3 rvec(0.0, 0.0, roll_angle_rad);
            GazeVector3 tvec(0.0, 0.0, 700.0);
            if (tracking_state.is_tracking)
            {
                rvec = tracking_state.last_rvec;
                tvec = tracking_state.last_tvec;
            }

            bool pnp_success = false;
            try
            {
                pnp_success = solve_pnp_lm(model_points, image_points, fx, fx, cx, cy, rvec, tvec, true);
            }
            catch (const std::exception &e)
            {
                log_error("YuNetPipelinePnPException", "what", e.what());
            }

            if (pnp_success)
            {
                tracking_state.is_tracking = true;
                tracking_state.last_rvec = rvec;
                tracking_state.last_tvec = tvec;
                tracking_state.last_roll_rad = roll_angle_rad;
                tracking_state.missing_frames_count = 0;

                GazeBasis3D R = rodrigues_to_basis(rvec);

                out_crops.head_pose_rotation = rvec;
                out_crops.head_pose_translation = tvec;

                // Calculate and log PnP reprojection error (RMSE) in pixels
                double total_sq_err = 0.0;
                for (size_t i = 0; i < 5; ++i)
                {
                    GazeVector3 p_cam = R.multiply_vector(model_points[i]) + tvec;
                    double depth = p_cam.z; // OpenCV depth is positive Z
                    if (depth > 0.01)
                    {
                        double proj_x = (p_cam.x / depth) * fx + cx;
                        double proj_y = (p_cam.y / depth) * fx + cy; // OpenCV space Y-down matches image Y-down
                        double dx = proj_x - image_points[i].x;
                        double dy = proj_y - image_points[i].y;
                        total_sq_err += dx * dx + dy * dy;
                    }
                }
                double rmse = std::sqrt(total_sq_err / 5.0);
                log_info(3, "ORTYuNetPipelinePnPResult", "rmse_px", rmse, "tx", tvec.x, "ty", tvec.y, "tz", tvec.z);

                out_crops.left_eye_center_cam = R.multiply_vector(GazeVector3(FaceModelGeometry::EYE_X, FaceModelGeometry::EYE_Y, FaceModelGeometry::EYE_Z)) + tvec;
                out_crops.right_eye_center_cam = R.multiply_vector(GazeVector3(-FaceModelGeometry::EYE_X, FaceModelGeometry::EYE_Y, FaceModelGeometry::EYE_Z)) + tvec;
            }
            else
            {
                // Robust fallback handling division-by-zero risk
                double dx = final_ldm[1].x - final_ldm[0].x;
                double dy = final_ldm[1].y - final_ldm[0].y;
                double dist_px = std::sqrt(dx * dx + dy * dy);

                double depth_z;
                if (dist_px > 1e-4)
                {
                    depth_z = (config.ipd_mm * fx) / dist_px;
                }
                else
                {
                    depth_z = 600.0; // Default fallback distance in mm
                }

                double mid_x = (final_ldm[1].x + final_ldm[0].x) / 2.0;
                double mid_y = (final_ldm[1].y + final_ldm[0].y) / 2.0;

                double mid_cam_x = (mid_x - cx) * depth_z / fx;
                double mid_cam_y = (mid_y - cy) * depth_z / fx;

                GazeVector3 mid_cam(mid_cam_x, mid_cam_y, depth_z);
                double half_ipd = config.ipd_mm * 0.5;
                out_crops.left_eye_center_cam = mid_cam + GazeVector3(half_ipd, 0.0, 0.0);
                out_crops.right_eye_center_cam = mid_cam - GazeVector3(half_ipd, 0.0, 0.0);
                out_crops.head_pose_rotation = GazeVector3(0.0, 0.0, 0.0);
                out_crops.head_pose_translation = mid_cam;
            }

            return true;
        }
        catch (const std::exception &e)
        {
            log_error("ORTYuNetPipelineProcessException", "what", e.what());
            return false;
        }
    }

    bool ORTYuNetPipeline::crop_eye(const Frame &frame, const GazePoint landmarks[5], bool is_left, uint8_t out_buffer[EyeCrops::EYE_CROP_SIZE])
    {
        // Select primary eye landmark (landmarks[1] is left eye, landmarks[0] is right eye)
        GazePoint eye_center = is_left ? landmarks[1] : landmarks[0];

        // Compute face roll angle from the eye-to-eye vector (image-left landmarks[0] to image-right landmarks[1])
        double roll_dx = landmarks[1].x - landmarks[0].x;
        double roll_dy = landmarks[1].y - landmarks[0].y;
        double angle = std::atan2(roll_dy, roll_dx) * (180.0 / 3.141592653589793);

        // Calculate inter-pupillary distance in pixels
        double dist_px = std::sqrt(roll_dx * roll_dx + roll_dy * roll_dy);

        // Normalize eye crop scale based on eye distance in pixels to handle different resolutions/distances
        double scale = 70.0 / (dist_px > 1e-6 ? dist_px : 70.0);

        GazeVector2 eye_center_vec(eye_center.x, eye_center.y);

        if (warper)
        {
            return warper->warp(frame.data, frame.width, frame.height, 3, eye_center_vec, angle, scale, out_buffer);
        }
        return false;
    }

    void ORTYuNetPipeline::set_camera_focal_length_px(double f)
    {
        camera_focal_length_px = f;
    }

    void ORTYuNetPipeline::rotate_image_bgr(const unsigned char *src, int w, int h, unsigned char *dst, double angle_rad)
    {
        double cos_a = std::cos(angle_rad);
        double sin_a = std::sin(angle_rad);
        double cx = w / 2.0;
        double cy = h / 2.0;

        for (int y = 0; y < h; ++y)
        {
            double dy = y - cy;
            for (int x = 0; x < w; ++x)
            {
                double dx = x - cx;
                // Backward mapping
                double src_x = cx + dx * cos_a + dy * sin_a;
                double src_y = cy - dx * sin_a + dy * cos_a;

                int dst_idx = (y * w + x) * 3;
                if (src_x >= 0.0 && src_x < w - 1.0 && src_y >= 0.0 && src_y < h - 1.0)
                {
                    int x0 = static_cast<int>(std::floor(src_x));
                    int y0 = static_cast<int>(std::floor(src_y));
                    int x1 = x0 + 1;
                    int y1 = y0 + 1;
                    float tx = static_cast<float>(src_x - x0);
                    float ty = static_cast<float>(src_y - y0);

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

} // namespace Gaze
