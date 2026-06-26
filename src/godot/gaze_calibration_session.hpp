/**
 * @file gaze_calibration_session.hpp
 * @brief Godot Resource for tracking calibration sampling sessions
 *
 * Collects target pixel coordinates, gaze origins, and raw gaze direction vectors
 * during calibration sequences. Uses collected sample points to calculate pitch/yaw
 * and pixel biases.
 */
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
    Array gaze_origins;
    Array gaze_directions;

protected:
    static void _bind_methods();

public:
    GazeCalibrationSession() = default;
    virtual ~GazeCalibrationSession() = default;

    void add_sample(Vector2 target_pixel, Vector3 gaze_origin, Vector3 gaze_direction);
    void clear();
    int get_sample_count() const;

    Ref<GazeCalibrationResource> calculate_calibration(GazeTracker *tracker, bool use_3d);

    // Getters and setters for properties to allow serialization
    void set_target_pixels(const Array& arr) { target_pixels = arr; }
    Array get_target_pixels() const { return target_pixels; }

    void set_gaze_origins(const Array& arr) { gaze_origins = arr; }
    Array get_gaze_origins() const { return gaze_origins; }

    void set_gaze_directions(const Array& arr) { gaze_directions = arr; }
    Array get_gaze_directions() const { return gaze_directions; }
};

} // namespace godot
