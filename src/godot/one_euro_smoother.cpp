#include "one_euro_smoother.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <cmath>

namespace godot {

// --- OneEuroFilterState ---

void OneEuroFilterState::_bind_methods() {
    // Internal helper state class; no methods need to be bound.
}

OneEuroFilterState::OneEuroFilterState() {
    impl = new OneEuroFilter(60.0, 1.0, 0.01, 1.0);
}

OneEuroFilterState::~OneEuroFilterState() {
    if (impl) {
        delete impl;
        impl = nullptr;
    }
}


// --- OneEuroSmoother ---

void OneEuroSmoother::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_min_cutoff"), &OneEuroSmoother::get_min_cutoff);
    ClassDB::bind_method(D_METHOD("set_min_cutoff", "val"), &OneEuroSmoother::set_min_cutoff);
    ClassDB::bind_method(D_METHOD("get_beta"), &OneEuroSmoother::get_beta);
    ClassDB::bind_method(D_METHOD("set_beta", "val"), &OneEuroSmoother::set_beta);
    ClassDB::bind_method(D_METHOD("get_d_cutoff"), &OneEuroSmoother::get_d_cutoff);
    ClassDB::bind_method(D_METHOD("set_d_cutoff", "val"), &OneEuroSmoother::set_d_cutoff);

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_cutoff"), "set_min_cutoff", "get_min_cutoff");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "beta"), "set_beta", "get_beta");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "d_cutoff"), "set_d_cutoff", "get_d_cutoff");
}

OneEuroSmoother::OneEuroSmoother() {}

OneEuroSmoother::~OneEuroSmoother() {}

Array OneEuroSmoother::_smoother_init() {
    Array state;
    state.resize(2);
    
    Ref<OneEuroFilterState> x_state;
    x_state.instantiate();
    x_state->impl->set_min_cutoff(min_cutoff);
    x_state->impl->set_beta(beta);
    x_state->impl->set_d_cutoff(d_cutoff);

    Ref<OneEuroFilterState> y_state;
    y_state.instantiate();
    y_state->impl->set_min_cutoff(min_cutoff);
    y_state->impl->set_beta(beta);
    y_state->impl->set_d_cutoff(d_cutoff);

    state[0] = x_state;
    state[1] = y_state;
    
    return state;
}

Variant OneEuroSmoother::_smoother_next(Array state, double tstamp, Variant value) {
    if (state.size() < 2) return value;
    
    Ref<OneEuroFilterState> x_state = state[0];
    Ref<OneEuroFilterState> y_state = state[1];
    
    if (x_state.is_null() || y_state.is_null() || !x_state->impl || !y_state->impl) {
        return value;
    }

    // Keep filter parameters updated in case properties changed in the resource
    x_state->impl->set_min_cutoff(min_cutoff);
    x_state->impl->set_beta(beta);
    x_state->impl->set_d_cutoff(d_cutoff);
    
    y_state->impl->set_min_cutoff(min_cutoff);
    y_state->impl->set_beta(beta);
    y_state->impl->set_d_cutoff(d_cutoff);

    if (value.get_type() == Variant::VECTOR2) {
        Vector2 vec = value;
        double fx = x_state->impl->filter(vec.x, tstamp);
        double fy = y_state->impl->filter(vec.y, tstamp);
        return Vector2(fx, fy);
    } else if (value.get_type() == Variant::FLOAT) {
        double val = value;
        double f = x_state->impl->filter(val, tstamp);
        return f;
    }
    return value;
}

void OneEuroSmoother::set_min_cutoff(double val) { min_cutoff = val; }
double OneEuroSmoother::get_min_cutoff() const { return min_cutoff; }

void OneEuroSmoother::set_beta(double val) { beta = val; }
double OneEuroSmoother::get_beta() const { return beta; }

void OneEuroSmoother::set_d_cutoff(double val) { d_cutoff = val; }
double OneEuroSmoother::get_d_cutoff() const { return d_cutoff; }

} // namespace godot
