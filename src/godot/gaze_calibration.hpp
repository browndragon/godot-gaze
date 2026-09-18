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

private:
    std::vector<Sample> samples;

    void _add_sample(const Vector2 &p_target_px, const Transform3D &p_eye_transform, const Transform3D &p_head_transform);

protected:
    static void _bind_methods();

public:
    GazeCalibration() = default;
    virtual ~GazeCalibration() = default;

    void add_event(const Ref<InputEventGaze> &p_event, const Vector2 &p_target_px = Vector2(NAN, NAN));
    void add_point(const Vector2 &p_target_px, const Ref<InputEventGaze> &p_event);
    void add_point_transforms(const Vector2 &p_target_px, const Transform3D &p_eye_transform, const Transform3D &p_head_transform);
    void clear();
    int get_sample_count() const { return static_cast<int>(samples.size()); }

    Error calibrate(const Ref<GazeBioProfile> &p_bio_profile);
    Ref<GazeBioProfile> install(const Ref<GazeBioProfile> &p_bio_profile = Ref<GazeBioProfile>(), const String &p_path = "user://calibrations/bio_profile.cfg");
};

} // namespace godot
