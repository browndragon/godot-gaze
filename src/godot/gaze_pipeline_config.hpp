/**
 * @file gaze_pipeline_config.hpp
 * @brief Godot Resource for pipeline configurations
 *
 * Exposes core tracking parameters (pitch/yaw translation compensation gains,
 * nose depth, IPD, and detection resolution) to Godot scripts as a serializable
 * resource object.
 */
#pragma once
#include <godot_cpp/classes/resource.hpp>
#include "pipeline_config.hpp"

namespace godot {

class GazePipelineConfig : public Resource {
    GDCLASS(GazePipelineConfig, Resource);
private:
    Gaze::PipelineConfig config;
protected:
    static void _bind_methods();
public:
    GazePipelineConfig() = default;
    virtual ~GazePipelineConfig() = default;

    void set_pitch_t_gain(double val);
    double get_pitch_t_gain() const;

    void set_yaw_t_gain(double val);
    double get_yaw_t_gain() const;

    void set_nose_z(double val);
    double get_nose_z() const;

    void set_ipd_mm(double val);
    double get_ipd_mm() const;

    void set_face_detect_width(int val);
    int get_face_detect_width() const;

    void set_face_detect_height(int val);
    int get_face_detect_height() const;

    Gaze::PipelineConfig get_config() const { return config; }
};

} // namespace godot
