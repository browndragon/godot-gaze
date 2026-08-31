/**
 * @file gaze_calibration_resource.hpp
 * @brief Serializable Godot Resource wrapping decoupled calibration systems
 */
#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/classes/ref.hpp>

namespace godot {

class DeviceCalibration : public Resource {
    GDCLASS(DeviceCalibration, Resource);

protected:
    Vector2 physical_size_mm = Vector2(-1.0, -1.0);
    Vector2i logical_size_px = Vector2i(0, 0);
    Vector3 camera_offset = Vector3(-1000.0, -1000.0, -1000.0);
    double camera_tilt = -1000.0;

    static void _bind_methods();

public:
    DeviceCalibration() = default;
    virtual ~DeviceCalibration() = default;

    virtual Vector2 get_physical_size_mm() const;
    virtual void set_physical_size_mm(Vector2 p_size);

    virtual Vector2i get_logical_size_px() const;
    virtual void set_logical_size_px(Vector2i p_size);

    virtual Vector3 get_camera_offset() const;
    virtual void set_camera_offset(Vector3 p_offset);

    virtual double get_camera_tilt() const;
    virtual void set_camera_tilt(double p_tilt);

    virtual Vector2 get_window_position() const;

    Vector2 get_pixel_size_mm() const;
    Vector2 get_dpi() const;

    static double get_focal_length_under_scaling_static(double f_original, double original_dim, double new_dim);
    static double get_card_width_px_static(double fov_degrees, double card_distance_mm, double frame_width, double card_width_mm = 85.603);
    static double diagonal_to_horizontal_fov_static(double diagonal_fov_degrees, double width, double height);
};

class GuessDeviceCalibration : public DeviceCalibration {
    GDCLASS(GuessDeviceCalibration, DeviceCalibration);

protected:
    static void _bind_methods();

public:
    GuessDeviceCalibration() = default;
    virtual ~GuessDeviceCalibration() = default;

    virtual Vector2 get_physical_size_mm() const override;
    virtual Vector2i get_logical_size_px() const override;
    virtual Vector3 get_camera_offset() const override;
    virtual double get_camera_tilt() const override;
    virtual Vector2 get_window_position() const override;
};

class StoredDeviceCalibration : public DeviceCalibration {
    GDCLASS(StoredDeviceCalibration, DeviceCalibration);

protected:
    static void _bind_methods();

public:
    StoredDeviceCalibration();
    virtual ~StoredDeviceCalibration() = default;

    virtual Vector2 get_window_position() const override;
};

class MockDeviceCalibration : public DeviceCalibration {
    GDCLASS(MockDeviceCalibration, DeviceCalibration);

private:
    Vector2 window_position_px = Vector2(0.0, 0.0);

protected:
    static void _bind_methods();

public:
    MockDeviceCalibration();
    virtual ~MockDeviceCalibration() = default;

    void set_window_position(Vector2 p_pos) { window_position_px = p_pos; emit_changed(); }
    virtual Vector2 get_window_position() const override { return window_position_px; }
};

class DefaultDeviceCalibration : public DeviceCalibration {
    GDCLASS(DefaultDeviceCalibration, DeviceCalibration);

protected:
    mutable Ref<DeviceCalibration> cached_calibration;
    static void _bind_methods();

public:
    DefaultDeviceCalibration() = default;
    virtual ~DefaultDeviceCalibration() = default;

    Ref<DeviceCalibration> get_actual_calibration() const;
    void clear_cache();

    virtual Vector2 get_physical_size_mm() const override;
    virtual void set_physical_size_mm(Vector2 p_size) override;

    virtual Vector2i get_logical_size_px() const override;
    virtual void set_logical_size_px(Vector2i p_size) override;

    virtual Vector3 get_camera_offset() const override;
    virtual void set_camera_offset(Vector3 p_offset) override;

    virtual double get_camera_tilt() const override;
    virtual void set_camera_tilt(double p_tilt) override;

    virtual Vector2 get_window_position() const override;
};

class BioCalibration : public Resource {
    GDCLASS(BioCalibration, Resource);

protected:
    double bias_pitch = 0.0;
    double bias_yaw = 0.0;
    double scale_pitch = 1.0;
    double scale_yaw = 1.0;

    static void _bind_methods();

public:
    BioCalibration() = default;
    virtual ~BioCalibration() = default;

    virtual double get_bias_pitch() const { return bias_pitch; }
    virtual double get_bias_yaw() const { return bias_yaw; }
    virtual double get_scale_pitch() const { return scale_pitch; }
    virtual double get_scale_yaw() const { return scale_yaw; }

    virtual void set_bias_pitch(double val) { bias_pitch = val; emit_changed(); }
    virtual void set_bias_yaw(double val) { bias_yaw = val; emit_changed(); }
    virtual void set_scale_pitch(double val) { scale_pitch = val; emit_changed(); }
    virtual void set_scale_yaw(double val) { scale_yaw = val; emit_changed(); }
};

class GuessBioCalibration : public BioCalibration {
    GDCLASS(GuessBioCalibration, BioCalibration);

protected:
    static void _bind_methods() {}

public:
    GuessBioCalibration() = default;
    virtual ~GuessBioCalibration() = default;
};

class StoredBioCalibration : public BioCalibration {
    GDCLASS(StoredBioCalibration, BioCalibration);

protected:
    static void _bind_methods() {}

public:
    StoredBioCalibration() = default;
    virtual ~StoredBioCalibration() = default;
};

class DefaultBioCalibration : public BioCalibration {
    GDCLASS(DefaultBioCalibration, BioCalibration);

protected:
    mutable Ref<BioCalibration> cached_calibration;
    static void _bind_methods();

public:
    DefaultBioCalibration() = default;
    virtual ~DefaultBioCalibration() = default;

    Ref<BioCalibration> get_actual_calibration() const;
    void clear_cache();

    virtual double get_bias_pitch() const override;
    virtual double get_bias_yaw() const override;
    virtual double get_scale_pitch() const override;
    virtual double get_scale_yaw() const override;

    virtual void set_bias_pitch(double val) override;
    virtual void set_bias_yaw(double val) override;
    virtual void set_scale_pitch(double val) override;
    virtual void set_scale_yaw(double val) override;
};

class GazeDeviceEstimatedCalibration : public Object {
    GDCLASS(GazeDeviceEstimatedCalibration, Object);

private:
    Ref<DeviceCalibration> calibration;

protected:
    static void _bind_methods();

public:
    GazeDeviceEstimatedCalibration();
    virtual ~GazeDeviceEstimatedCalibration() = default;

    Ref<DeviceCalibration> get_calibration() const { return calibration; }
};

} // namespace godot
