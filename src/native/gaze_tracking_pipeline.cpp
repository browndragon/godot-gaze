#include "gaze_tracking_pipeline.hpp"
#include "../core/cpu_image_warper.hpp"
#include "../core/opencv_space_conversions.hpp"
#include "../core/face_model_geometry.hpp"
#include "../core/math_defs.hpp"
#include "../core/pnp_solver.hpp"
#include "../core/log.hpp"
#include <cstring>
#include <chrono>
#include <fstream>

namespace Gaze
{
    GazeTrackingPipeline::~GazeTrackingPipeline()
    {
        log_info(2, "GazeTrackingPipeline_Destructor_Began");
        stop();
        log_info(2, "GazeTrackingPipeline_Destructor_Finished");
    }

    bool GazeTrackingPipeline::initialize(
        const std::vector<uint8_t> &yunet_model_data,
        const std::vector<uint8_t> &gaze_model_data,
        const std::vector<uint8_t> &eye_openness_model_data,
        const std::vector<uint8_t> &landmark_model_data)
    {
        std::lock_guard<std::mutex> life_lock(lifecycle_mutex);
        std::lock_guard<std::mutex> lock(state_mutex);

        face_detector = std::make_unique<ORTYuNetDetector>(yunet_model_data);
        eye_state_model = std::make_unique<ORTEyeStateModel>(eye_openness_model_data);
        gaze_estimator = std::make_unique<ORTGazeModel>(gaze_model_data);

        if (!landmark_model_data.empty())
        {
            landmark_model = std::make_unique<ORTLandmarkModel>(landmark_model_data);
        }
        else
        {
            std::string lm_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
            std::ifstream f(lm_path.c_str());
            if (!f.good()) lm_path = "../" + lm_path;
            landmark_model = std::make_unique<ORTLandmarkModel>(lm_path);
        }

        gaze_estimator->set_config(active_config);

        bool face_ok = face_detector->initialize();
        bool eye_ok = eye_state_model->initialize();
        bool gaze_ok = gaze_estimator->initialize();
        if (landmark_model)
        {
            landmark_model->initialize();
        }

        if (face_ok && eye_ok && gaze_ok)
        {
            initialized = true;
            log_info("GazeTrackingPipeline_Initialized");
            return true;
        }

        log_error("GazeTrackingPipeline_InitializeFailed");
        return false;
    }

    bool GazeTrackingPipeline::initialize(
        const std::string &yunet_model_path,
        const std::string &gaze_model_path,
        const std::string &eye_openness_model_path,
        const std::string &landmark_model_path)
    {
        std::lock_guard<std::mutex> life_lock(lifecycle_mutex);
        std::lock_guard<std::mutex> lock(state_mutex);

        face_detector = std::make_unique<ORTYuNetDetector>(yunet_model_path);
        eye_state_model = std::make_unique<ORTEyeStateModel>(eye_openness_model_path);
        gaze_estimator = std::make_unique<ORTGazeModel>(gaze_model_path);

        std::string lm_path = landmark_model_path;
        if (lm_path.empty())
        {
            lm_path = "project/addons/godot-gaze/models/facial-landmarks-35-adas-0002.ort";
            std::ifstream f(lm_path.c_str());
            if (!f.good()) lm_path = "../" + lm_path;
        }
        landmark_model = std::make_unique<ORTLandmarkModel>(lm_path);

        gaze_estimator->set_config(active_config);

        bool face_ok = face_detector->initialize();
        bool eye_ok = eye_state_model->initialize();
        bool gaze_ok = gaze_estimator->initialize();
        if (landmark_model)
        {
            landmark_model->initialize();
        }

        if (face_ok && eye_ok && gaze_ok)
        {
            initialized = true;
            log_info("GazeTrackingPipeline_Initialized");
            return true;
        }

        log_error("GazeTrackingPipeline_InitializeFailed");
        return false;
    }

    void GazeTrackingPipeline::start()
    {
        std::lock_guard<std::mutex> life_lock(lifecycle_mutex);
        std::lock_guard<std::mutex> lock(state_mutex);
        if (thread_running)
            return;
        if (!initialized)
        {
            log_error("GazeTrackingPipeline_CannotStart_NotInitialized");
            return;
        }

        thread_running = true;
        worker_thread = std::thread(&GazeTrackingPipeline::_worker_loop, this);
        log_info("GazeTrackingPipeline_ThreadStarted");
    }

