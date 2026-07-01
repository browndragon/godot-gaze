#pragma once

#include "gaze_model.hpp"
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <vector>
#include <string>

namespace Gaze
{

    // Helper function to access the shared Ort Env singleton
    inline Ort::Env &get_ort_env()
    {
        static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "GodotGaze");
        return env;
    }

    class ORTGazeModel : public GazeModel
    {
    private:
        std::string model_path;
        std::vector<uint8_t> model_buffer;
        bool load_from_buffer = false;

        std::unique_ptr<Ort::Session> session;
        Ort::MemoryInfo memory_info{nullptr};
        PipelineConfig config;

        // Input/Output Node Names
        const std::vector<const char *> input_names = {"left_eye_image", "right_eye_image", "head_pose_angles"};
        const std::vector<const char *> output_names = {"gaze_vector/sink_port_0"};

        void preprocess_eye_crop(const uint8_t *raw_crop, float *out_buffer);

    public:
        ORTGazeModel(const std::string &gaze_ort_path);
        ORTGazeModel(const std::vector<uint8_t> &ort_buffer);
        ORTGazeModel(const std::vector<uint8_t> &xml_buffer, const std::vector<uint8_t> &bin_buffer); // Compatibility fallback
        virtual ~ORTGazeModel() = default;

        virtual bool initialize() override;
        virtual bool estimate_raw_gaze(const EyeCrops &crops, GazeVector3 &out_gaze_dir_cv) override;
        virtual void set_config(const PipelineConfig &cfg) override { config = cfg; }
    };

} // namespace Gaze
