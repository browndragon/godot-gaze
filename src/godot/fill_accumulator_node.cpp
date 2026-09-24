#include "fill_accumulator_node.hpp"

namespace godot {

void FillAccumulator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_fill", "fill"), &FillAccumulator::set_fill);
    ClassDB::bind_method(D_METHOD("get_fill"), &FillAccumulator::get_fill);

    ClassDB::bind_method(D_METHOD("get_velocity"), &FillAccumulator::get_velocity);
    ClassDB::bind_method(D_METHOD("is_filled"), &FillAccumulator::is_filled);
    ClassDB::bind_method(D_METHOD("is_empty"), &FillAccumulator::is_empty);

    ClassDB::bind_method(D_METHOD("set_fill_rate", "rate"), &FillAccumulator::set_fill_rate);
    ClassDB::bind_method(D_METHOD("get_fill_rate"), &FillAccumulator::get_fill_rate);

    ClassDB::bind_method(D_METHOD("set_drain_rate", "rate"), &FillAccumulator::set_drain_rate);
    ClassDB::bind_method(D_METHOD("get_drain_rate"), &FillAccumulator::get_drain_rate);

    ClassDB::bind_method(D_METHOD("set_fill_sec", "sec"), &FillAccumulator::set_fill_sec);
    ClassDB::bind_method(D_METHOD("get_fill_sec"), &FillAccumulator::get_fill_sec);

    ClassDB::bind_method(D_METHOD("set_drain_sec", "sec"), &FillAccumulator::set_drain_sec);
    ClassDB::bind_method(D_METHOD("get_drain_sec"), &FillAccumulator::get_drain_sec);

    ClassDB::bind_method(D_METHOD("set_fill_acceleration", "accel"), &FillAccumulator::set_fill_acceleration);
    ClassDB::bind_method(D_METHOD("get_fill_acceleration"), &FillAccumulator::get_fill_acceleration);

    ClassDB::bind_method(D_METHOD("set_drain_acceleration", "accel"), &FillAccumulator::set_drain_acceleration);
    ClassDB::bind_method(D_METHOD("get_drain_acceleration"), &FillAccumulator::get_drain_acceleration);

    ClassDB::bind_method(D_METHOD("set_error_doubling_distance", "dist"), &FillAccumulator::set_error_doubling_distance);
    ClassDB::bind_method(D_METHOD("get_error_doubling_distance"), &FillAccumulator::get_error_doubling_distance);

    ClassDB::bind_method(D_METHOD("set_process_callback", "callback"), &FillAccumulator::set_process_callback);
    ClassDB::bind_method(D_METHOD("get_process_callback"), &FillAccumulator::get_process_callback);

    ClassDB::bind_method(D_METHOD("set_target_error", "error"), &FillAccumulator::set_target_error);
    ClassDB::bind_method(D_METHOD("get_target_error"), &FillAccumulator::get_target_error);

    ClassDB::bind_method(D_METHOD("set_active_target", "active"), &FillAccumulator::set_active_target);
    ClassDB::bind_method(D_METHOD("is_active_target"), &FillAccumulator::is_active_target_dwell);

    ClassDB::bind_method(D_METHOD("process_error_delta", "error", "delta"), &FillAccumulator::process_error_delta);
    ClassDB::bind_method(D_METHOD("process_dwell_delta", "is_active", "delta"), &FillAccumulator::process_dwell_delta);
    ClassDB::bind_method(D_METHOD("reset"), &FillAccumulator::reset);

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fill"), "set_fill", "get_fill");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fill_rate"), "set_fill_rate", "get_fill_rate");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "drain_rate"), "set_drain_rate", "get_drain_rate");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fill_acceleration"), "set_fill_acceleration", "get_fill_acceleration");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "drain_acceleration"), "set_drain_acceleration", "get_drain_acceleration");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "error_doubling_distance"), "set_error_doubling_distance", "get_error_doubling_distance");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "process_callback", PROPERTY_HINT_ENUM, "Manual,Idle,Physics"), "set_process_callback", "get_process_callback");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "target_error"), "set_target_error", "get_target_error");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "active_target"), "set_active_target", "is_active_target");

    ADD_SIGNAL(MethodInfo("fill_changed", PropertyInfo(Variant::FLOAT, "fill")));
    ADD_SIGNAL(MethodInfo("fill_started"));
    ADD_SIGNAL(MethodInfo("filled"));
    ADD_SIGNAL(MethodInfo("emptied"));

    BIND_ENUM_CONSTANT(PROCESS_MANUAL);
    BIND_ENUM_CONSTANT(PROCESS_IDLE);
    BIND_ENUM_CONSTANT(PROCESS_PHYSICS);
}

FillAccumulator::FillAccumulator() {
    impl.set_fill_rate(1.6);
    impl.set_drain_rate(2.0);
    impl.set_fill_acceleration(32.0);
    impl.set_drain_acceleration(32.0);
    impl.set_error_doubling_distance(100.0);
}

void FillAccumulator::set_fill(double p_fill) {
    impl.set_fill(p_fill);
}

double FillAccumulator::get_fill() const {
    return impl.get_fill();
}

double FillAccumulator::get_velocity() const {
    return impl.get_velocity();
}

