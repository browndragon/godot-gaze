// TODO: Add docs. This is our core "entrypoint" that defines a continuous face- and gaze- analysis loop, the threading model, etc.
#pragma once

#include "../core/camera_interface.hpp"
#include "../core/pipeline_config.hpp"
#include "../core/atomic_mailbox.hpp"
#include "../core/pool.hpp"
#include "../core/gaze_frame_data.hpp"
#include "ort_yunet_pipeline.hpp"
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
        std::unique_ptr<ORTYuNetPipeline> face_detector;
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

        void _worker_loop();

    public:
        Pool<GazeFrameData, 2> frame_pool;

        GazeTrackingPipeline() = default;
        ~GazeTrackingPipeline();

        bool initialize(const std::vector<uint8_t> &yunet_model_data, const std::vector<uint8_t> &gaze_model_data);
        void start();
        void stop();

        void set_config(const PipelineConfig &p_config);
        void push_frame_request(GazeFrameData *p_req);
        bool pop_result(GazeFrameData** out_res);
        void clear_work_queue();
        bool is_busy() const { return worker_busy.load() || request_mailbox.is_pending(); }
    };

} // namespace Gaze
