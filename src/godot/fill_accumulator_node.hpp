#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include "../core/fill_accumulator.hpp"

namespace godot {

class FillAccumulator : public Node {
    GDCLASS(FillAccumulator, Node);

public:
    enum ProcessCallback {
        PROCESS_MANUAL = 0,
        PROCESS_IDLE = 1,
        PROCESS_PHYSICS = 2,
    };

private:
    Gaze::FillAccumulator impl;
    ProcessCallback process_callback = PROCESS_MANUAL;
    bool last_was_charging = false;
    bool last_is_filled = false;
    bool last_is_empty = true;
    double current_error = 0.0;
    bool is_active_target = false;

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    FillAccumulator();
    virtual ~FillAccumulator() = default;

    void set_fill(double p_fill);
    double get_fill() const;

    double get_velocity() const;

    bool is_filled() const;
    bool is_empty() const;

    void set_fill_rate(double p_rate);
    double get_fill_rate() const;

    void set_drain_rate(double p_rate);
    double get_drain_rate() const;

    void set_fill_sec(double p_sec);
    double get_fill_sec() const;

    void set_drain_sec(double p_sec);
    double get_drain_sec() const;

    void set_fill_acceleration(double p_accel);
    double get_fill_acceleration() const;

    void set_drain_acceleration(double p_accel);
    double get_drain_acceleration() const;

    void set_error_doubling_distance(double p_dist);
    double get_error_doubling_distance() const;

    void set_process_callback(ProcessCallback p_callback);
    ProcessCallback get_process_callback() const;

    void set_target_error(double p_error);
    double get_target_error() const;

    void set_active_target(bool p_active);
    bool is_active_target_dwell() const;

    void process_error_delta(double p_error, double p_delta);
    void process_dwell_delta(bool p_is_active, double p_delta);
    void reset();
};

} // namespace godot

VARIANT_ENUM_CAST(godot::FillAccumulator::ProcessCallback);
