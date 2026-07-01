/**
 * @file gaze_calibration_session.hpp
 * @brief Godot Resource for tracking calibration sampling sessions
 */
#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

class GazeTracker;

class GazeCalibrationSession : public Resource {
    GDCLASS(GazeCalibrationSession, Resource);

private:
    bool freeze_camera_params = true;
    Array target_pixels_px;
    Array gaze_origins;
    Array gaze_directions;

protected:
    static void _bind_methods();

public:
    GazeCalibrationSession() = default;
    virtual ~GazeCalibrationSession() = default;

    void add_sample(Vector2 target_pixel_px, Vector3 gaze_origin, Vector3 gaze_direction);
    void clear();
    int get_sample_count() const;

    Dictionary calculate_calibration(GazeTracker *tracker);

    // Getters and setters for properties to allow serialization
    void set_freeze_camera_params(bool p_freeze) { freeze_camera_params = p_freeze; }
    bool get_freeze_camera_params() const { return freeze_camera_params; }

    void set_target_pixels_px(const Array& arr) { target_pixels_px = arr; }
    Array get_target_pixels_px() const { return target_pixels_px; }

    void set_gaze_origins(const Array& arr) { gaze_origins = arr; }
    Array get_gaze_origins() const { return gaze_origins; }

    void set_gaze_directions(const Array& arr) { gaze_directions = arr; }
    Array get_gaze_directions() const { return gaze_directions; }
};

} // namespace godot