bool FillAccumulator::is_filled() const {
    return impl.is_filled();
}

bool FillAccumulator::is_empty() const {
    return impl.is_empty();
}

void FillAccumulator::set_fill_rate(double p_rate) {
    impl.set_fill_rate(p_rate);
}

double FillAccumulator::get_fill_rate() const {
    return impl.get_fill_rate();
}

void FillAccumulator::set_drain_rate(double p_rate) {
    impl.set_drain_rate(p_rate);
}

double FillAccumulator::get_drain_rate() const {
    return impl.get_drain_rate();
}

void FillAccumulator::set_fill_sec(double p_sec) {
    impl.set_fill_sec(p_sec);
}

double FillAccumulator::get_fill_sec() const {
    return impl.get_fill_sec();
}

void FillAccumulator::set_drain_sec(double p_sec) {
    impl.set_drain_sec(p_sec);
}

double FillAccumulator::get_drain_sec() const {
    return impl.get_drain_sec();
}

void FillAccumulator::set_fill_acceleration(double p_accel) {
    impl.set_fill_acceleration(p_accel);
}

double FillAccumulator::get_fill_acceleration() const {
    return impl.get_fill_acceleration();
}

void FillAccumulator::set_drain_acceleration(double p_accel) {
    impl.set_drain_acceleration(p_accel);
}

double FillAccumulator::get_drain_acceleration() const {
    return impl.get_drain_acceleration();
}

void FillAccumulator::set_error_doubling_distance(double p_dist) {
    impl.set_error_doubling_distance(p_dist);
}

double FillAccumulator::get_error_doubling_distance() const {
    return impl.get_error_doubling_distance();
}

void FillAccumulator::set_process_callback(ProcessCallback p_callback) {
    process_callback = p_callback;
    set_process_internal(process_callback == PROCESS_IDLE);
    set_physics_process_internal(process_callback == PROCESS_PHYSICS);
}

FillAccumulator::ProcessCallback FillAccumulator::get_process_callback() const {
    return process_callback;
}

void FillAccumulator::set_target_error(double p_error) {
    current_error = p_error;
}

double FillAccumulator::get_target_error() const {
    return current_error;
}

void FillAccumulator::set_active_target(bool p_active) {
    is_active_target = p_active;
}

bool FillAccumulator::is_active_target_dwell() const {
    return is_active_target;
}

void FillAccumulator::process_error_delta(double p_error, double p_delta) {
    if (p_delta <= 0.0) return;

    double prev_fill = impl.get_fill();
    bool is_charging = (p_error <= 0.0);

    if (is_charging && !last_was_charging && prev_fill < 1.0) {
        emit_signal("fill_started");
    }
    last_was_charging = is_charging;

    impl.process_error_delta(p_error, p_delta);
    double new_fill = impl.get_fill();

    if (new_fill != prev_fill) {
        emit_signal("fill_changed", new_fill);
    }

    if (new_fill >= 0.9999 && !last_is_filled) {
        last_is_filled = true;
        emit_signal("filled");
    } else if (new_fill < 0.9999) {
        last_is_filled = false;
    }

    if (new_fill <= 0.0001 && !last_is_empty) {
        last_is_empty = true;
        emit_signal("emptied");
    } else if (new_fill > 0.0001) {
        last_is_empty = false;
    }
}

void FillAccumulator::process_dwell_delta(bool p_is_active, double p_delta) {
    if (p_delta <= 0.0) return;

    double prev_fill = impl.get_fill();
    bool is_charging = p_is_active;

    if (is_charging && !last_was_charging && prev_fill < 1.0) {
        emit_signal("fill_started");
    }
    last_was_charging = is_charging;

    impl.process_dwell_delta(p_is_active, p_delta);
    double new_fill = impl.get_fill();

    if (new_fill != prev_fill) {
        emit_signal("fill_changed", new_fill);
    }

    if (new_fill >= 0.9999 && !last_is_filled) {
        last_is_filled = true;
        emit_signal("filled");
    } else if (new_fill < 0.9999) {
        last_is_filled = false;
    }

    if (new_fill <= 0.0001 && !last_is_empty) {
        last_is_empty = true;
        emit_signal("emptied");
    } else if (new_fill > 0.0001) {
        last_is_empty = false;
    }
}

void FillAccumulator::reset() {
    impl.reset();
    last_was_charging = false;
    last_is_filled = false;
    last_is_empty = true;
}

void FillAccumulator::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_INTERNAL_PROCESS: {
            if (process_callback == PROCESS_IDLE) {
                double delta = get_process_delta_time();
                if (is_active_target) {
                    process_dwell_delta(true, delta);
                } else if (current_error > 0.0) {
                    process_error_delta(current_error, delta);
                } else {
                    process_dwell_delta(false, delta);
                }
            }
            break;
        }
        case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
            if (process_callback == PROCESS_PHYSICS) {
                double delta = get_physics_process_delta_time();
                if (is_active_target) {
                    process_dwell_delta(true, delta);
                } else if (current_error > 0.0) {
                    process_error_delta(current_error, delta);
                } else {
                    process_dwell_delta(false, delta);
                }
            }
            break;
        }
        default:
            break;
    }
}

} // namespace godot
