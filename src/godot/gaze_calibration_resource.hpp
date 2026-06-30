/**
 * @file gaze_calibration_resource.hpp
 * @brief Serializable Godot Resource wrapping polymorphic Gaze Calibration data
 */
#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <memory>
#include "gaze_calibration.hpp"

namespace godot {

class GazeCalibration : public Resource {
    GDCLASS(GazeCalibration, Resource);

protected:
    std::shared_ptr<Gaze::Calibration> impl;

    static void _bind_methods();

public:
    GazeCalibration();
    explicit GazeCalibration(std::shared_ptr<Gaze::Calibration> p_impl);
    virtual ~GazeCalibration() = default;

    std::shared_ptr<Gaze::Calibration> get_impl() const { return impl; }

    void set_pixel_size_mm(Vector2 val);
    Vector2 get_pixel_size_mm(Object* tracker = nullptr) const;
    Vector2 get_pixel_size_mm_bind() const { return get_pixel_size_mm(nullptr); }

    void set_camera_offset(Vector3 val);
    Vector3 get_camera_offset(Object* tracker = nullptr) const;
    Vector3 get_camera_offset_bind() const { return get_camera_offset(nullptr); }

    void set_camera_tilt(double val);
    double get_camera_tilt(Object* tracker = nullptr) const;
    double get_camera_tilt_bind() const { return get_camera_tilt(nullptr); }

    void set_bias_pitch(double val);
    double get_bias_pitch() const;

    void set_bias_yaw(double val);
    double get_bias_yaw() const;

    void set_bias_pixel_x(double val);
    double get_bias_pixel_x() const;

    void set_bias_pixel_y(double val);
    double get_bias_pixel_y() const;
};

class GazeDeviceCalibration : public GazeCalibration {
    GDCLASS(GazeDeviceCalibration, GazeCalibration);

protected:
    static void _bind_methods();

public:
    GazeDeviceCalibration();
    virtual ~GazeDeviceCalibration() = default;

    std::shared_ptr<Gaze::DeviceCalibration> get_device_impl() const {
        return std::static_pointer_cast<Gaze::DeviceCalibration>(impl);
    }

    void set_pixel_size_mm_override(Vector2 val);
    Vector2 get_pixel_size_mm_override() const;

    void set_camera_offset_override(Vector3 val);
    Vector3 get_camera_offset_override() const;

    void set_camera_tilt_override(double val);
    double get_camera_tilt_override() const;
};

class GazeDeviceEstimatedCalibration : public Object {
    GDCLASS(GazeDeviceEstimatedCalibration, Object);

private:
    Ref<GazeDeviceCalibration> calibration;

protected:
    static void _bind_methods();

public:
    GazeDeviceEstimatedCalibration();
    virtual ~GazeDeviceEstimatedCalibration() = default;

    Ref<GazeDeviceCalibration> get_calibration() const { return calibration; }
};

} // namespace godot
