#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/classes/ref.hpp>

namespace godot {

class GazeTracker;
class GazeCalibrationResource;

class GazeCalibrationSession : public Resource {
    GDCLASS(GazeCalibrationSession, Resource);

private:
    Array target_pixels;
    Array left_origins;
    Array left_directions;
    Array right_origins;
    Array right_directions;

protected:
    static void _bind_methods();

public:
    GazeCalibrationSession() = default;
    virtual ~GazeCalibrationSession() = default;

    void add_sample(Vector2 target_pixel, Vector3 left_origin, Vector3 left_direction, Vector3 right_origin, Vector3 right_direction);
    void clear();
    int get_sample_count() const;

    Ref<GazeCalibrationResource> calculate_calibration(GazeTracker *tracker, bool use_3d);

    // Getters and setters for properties to allow serialization
    void set_target_pixels(const Array& arr) { target_pixels = arr; }
    Array get_target_pixels() const { return target_pixels; }

    void set_left_origins(const Array& arr) { left_origins = arr; }
    Array get_left_origins() const { return left_origins; }

    void set_left_directions(const Array& arr) { left_directions = arr; }
    Array get_left_directions() const { return left_directions; }

    void set_right_origins(const Array& arr) { right_origins = arr; }
    Array get_right_origins() const { return right_origins; }

    void set_right_directions(const Array& arr) { right_directions = arr; }
    Array get_right_directions() const { return right_directions; }
};

} // namespace godot
