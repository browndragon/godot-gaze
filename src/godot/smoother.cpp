#include "smoother.hpp"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void Smoother::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_smoother_init"), &Smoother::_smoother_init);
    ClassDB::bind_method(D_METHOD("_smoother_next", "state", "tstamp", "value"), &Smoother::_smoother_next);
}

Smoother::Smoother() {}

Smoother::~Smoother() {}

Array Smoother::_smoother_init() {
    return Array();
}

Variant Smoother::_smoother_next(Array state, double tstamp, Variant value) {
    return value;
}

} // namespace godot
