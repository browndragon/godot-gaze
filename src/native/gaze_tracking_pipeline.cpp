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
        log_info("GazeTrackingPipeline_Destructor_Began");
        stop();
        log_info("GazeTrackingPipeline_Destructor_Finished");
    }

    bool GazeTrackingPipeline::initialize(const std::vector<uint8_t> &yunet_model_data, const std::vector<uint8_t> &gaze_model_data)
    {
        std::lock_guard<std::mutex> life_lock(lifecycle_mutex);
        std::lock_guard<std::mutex> lock(state_mutex);

        face_detector = std::make_unique<ORTYuNetPipeline>(yunet_model_data);
        gaze_estimator = std::make_unique<ORTGazeModel>(gaze_model_data);

        // Setup CPU image warper for eye cropping/alignment
        std::shared_ptr<ImageWarper> warper = std::make_shared<CPUImageWarper>();
        face_detector->set_image_warper(warper);

        face_detector->set_config(active_config);
        gaze_estimator->set_config(active_config);

        // Set standard default focal length and FOV
        face_detector->set_camera_focal_length_px(-1.0);
        face_detector->set_camera_fov_degrees(DEFAULT_CAMERA_FOV_DEGREES);

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

        thread_running = true;
        worker_thread = std::thread(&GazeTrackingPipeline::_worker_loop, this);
        log_info("GazeTrackingPipeline_ThreadStarted");
    }

    void GazeTrackingPipeline::stop()
    {
        log_info("GazeTrackingPipeline_Stop_Began");
        std::lock_guard<std::mutex> life_lock(lifecycle_mutex);
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            if (!thread_running) {
                log_info("GazeTrackingPipeline_Stop_ThreadNotRunning");
                return;
            }
            thread_running = false;
        }

        log_info("GazeTrackingPipeline_Stop_NotifyWorker");
        {
            std::lock_guard<std::mutex> lock(worker_mutex);
            frame_pending = true;
            worker_cv.notify_one();
        }

        if (worker_thread.joinable())
        {
            log_info("GazeTrackingPipeline_Stop_JoiningWorker");
            worker_thread.join();
            log_info("GazeTrackingPipeline_Stop_WorkerJoined");
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

                        EyeCrops crops;
                        auto start_face = std::chrono::steady_clock::now();
                        if (face_detector)
                        {
                            face_detector->set_camera_focal_length_px(data->camera_focal_length_px);
                            face_detector->set_camera_fov_degrees(data->camera_fov_degrees);
                        }
                        bool success = face_detector ? face_detector->process_frame(frame, crops) : false;
                        auto end_face = std::chrono::steady_clock::now();
                        double face_ms = std::chrono::duration<double, std::milli>(end_face - start_face).count();

                        double gaze_ms = 0.0;
                        data->face_detected = success && crops.face_detected;
                        data->gaze_success = false;

                        if (data->face_detected)
                        {
                            data->head_translation = crops.head_pose_translation;
                            data->head_rotation = crops.head_pose_rotation;

                            // Zero-copy writes: write directly into the Godot-backing buffers using our uint8_t pointers
                            if (data->left_eye_buffer)
                            {
                                copy_rgb_to_gbr(crops.left_eye_data, data->left_eye_buffer, EYE_CROP_SIZE * EYE_CROP_SIZE);
                            }
                            if (data->right_eye_buffer)
                            {
                                copy_rgb_to_gbr(crops.right_eye_data, data->right_eye_buffer, EYE_CROP_SIZE * EYE_CROP_SIZE);
                            }

                            // Also populate full_crop_buffer if requested
                            if (data->full_crop_buffer && data->full_crop_bytes >= 160 * 128 * 3)
                            {
                                resize_bgr_to_rgb(frame.data, frame.width, frame.height, data->full_crop_buffer, 160, 128);
                            }

                            GazeVector3 raw_gaze_dir_cam;
                            auto start_gaze = std::chrono::steady_clock::now();
                            bool gaze_success = gaze_estimator ? gaze_estimator->estimate_raw_gaze(crops, raw_gaze_dir_cam) : false;
                            auto end_gaze = std::chrono::steady_clock::now();
                            gaze_ms = std::chrono::duration<double, std::milli>(end_gaze - start_gaze).count();

                            if (gaze_success)
                            {
                                data->gaze_success = true;
                                GazeVector3 origin_cv = (crops.left_eye_center_cam + crops.right_eye_center_cam) * 0.5f;
                                GazeVector3 origin_cam_gaze = Inference::to_camera_space(origin_cv);
                                GazeVector3 dir_cam_gaze = Inference::to_camera_space(raw_gaze_dir_cam);

                                data->gaze_origin = origin_cam_gaze;
                                data->gaze_direction = dir_cam_gaze;
                            }
                        }

                        auto end_total = std::chrono::steady_clock::now();
                        double total_ms = std::chrono::duration<double, std::milli>(end_total - start_total).count();

                        static int stats_count = 0;
                        if (stats_count++ % 30 == 0)
                        {
                            log_info("Pipeline_PerformanceStats",
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
