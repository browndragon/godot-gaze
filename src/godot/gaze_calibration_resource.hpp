// Wraps core calibration data structure into a serializable Godot Resource.
#pragma once

#include <godot_cpp/classes/resource.hpp>
#include "gaze_calibration.hpp"

namespace godot {

class GazeCalibrationResource : public Resource {
    GDCLASS(GazeCalibrationResource, Resource);

private:
    Gaze::GazeCalibration calibration;

protected:
    static void _bind_methods();

public:
    GazeCalibrationResource() = default;
    virtual ~GazeCalibrationResource() = default;

    void set_bias_pitch(double val);
    double get_bias_pitch() const;

    void set_bias_yaw(double val);
    double get_bias_yaw() const;

    void set_bias_pixel_x(double val);
    double get_bias_pixel_x() const;

    void set_bias_pixel_y(double val);
    double get_bias_pixel_y() const;

    Gaze::GazeCalibration get_calibration() const;
    void set_calibration(const Gaze::GazeCalibration& c);
};

} // namespace godot