    void GazeTrackingPipeline::stop()
    {
        log_info(2, "GazeTrackingPipeline_Stop_Began");
        std::lock_guard<std::mutex> life_lock(lifecycle_mutex);
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            if (!thread_running) {
                log_info(2, "GazeTrackingPipeline_Stop_ThreadNotRunning");
                return;
            }
            thread_running = false;
        }

        log_info(2, "GazeTrackingPipeline_Stop_NotifyWorker");
        {
            std::lock_guard<std::mutex> lock(worker_mutex);
            frame_pending = true;
            worker_cv.notify_one();
        }

        if (worker_thread.joinable())
        {
            log_info(2, "GazeTrackingPipeline_Stop_JoiningWorker");
            worker_thread.join();
            log_info(2, "GazeTrackingPipeline_Stop_WorkerJoined");
        }
        log_info("GazeTrackingPipeline_ThreadStopped");
    }

    void GazeTrackingPipeline::set_config(const PipelineConfig &p_config)
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        active_config = p_config;
        config_dirty = true;
    }

    void GazeTrackingPipeline::push_frame_request(GazeFrameData *p_req)
    {
        request_mailbox.put(p_req);
        {
            std::lock_guard<std::mutex> lock(worker_mutex);
            frame_pending = true;
            worker_cv.notify_one();
        }
    }

    bool GazeTrackingPipeline::pop_result(GazeFrameData** out_res)
    {
        if (!out_res)
        {
            return false;
        }
        return results_mailbox.take(*out_res);
    }

    void GazeTrackingPipeline::clear_work_queue()
    {
        request_mailbox.clear();
        results_mailbox.clear();
        roll_filter.reset(0.0f);
    }

    void GazeTrackingPipeline::_worker_loop()
    {
        while (thread_running)
        {
            GazeFrameData *data = nullptr;
            bool has_request = false;
            {
                std::unique_lock<std::mutex> lock(worker_mutex);
                worker_cv.wait(lock, [this]
                               { return frame_pending || !thread_running; });
                if (!thread_running)
                {
                    break;
                }
                frame_pending = false;
            }

            if (request_mailbox.take(data))
            {
                has_request = true;
            }

            if (has_request && thread_running && data)
            {
                worker_busy = true;
                process_frame_synchronous(data);
                results_mailbox.put(data);
                worker_busy = false;
            }
        }
    }

    void GazeTrackingPipeline::process_frame_synchronous(GazeFrameData *data)
    {
        if (!data) return;
        std::lock_guard<std::mutex> lock(state_mutex);
        if (!initialized)
        {
            data->face_detected = false;
            data->gaze_success = false;
            return;
        }

        if (config_dirty)
        {
            if (face_detector)
                face_detector->set_config(active_config);
            if (gaze_estimator)
                gaze_estimator->set_config(active_config);
            config_dirty = false;
        }

        auto start_total = std::chrono::steady_clock::now();

        Frame working_frame;
        _stage_1_apply_roll_hint(data, working_frame);

        auto start_face = std::chrono::steady_clock::now();
        bool face_ok = _stage_2_detect_face_bbox(data, working_frame);
        auto end_face = std::chrono::steady_clock::now();
        double face_ms = std::chrono::duration<double, std::milli>(end_face - start_face).count();

        double gaze_ms = 0.0;
        if (face_ok)
        {
            bool lm_ok = _stage_3_extract_landmarks(data, working_frame);
            if (lm_ok)
            {
                OpenCVCameraVector3 rvec(0.0, 0.0, 0.0);
                OpenCVCameraVector3 tvec(0.0, 0.0, 600.0);
                bool pose_ok = _stage_4_solve_head_pose(data, working_frame, rvec, tvec);
                if (pose_ok)
                {
                    _stage_5_extract_eye_crops(data, working_frame, rvec, tvec);
                    _stage_6_estimate_eye_state(data);

                    auto start_gaze = std::chrono::steady_clock::now();
                    _stage_7_estimate_gaze_direction(data);
                    auto end_gaze = std::chrono::steady_clock::now();
                    gaze_ms = std::chrono::duration<double, std::milli>(end_gaze - start_gaze).count();
                }
                else
                {
                    data->face_detected = false;
                }
            }
            else
            {
                data->face_detected = false;
            }
            if (data->face_detected)
            {
                _stage_8_unroll_to_canonical_godot_camera(data);
            }
        }

        // Update temporal roll filter
        float solved_roll = 0.0f;
        if (data->face_detected)
        {
            solved_roll = static_cast<float>(data->head_rotation.z);
        }
        float conf = std::clamp((data->face_score - 0.20f) / 0.60f, 0.25f, 1.0f);
        roll_filter.update(data->face_detected, solved_roll, conf, data->timestamp);

        auto end_total = std::chrono::steady_clock::now();
        double total_ms = std::chrono::duration<double, std::milli>(end_total - start_total).count();

        static int stats_count = 0;
        int verbosity = get_log_verbosity().load(std::memory_order_acquire);
        if (verbosity >= 3 || (verbosity >= 1 && stats_count++ % 30 == 0))
        {
            log_info(verbosity >= 3 ? 3 : 1, "Pipeline_PerformanceStats",
                     "face_ms", face_ms,
                     "gaze_ms", gaze_ms,
                     "total_ms", total_ms,
                     "frame_w", data->camera_width,
                     "frame_h", data->camera_height);
        }
    }

    void GazeTrackingPipeline::_stage_1_apply_roll_hint(GazeFrameData *data, Frame &working_frame)
    {
        if (data->auto_roll_enabled)
        {
            if (std::abs(roll_filter.get_value()) < 1e-4f && std::abs(data->roll_hint_rad) > 1e-4f)
            {
                roll_filter.reset(data->roll_hint_rad);
            }
            data->roll_hint_rad = roll_filter.get_value();
        }
        Frame frame;
        frame.width = data->camera_width;
        frame.height = data->camera_height;
        frame.data = data->camera_raw_bgr.data();
        frame.timestamp = data->timestamp;

        const unsigned char *working_data = frame.data;
        if (std::abs(data->roll_hint_rad) > 1e-4f)
        {
            data->internal_rotated_frame_bgr.resize(frame.width * frame.height * 3);
            rotate_image(frame.data, frame.width, frame.height, data->internal_rotated_frame_bgr.data(), +data->roll_hint_rad);
            working_data = data->internal_rotated_frame_bgr.data();
        }
        working_frame = frame;
        working_frame.data = const_cast<unsigned char *>(working_data);
    }

    bool GazeTrackingPipeline::_stage_2_detect_face_bbox(GazeFrameData *data, const Frame &working_frame)
    {
        if (!face_detector) return false;
        YuNetResult yunet_res;
        bool success = face_detector->process_frame(working_frame, yunet_res, 0.0f);
        data->face_detected = success && yunet_res.face_detected;
        if (data->face_detected)
        {
            data->face_bbox = GazeRect(yunet_res.roi_x, yunet_res.roi_y, yunet_res.roi_w, yunet_res.roi_h);
            data->face_score = yunet_res.score;
        }
        else
        {
            data->face_score = 0.0f;
        }
        return data->face_detected;
    }

    bool GazeTrackingPipeline::_stage_3_extract_landmarks(GazeFrameData *data, const Frame &working_frame)
    {
        if (!landmark_model || !data->face_detected)
        {
            data->has_landmarks_2d = false;
            return false;
        }
        bool lm_ok = landmark_model->extract_landmarks(working_frame.data, working_frame.width, working_frame.height, data->face_bbox, data->internal_landmarks_working_px, 0.0f);
        if (lm_ok && data->internal_landmarks_working_px.size() == 35)
        {
            data->has_landmarks_2d = true;
        }
        else
        {
            data->has_landmarks_2d = false;
        }
        return data->has_landmarks_2d;
    }

    bool GazeTrackingPipeline::_stage_4_solve_head_pose(GazeFrameData *data, const Frame &working_frame, OpenCVCameraVector3 &out_rvec, OpenCVCameraVector3 &out_tvec)
    {
        if (!data->has_landmarks_2d || data->internal_landmarks_working_px.size() != 35) return false;
        double focal = (data->camera_focal_length_px > 0.0) ? data->camera_focal_length_px : calculate_default_focal_length(static_cast<double>(working_frame.width));
        double cx = working_frame.width * 0.5;
        double cy = working_frame.height * 0.5;
        static const auto model_35pt = FaceModelGeometry::get_canonical_35pt_model_points();
        bool pnp_ok = SQPnPSolver::solve_rvec(model_35pt, data->internal_landmarks_working_px, focal, focal, cx, cy, out_rvec, out_tvec);
        if (!pnp_ok) return false;

        data->head_transform = CoordinateConversions::opencv_pose_to_godot_camera_transform(out_tvec, out_rvec);
        data->head_translation = data->head_transform.origin;
        data->head_rotation = data->head_transform.basis.get_euler_rad();
        return true;
    }

    void GazeTrackingPipeline::_stage_5_extract_eye_crops(GazeFrameData *data, const Frame &working_frame, const OpenCVCameraVector3 &rvec, const OpenCVCameraVector3 &tvec)
    {
        if (!data->has_landmarks_2d || data->internal_landmarks_working_px.size() != 35) return;

        // Landmarks: [0..1] = Image Left eye (Anatomical Right), [2..3] = Image Right eye (Anatomical Left)
        float r_cx = static_cast<float>((data->internal_landmarks_working_px[0].x + data->internal_landmarks_working_px[1].x) * 0.5);
        float r_cy = static_cast<float>((data->internal_landmarks_working_px[0].y + data->internal_landmarks_working_px[1].y) * 0.5);
        float r_dx = static_cast<float>(data->internal_landmarks_working_px[0].x - data->internal_landmarks_working_px[1].x);
        float r_dy = static_cast<float>(data->internal_landmarks_working_px[0].y - data->internal_landmarks_working_px[1].y);
        float r_w = std::sqrt(r_dx * r_dx + r_dy * r_dy);

        float l_cx = static_cast<float>((data->internal_landmarks_working_px[2].x + data->internal_landmarks_working_px[3].x) * 0.5);
        float l_cy = static_cast<float>((data->internal_landmarks_working_px[2].y + data->internal_landmarks_working_px[3].y) * 0.5);
        float l_dx = static_cast<float>(data->internal_landmarks_working_px[2].x - data->internal_landmarks_working_px[3].x);
        float l_dy = static_cast<float>(data->internal_landmarks_working_px[2].y - data->internal_landmarks_working_px[3].y);
        float l_w = std::sqrt(l_dx * l_dx + l_dy * l_dy);

        float eye_box_sz = std::max({24.0f, r_w * 1.8f, l_w * 1.8f});
        data->eye_box_sz = eye_box_sz;

        data->eye_crops.face_detected = true;
        data->eye_crops.head_pose_translation = tvec;
        data->eye_crops.head_pose_rotation = rvec;

        SpacedBasis<Space::OpenCVFaceModel, Space::OpenCVCamera> head_rot = rodrigues_to_basis<Space::OpenCVFaceModel, Space::OpenCVCamera>(rvec);
        OpenCVCameraVector3 right_eye_cv = head_rot.transform(OpenCVFaceVector3(-31.5, -32.0, 35.0)) + tvec;
        OpenCVCameraVector3 left_eye_cv = head_rot.transform(OpenCVFaceVector3(31.5, -32.0, 35.0)) + tvec;
        data->eye_crops.right_eye_center_cam = CoordinateConversions::to_godot_camera(right_eye_cv);
        data->eye_crops.left_eye_center_cam = CoordinateConversions::to_godot_camera(left_eye_cv);

        crop_and_resize_bgr(working_frame.data, working_frame.width, working_frame.height,
                            r_cx - eye_box_sz * 0.5f, r_cy - eye_box_sz * 0.5f, eye_box_sz, eye_box_sz,
                            data->eye_crops.right_eye_data, 60, 60);

        crop_and_resize_bgr(working_frame.data, working_frame.width, working_frame.height,
                            l_cx - eye_box_sz * 0.5f, l_cy - eye_box_sz * 0.5f, eye_box_sz, eye_box_sz,
                            data->eye_crops.left_eye_data, 60, 60);

        if (data->left_eye_buffer)
        {
            std::memcpy(data->left_eye_buffer, data->eye_crops.left_eye_data, EYE_CROP_SIZE * EYE_CROP_SIZE * 3);
        }
        if (data->right_eye_buffer)
        {
            std::memcpy(data->right_eye_buffer, data->eye_crops.right_eye_data, EYE_CROP_SIZE * EYE_CROP_SIZE * 3);
        }
        if (data->full_crop_buffer && data->full_crop_bytes >= 160 * 128 * 3)
        {
            crop_and_resize_bgr_to_rgb(data->camera_raw_bgr.data(), data->camera_width, data->camera_height,
                                      data->face_bbox.x, data->face_bbox.y, data->face_bbox.width, data->face_bbox.height,
                                      data->full_crop_buffer, 160, 128);
        }
    }

    void GazeTrackingPipeline::_stage_6_estimate_eye_state(GazeFrameData *data)
    {
        if (!eye_state_model || !data->face_detected) return;
        eye_state_model->estimate_openness(data->eye_crops.left_eye_data, data->left_eye_openness);
        eye_state_model->estimate_openness(data->eye_crops.right_eye_data, data->right_eye_openness);
    }

    void GazeTrackingPipeline::_stage_7_estimate_gaze_direction(GazeFrameData *data)
    {
        if (!gaze_estimator || !data->face_detected)
        {
            data->gaze_success = false;
            return;
        }
        OpenVINOGazeVector3 raw_gaze_dir_openvino;
        bool success = gaze_estimator->estimate_raw_gaze(data->eye_crops, raw_gaze_dir_openvino);
        if (!success)
        {
            data->gaze_success = false;
            return;
        }
        data->gaze_success = true;

        // Raw gaze mapped directly into canonical Hub GodotCameraVector3
        GodotCameraVector3 gaze_hint_rolled = CoordinateConversions::to_godot_camera(raw_gaze_dir_openvino);
        data->gaze_direction = gaze_hint_rolled.normalized();

        // Eye midpoint in hint-rolled GodotCamera space
        data->gaze_origin = (data->eye_crops.right_eye_center_cam + data->eye_crops.left_eye_center_cam) * 0.5;
    }

    void GazeTrackingPipeline::_stage_8_unroll_to_canonical_godot_camera(GazeFrameData *data)
    {
        if (!data->face_detected)
        {
            return;
        }

        // 1. Unroll 3D Head Transform from GodotCameraHintRolled to canonical GodotCamera
        data->head_transform = CoordinateConversions::godot_camera_hint_rolled_to_godot_camera(data->head_transform, -data->roll_hint_rad);
        data->head_translation = data->head_transform.origin;
        data->head_rotation = data->head_transform.basis.get_euler_rad();

        // 2. Unroll 2D Landmarks back to original camera pixel coordinates
        if (data->has_landmarks_2d && data->internal_landmarks_working_px.size() == 35)
        {
            for (size_t i = 0; i < 35; ++i)
            {
                SpacedVector2<Space::GodotCameraWorkingImagePixels> pt = data->internal_landmarks_working_px[i];
                if (std::abs(data->roll_hint_rad) > 1e-4f)
                {
                    pt = rotate_point_2d(pt, -data->roll_hint_rad, data->camera_width, data->camera_height);
                }
                data->landmarks_2d_px[i * 2 + 0] = static_cast<float>(pt.x);
                data->landmarks_2d_px[i * 2 + 1] = static_cast<float>(pt.y);
            }
        }

        // 3. Unroll 3D Gaze Ray and Eye Centers
        if (std::abs(data->roll_hint_rad) > 1e-4f)
        {
            float cos_a = std::cos(-data->roll_hint_rad);
            float sin_a = std::sin(-data->roll_hint_rad);
            SpacedBasis<Space::GodotCamera, Space::GodotCamera> R_roll(
                GodotCameraVector3(cos_a, -sin_a, 0.0),
                GodotCameraVector3(sin_a,  cos_a, 0.0),
                GodotCameraVector3( 0.0,   0.0, 1.0)
            );
            if (data->gaze_success)
            {
                data->gaze_direction = R_roll.transform(data->gaze_direction).normalized();
                data->gaze_origin = R_roll.transform(data->gaze_origin);
            }
            // CRITICAL FIX: Unroll eye crops eye centers so (left + right) * 0.5 == gaze_origin in canonical GodotCamera space!
            data->eye_crops.left_eye_center_cam = R_roll.transform(data->eye_crops.left_eye_center_cam);
            data->eye_crops.right_eye_center_cam = R_roll.transform(data->eye_crops.right_eye_center_cam);
        }
    }

} // namespace Gaze
