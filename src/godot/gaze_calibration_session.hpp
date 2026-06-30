/**
 * @file gaze_calibration_session.hpp
 * @brief Godot Resource for tracking calibration sampling sessions
 */
#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/classes/ref.hpp>

namespace godot {

class GazeTracker;
class GazeCalibration;

class GazeCalibrationSession : public Resource {
    GDCLASS(GazeCalibrationSession, Resource);

private:
    Array target_pixels_ppix;
    Array gaze_origins;
    Array gaze_directions;

protected:
    static void _bind_methods();

public:
    GazeCalibrationSession() = default;
    virtual ~GazeCalibrationSession() = default;

    void add_sample(Vector2 target_pixel_ppix, Vector3 gaze_origin, Vector3 gaze_direction);
    void clear();
    int get_sample_count() const;

    Ref<GazeCalibration> calculate_calibration(GazeTracker *tracker);

    // Getters and setters for properties to allow serialization
    void set_target_pixels_ppix(const Array& arr) { target_pixels_ppix = arr; }
    Array get_target_pixels_ppix() const { return target_pixels_ppix; }

    void set_gaze_origins(const Array& arr) { gaze_origins = arr; }
    Array get_gaze_origins() const { return gaze_origins; }

    void set_gaze_directions(const Array& arr) { gaze_directions = arr; }
    Array get_gaze_directions() const { return gaze_directions; }
};

} // namespace godot
