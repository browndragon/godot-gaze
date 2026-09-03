// TODO: Add docs. This is our core "entrypoint" that defines a continuous face- and gaze- analysis loop, the threading model, etc.
#pragma once

#include "../core/camera_interface.hpp"
#include "../core/pipeline_config.hpp"
#include "../core/concurrency/atomic_mailbox.hpp"
#include "../core/concurrency/pool.hpp"
#include "../core/gaze_frame_data.hpp"
#include "ort_yunet_detector.hpp"
#include "ort_landmark_model.hpp"
#include "ort_eye_state_model.hpp"
#include "ort_gaze_model.hpp"
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

namespace Gaze
{

    class GazeTrackingPipeline
    {
    private:
        std::unique_ptr<ORTYuNetDetector> face_detector;
        std::unique_ptr<ORTLandmarkModel> landmark_model;
        std::unique_ptr<ORTEyeStateModel> eye_state_model;
        std::unique_ptr<ORTGazeModel> gaze_estimator;

        std::thread worker_thread;
        std::atomic<bool> thread_running{false};
        mutable std::mutex state_mutex;
        std::condition_variable worker_cv;
        std::mutex worker_mutex;
        std::mutex lifecycle_mutex;
        bool frame_pending = false;

        AtomicMailbox<GazeFrameData *> request_mailbox;
        AtomicMailbox<GazeFrameData *> results_mailbox;
        PipelineConfig active_config;
        bool initialized = false;
        bool config_dirty = false;
        std::atomic<bool> worker_busy{false};
        float pipeline_roll_rad = 0.0f;

        void _worker_loop();
        void _stage_1_apply_roll_hint(GazeFrameData *data, Frame &working_frame);
        bool _stage_2_detect_face_bbox(GazeFrameData *data, const Frame &working_frame);
        bool _stage_3_extract_landmarks(GazeFrameData *data, const Frame &working_frame);
        bool _stage_4_solve_head_pose(GazeFrameData *data, const Frame &working_frame, OpenCVCameraVector3 &out_rvec, OpenCVCameraVector3 &out_tvec);
        void _stage_5_extract_eye_crops(GazeFrameData *data, const Frame &working_frame, const OpenCVCameraVector3 &rvec, const OpenCVCameraVector3 &tvec);
        void _stage_6_estimate_eye_state(GazeFrameData *data);
        void _stage_7_estimate_gaze_direction(GazeFrameData *data);
        void _stage_8_unroll_to_canonical_godot_camera(GazeFrameData *data);

    public:
        Pool<GazeFrameData, 2> frame_pool;

        GazeTrackingPipeline() = default;
        ~GazeTrackingPipeline();

        bool initialize(
            const std::vector<uint8_t> &yunet_model_data,
            const std::vector<uint8_t> &gaze_model_data,
            const std::vector<uint8_t> &eye_openness_model_data = {},
            const std::vector<uint8_t> &landmark_model_data = {});
        bool initialize(
            const std::string &yunet_model_path,
            const std::string &gaze_model_path,
            const std::string &eye_openness_model_path = {},
            const std::string &landmark_model_path = {});
        void start();
        void stop();

        void process_frame_synchronous(GazeFrameData *data);

        void set_config(const PipelineConfig &p_config);
        void push_frame_request(GazeFrameData *p_req);
        bool pop_result(GazeFrameData** out_res);
        void clear_work_queue();
        void reset_tracker() { pipeline_roll_rad = 0.0f; }
        bool is_initialized() const { return initialized; }
        bool is_busy() const { return worker_busy.load() || request_mailbox.is_pending(); }
    };

} // namespace Gaze
