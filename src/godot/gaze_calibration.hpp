/**
 * @file gaze_calibration.hpp
 * @brief Standalone calibration worker for collecting calibration points and solving Angle Kappa
 */
#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <vector>
#include "gaze_bio_profile.hpp"
#include "input_event_gaze.hpp"
#include "../core/damped_spring.hpp"
#include "../core/fill_accumulator.hpp"

namespace godot {

class GazeCalibration : public RefCounted {
    GDCLASS(GazeCalibration, RefCounted);

public:
    struct Sample {
        Vector2 target_px;
        double target_pitch = 0.0;
        double target_yaw = 0.0;
        double meas_pitch = 0.0;
        double meas_yaw = 0.0;
    };

    enum Stage {
        STAGE_IDLE = 0,
        STAGE_SETTLING = 1,
        STAGE_SAMPLING = 2
    };

private:
    std::vector<Sample> samples;

    // Continuous Two-Stage Pipeline
    Gaze::DampedSpring spring;
    Gaze::FillAccumulator settle_accumulator{0.40, 0.25};
    Gaze::FillAccumulator sample_accumulator{0.40, 0.25};

    Vector2 current_target_px{0.0f, 0.0f};
    double target_radius_px = 60.0;
    bool has_active_target = false;
    Stage current_stage = STAGE_IDLE;
    bool point_just_completed = false;

    // Running circular-mean sums for active target
    double active_sum_sin_pitch = 0.0;
    double active_sum_cos_pitch = 0.0;
    double active_sum_sin_yaw = 0.0;
    double active_sum_cos_yaw = 0.0;
    int active_sample_count = 0;

    void _add_sample(const Vector2 &p_target_px, const Transform3D &p_eye_transform, const Transform3D &p_head_transform);
    void _compute_angles(const Vector2 &p_target_px, const Transform3D &p_eye_transform, const Transform3D &p_head_transform,
                         double &r_targ_pitch, double &r_targ_yaw, double &r_meas_pitch, double &r_meas_yaw);
    void _reset_active_sums();

protected:
    static void _bind_methods();

public:
    GazeCalibration();
    virtual ~GazeCalibration() = default;

    // Discrete snapshot API
    void add_event(const Ref<InputEventGaze> &p_event, const Vector2 &p_target_px = Vector2(NAN, NAN));
    void add_point(const Vector2 &p_target_px, const Ref<InputEventGaze> &p_event);
    void add_point_transforms(const Vector2 &p_target_px, const Transform3D &p_eye_transform, const Transform3D &p_head_transform);
    void clear();
    int get_sample_count() const { return static_cast<int>(samples.size()); }

    // Continuous Two-Stage API
    void set_target(const Vector2 &p_target_px, double p_radius_px = 60.0);
    bool add_sample(const Ref<InputEventGaze> &p_event, double p_delta);
    void cancel_target();
    double get_display_fill() const;
    int get_stage() const { return static_cast<int>(current_stage); }
    bool is_point_complete() const { return point_just_completed; }
    Vector2 get_smoothed_gaze() const;
    Vector2 get_target_position() const { return current_target_px; }
    double get_target_radius() const { return target_radius_px; }

    Error calibrate(const Ref<GazeBioProfile> &p_bio_profile);
    Ref<GazeBioProfile> install(const Ref<GazeBioProfile> &p_bio_profile = Ref<GazeBioProfile>(), const String &p_path = "user://calibrations/bio_profile.cfg");
};

} // namespace godot

VARIANT_ENUM_CAST(godot::GazeCalibration::Stage);
