#include "gaze_tracking_pipeline.hpp"
#include "../core/cpu_image_warper.hpp"
#include "../core/space_conversions.hpp"
#include "../core/math_defs.hpp"
#include "../core/log.hpp"
#include <cstring>
#include <chrono>

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

    bool GazeTrackingPipeline::initialize(const std::vector<uint8_t> &face_mesh_model_data, const std::vector<uint8_t> &gaze_model_data, const std::vector<uint8_t> &eye_openness_model_data)
    {
        std::lock_guard<std::mutex> life_lock(lifecycle_mutex);
        std::lock_guard<std::mutex> lock(state_mutex);

        face_detector = std::make_unique<MediaPipeFaceMeshPipeline>(face_mesh_model_data, eye_openness_model_data);
        gaze_estimator = std::make_unique<ORTGazeModel>(gaze_model_data);

        // 2-Model Architecture: MediaPipe Face Mesh pipeline computes 3D EAR eye openness directly.
        // No heuristic fallback is used.
        blink_estimator = nullptr;

        gaze_estimator->set_config(active_config);

        if (face_detector->initialize() && gaze_estimator->initialize())
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

                        MediaPipeFaceMeshResult mp_res;
                        auto start_face = std::chrono::steady_clock::now();
                        bool success = face_detector ? face_detector->process_frame(frame, mp_res) : false;
                        auto end_face = std::chrono::steady_clock::now();
                        double face_ms = std::chrono::duration<double, std::milli>(end_face - start_face).count();

                        double gaze_ms = 0.0;
                        data->face_detected = success && mp_res.face_detected;
                        data->gaze_success = false;

                        if (data->face_detected)
                        {
                            data->head_translation = mp_res.head_pose.translation();
                            data->head_rotation = mp_res.head_pose.rotation_vector();
                            data->left_eye_openness = mp_res.left_eye_openness;
                            data->right_eye_openness = mp_res.right_eye_openness;

                            if (data->left_eye_buffer)
                            {
                                std::memcpy(data->left_eye_buffer, mp_res.left_eye_crop, EYE_CROP_SIZE * EYE_CROP_SIZE * 3);
                            }
                            if (data->right_eye_buffer)
                            {
                                std::memcpy(data->right_eye_buffer, mp_res.right_eye_crop, EYE_CROP_SIZE * EYE_CROP_SIZE * 3);
                            }

                            if (data->full_crop_buffer && data->full_crop_bytes >= 160 * 128 * 3)
                            {
                                crop_and_resize_bgr_to_rgb(frame.data, frame.width, frame.height, mp_res.roi_x, mp_res.roi_y, mp_res.roi_w, mp_res.roi_h, data->full_crop_buffer, 160, 128);
                            }

                            EyeCrops crops;
                            crops.face_detected = true;
                            crops.head_pose_translation = mp_res.head_pose.translation();
                            crops.head_pose_rotation = mp_res.head_pose.rotation_vector();

                            GazeBasis3D head_rot = mp_res.head_pose.rotation_matrix();
                            GazeVector3 head_trans = mp_res.head_pose.translation();
                            crops.left_eye_center_cam = head_rot.multiply_vector(GazeVector3(-30.0, 28.676, 0.0)) + head_trans;
                            crops.right_eye_center_cam = head_rot.multiply_vector(GazeVector3(30.0, 28.676, 0.0)) + head_trans;
                            std::memcpy(crops.left_eye_data, mp_res.left_eye_crop, 60*60*3);
                            std::memcpy(crops.right_eye_data, mp_res.right_eye_crop, 60*60*3);

                            GazeVector3 raw_gaze_dir_cam;
                            auto start_gaze = std::chrono::steady_clock::now();
                            bool gaze_success = gaze_estimator ? gaze_estimator->estimate_raw_gaze(crops, raw_gaze_dir_cam) : false;
                            auto end_gaze = std::chrono::steady_clock::now();
                            gaze_ms = std::chrono::duration<double, std::milli>(end_gaze - start_gaze).count();

                            if (gaze_success)
                            {
                                data->gaze_success = true;
                                GazeTransform3D head_xform = Gaze::Inference::get_head_transform_in_camera_space(mp_res.head_pose.translation(), mp_res.head_pose.rotation_vector());
                                data->gaze_origin = head_xform.origin;
                                data->gaze_direction = raw_gaze_dir_cam;
                            }
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
