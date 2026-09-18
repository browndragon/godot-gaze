/**
 * @file gaze_bio_profile.hpp
 * @brief Serializable Godot Resource holding biological calibration offsets (Angle Kappa & scale)
 */
#pragma once

#include "gaze_profile.hpp"

namespace godot {

class GazeBioProfile : public GazeProfile {
    GDCLASS(GazeBioProfile, GazeProfile);

private:
    double bias_pitch_deg = 0.0;
    double bias_yaw_deg = 0.0;

protected:
    static void _bind_methods();

    virtual void _write_to_config(Ref<ConfigFile> &p_cfg) const override;
    virtual Error _read_from_config(const Ref<ConfigFile> &p_cfg) override;

public:
    GazeBioProfile() = default;
    virtual ~GazeBioProfile() = default;

    double get_bias_pitch_deg() const { return bias_pitch_deg; }
    void set_bias_pitch_deg(double p_val);

    double get_bias_yaw_deg() const { return bias_yaw_deg; }
    void set_bias_yaw_deg(double p_val);

    void reset();
};

} // namespace godot
