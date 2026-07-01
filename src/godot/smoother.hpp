#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot {

class Smoother : public Resource {
    GDCLASS(Smoother, Resource);

protected:
    static void _bind_methods();

public:
    Smoother();
    virtual ~Smoother();

    // Allocates and returns a new mutable state array
    virtual Array _smoother_init();

    // Filters value with the given timestamp and updates state array in-place
    virtual Variant _smoother_next(Array state, double tstamp, Variant value);
};

} // namespace godot
