#include "gaze_tracking_pipeline.hpp"
#include "../core/cpu_image_warper.hpp"
#include "../core/space_conversions.hpp"
#include "../core/math_defs.hpp"
#include "../core/pnp_solver.hpp"
#include "../core/log.hpp"
#include <cstring>
#include <chrono>
#include <fstream>

namespace Gaze
{
    // Copies a block of data from src to dst, reversing triples.
    static void copy_rgb_to_gbr(const uint8_t *src, uint8_t *dst, size_t sz)
    {
        for (size_t i = 0; i < EYE_CROP_SIZE * EYE_CROP_SIZE; ++i)
        {
            // Copy rgb->bgr
            for (size_t rgb = 0; rgb < 3; ++rgb)
            {
                size_t bgr = 2 - rgb;
                dst[i * 3 + rgb] = src[i * 3 + bgr];
            }
        }
    }

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
                {
                    std::lock_guard<std::mutex> lock(state_mutex);
                    if (initialized)
                    {
                        if (config_dirty)
                        {
                            if (face_detector)
                                face_detector->set_config(active_config);
                            if (gaze_estimator)
                                gaze_estimator->set_config(active_config);
                            config_dirty = false;
                        }

                        auto start_total = std::chrono::steady_clock::now();

                        // Form a temporary Frame pointing to data's camera_raw_bgr
                        Frame frame;
                        frame.width = data->camera_width;
                        frame.height = data->camera_height;
                        frame.data = data->camera_raw_bgr.data();
                        frame.timestamp = data->timestamp;

                        // 1. Counter-rotate frame by -prev_roll_rad so face is presented upright in working_frame
                        const unsigned char *working_data = frame.data;
                        if (std::abs(prev_roll_rad) > 1e-4f)
                        {
                            rotated_frame_buffer.resize(frame.width * frame.height * 3);
                            rotate_image_bgr(frame.data, frame.width, frame.height, rotated_frame_buffer.data(), -prev_roll_rad);
                            working_data = rotated_frame_buffer.data();
                        }
                        Frame working_frame = frame;
                        working_frame.data = const_cast<unsigned char *>(working_data);

                        YuNetResult yunet_res;
                        auto start_face = std::chrono::steady_clock::now();
                        bool success = face_detector ? face_detector->process_frame(working_frame, yunet_res, 0.0f) : false;
                        auto end_face = std::chrono::steady_clock::now();
                        double face_ms = std::chrono::duration<double, std::milli>(end_face - start_face).count();

                        double gaze_ms = 0.0;
                        data->face_detected = success && yunet_res.face_detected;
                        data->gaze_success = false;

                        if (data->face_detected)
                        {
                            std::vector<GazeVector2> landmarks_35;
                            bool lm_ok = false;
                            if (landmark_model)
                            {
                                GazeRect bbox(yunet_res.roi_x, yunet_res.roi_y, yunet_res.roi_w, yunet_res.roi_h);
                                lm_ok = landmark_model->extract_landmarks(working_frame.data, working_frame.width, working_frame.height, bbox, landmarks_35, 0.0f);
                            }

                            if (lm_ok && landmarks_35.size() == 35)
                            {
                                double focal = (data->camera_focal_length_px > 0.0) ? data->camera_focal_length_px : static_cast<double>(working_frame.width);
                                double cx = working_frame.width * 0.5;
                                double cy = working_frame.height * 0.5;
                                GazeVector3 rvec(0.0f, 0.0f, 0.0f);
                                GazeVector3 tvec(0.0f, 0.0f, 600.0f);
                                static const auto model_35pt = get_canonical_35pt_face_model();
                                bool pnp_ok = solve_pnp_lm(model_35pt, landmarks_35, focal, focal, cx, cy, rvec, tvec, false);

                                if (pnp_ok)
                                {
                                    if (std::abs(prev_roll_rad) > 1e-4f)
                                    {
                                        GazeBasis3D R_up = rodrigues_to_basis(rvec);
                                        GazeBasis3D R_z = rodrigues_to_basis(GazeVector3(0.0, 0.0, prev_roll_rad));
                                        GazeBasis3D R_orig = R_z * R_up;
                                        GazeVector3 t_orig = R_z.multiply_vector(tvec);
                                        GazeVector3 r_orig = basis_to_rodrigues(R_orig);

                                        yunet_res.head_pose.pitch_rad = r_orig.x;
                                        yunet_res.head_pose.yaw_rad = r_orig.y;
                                        yunet_res.head_pose.roll_rad = r_orig.z;
                                        yunet_res.head_pose.trans_x_mm = t_orig.x;
                                        yunet_res.head_pose.trans_y_mm = t_orig.y;
                                        yunet_res.head_pose.trans_z_mm = t_orig.z;
                                    }
                                    else
                                    {
                                        yunet_res.head_pose.pitch_rad = rvec.x;
                                        yunet_res.head_pose.yaw_rad = rvec.y;
                                        yunet_res.head_pose.roll_rad = rvec.z;
                                        yunet_res.head_pose.trans_x_mm = tvec.x;
                                        yunet_res.head_pose.trans_y_mm = tvec.y;
                                        yunet_res.head_pose.trans_z_mm = tvec.z;
                                    }
                                }

                                // Extract dynamic eye crops directly from upright working_frame (naturally horizontal & centered)
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

                                float r_box_s = std::max(20.0f, r_w * 2.2f);
                                float l_box_s = std::max(20.0f, l_w * 2.2f);

                                crop_and_resize_bgr(working_frame.data, working_frame.width, working_frame.height,
                                                    r_cx - r_box_s * 0.5f, r_cy - r_box_s * 0.5f, r_box_s, r_box_s,
                                                    yunet_res.right_eye_crop, 60, 60);

                                crop_and_resize_bgr(working_frame.data, working_frame.width, working_frame.height,
                                                    l_cx - l_box_s * 0.5f, l_cy - l_box_s * 0.5f, l_box_s, l_box_s,
                                                    yunet_res.left_eye_crop, 60, 60);

                                data->has_landmarks_2d = true;
                                for (size_t i = 0; i < 35; ++i)
                                {
                                    GazeVector2 pt = landmarks_35[i];
                                    if (std::abs(prev_roll_rad) > 1e-4f)
                                    {
                                        pt = rotate_point_back(pt, prev_roll_rad, frame.width, frame.height);
                                    }
                                    data->landmarks_2d_px[i * 2 + 0] = pt.x;
                                    data->landmarks_2d_px[i * 2 + 1] = pt.y;
                                }
                            }
                            else
                            {
                                data->has_landmarks_2d = false;
                                yunet_res.head_pose.roll_rad = prev_roll_rad + yunet_res.head_pose.roll_rad;
                            }

                            data->head_translation = yunet_res.head_pose.translation();
                            data->head_rotation = yunet_res.head_pose.rotation_vector();
                            prev_roll_rad = yunet_res.head_pose.roll_rad;

                            // Estimate Eye Openness for Left & Right eyes via ORTEyeStateModel
                            if (eye_state_model)
                            {
                                eye_state_model->estimate_openness(yunet_res.left_eye_crop, data->left_eye_openness);
                                eye_state_model->estimate_openness(yunet_res.right_eye_crop, data->right_eye_openness);
                            }

                            if (data->left_eye_buffer)
                            {
                                std::memcpy(data->left_eye_buffer, yunet_res.left_eye_crop, EYE_CROP_SIZE * EYE_CROP_SIZE * 3);
                            }
                            if (data->right_eye_buffer)
                            {
                                std::memcpy(data->right_eye_buffer, yunet_res.right_eye_crop, EYE_CROP_SIZE * EYE_CROP_SIZE * 3);
                            }

                            if (data->full_crop_buffer && data->full_crop_bytes >= 160 * 128 * 3)
                            {
                                crop_and_resize_bgr_to_rgb(frame.data, frame.width, frame.height, yunet_res.roi_x, yunet_res.roi_y, yunet_res.roi_w, yunet_res.roi_h, data->full_crop_buffer, 160, 128);
                            }

                            EyeCrops crops;
                            crops.face_detected = true;
                            crops.head_pose_translation = yunet_res.head_pose.translation();
                            crops.head_pose_rotation = yunet_res.head_pose.rotation_vector();

                            GazeBasis3D head_rot = yunet_res.head_pose.rotation_matrix();
                            GazeVector3 head_trans = yunet_res.head_pose.translation();
                            // Canonical 35-pt model: Right eye is at X = -30.5mm, Left eye is at X = +30.5mm, Y = -32.0mm, Z = -13.0mm
                            crops.right_eye_center_cam = head_rot.multiply_vector(GazeVector3(-30.5, -32.0, -13.0)) + head_trans;
                            crops.left_eye_center_cam = head_rot.multiply_vector(GazeVector3(30.5, -32.0, -13.0)) + head_trans;
                            std::memcpy(crops.left_eye_data, yunet_res.left_eye_crop, 60*60*3);
                            std::memcpy(crops.right_eye_data, yunet_res.right_eye_crop, 60*60*3);

                            GazeVector3 raw_gaze_dir_cam;
                            auto start_gaze = std::chrono::steady_clock::now();
                            bool gaze_success = gaze_estimator ? gaze_estimator->estimate_raw_gaze(crops, raw_gaze_dir_cam) : false;
                            auto end_gaze = std::chrono::steady_clock::now();
                            gaze_ms = std::chrono::duration<double, std::milli>(end_gaze - start_gaze).count();

                            if (gaze_success)
                            {
                                data->gaze_success = true;
                                GazeTransform3D head_xform = Gaze::Inference::get_head_transform_in_camera_space(yunet_res.head_pose.translation(), yunet_res.head_pose.rotation_vector());
                                data->gaze_origin = head_xform.origin;
                                data->gaze_direction = raw_gaze_dir_cam;
                            }
                        }
                        else
                        {
                            prev_roll_rad = 0.0f; // Reset roll if face is lost
                        }

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
                                     "frame_w", frame.width,
                                     "frame_h", frame.height);
                        }
                    }
                }
                results_mailbox.put(data);
                worker_busy = false;
            }
        }
    }

} // namespace Gaze
