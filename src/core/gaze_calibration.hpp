/**
 * @file gaze_calibration.hpp
 * @brief Gaze Calibration parameters and polymorphic interface (Layer 4)
 */
#pragma once

#include "math_defs.hpp"

namespace Gaze {

struct GazeCalibration {
    // Physical display parameters in millimeter-per-pixel size representation
    GazeVector2 pixel_size_mm = GazeVector2(0.25, 0.25);
    GazeVector3 camera_offset = GazeVector3(0.0, 148.0, 0.0);
    double camera_tilt = 0.0;

    // 3D angular pitch and yaw bias corrections (in radians)
    // Applied to the gaze vector before projection.
    double bias_pitch = 0.0;
    double bias_yaw = 0.0;

    // Scale multipliers to dampen/gain the angular pitch/yaw rotation signal
    double scale_yaw = 1.0;
    double scale_pitch = 1.0;

    // 2D screen-space pixel bias corrections
    // Added to the final pixel coordinate after projection.
    double bias_pixel_x = 0.0;
    double bias_pixel_y = 0.0;

    GazeCalibration() = default;
    GazeCalibration(double pitch, double yaw, double px, double py)
        : bias_pitch(pitch), bias_yaw(yaw), bias_pixel_x(px), bias_pixel_y(py) {}
};

class Calibration {
public:
    virtual ~Calibration() = default;

    virtual GazeVector2 get_pixel_size_mm(const GazeVector2& default_size) const = 0;
    virtual GazeVector3 get_camera_offset(const GazeVector3& default_offset) const = 0;
    virtual double get_camera_tilt(double default_tilt) const = 0;

    virtual double get_bias_pitch() const = 0;
    virtual double get_bias_yaw() const = 0;
    virtual double get_scale_yaw() const = 0;
    virtual double get_scale_pitch() const = 0;
    virtual double get_bias_pixel_x() const = 0;
    virtual double get_bias_pixel_y() const = 0;
};

class FixedCalibration : public Calibration {
private:
    GazeCalibration data;

public:
    FixedCalibration() = default;
    explicit FixedCalibration(const GazeCalibration& d) : data(d) {}

    GazeVector2 get_pixel_size_mm(const GazeVector2& default_size) const override { return data.pixel_size_mm; }
    GazeVector3 get_camera_offset(const GazeVector3& default_offset) const override { return data.camera_offset; }
    double get_camera_tilt(double default_tilt) const override { return data.camera_tilt; }

    double get_bias_pitch() const override { return data.bias_pitch; }
    double get_bias_yaw() const override { return data.bias_yaw; }
    double get_scale_yaw() const override { return data.scale_yaw; }
    double get_scale_pitch() const override { return data.scale_pitch; }
    double get_bias_pixel_x() const override { return data.bias_pixel_x; }
    double get_bias_pixel_y() const override { return data.bias_pixel_y; }

    GazeCalibration get_data() const { return data; }
    void set_data(const GazeCalibration& d) { data = d; }
};

class DeviceCalibration : public Calibration {
private:
    // Optional overrides. If set to a negative/sentinel value, they indicate "unset/use tracker default".
    GazeVector2 pixel_size_mm_override = GazeVector2(-1.0, -1.0);
    GazeVector3 camera_offset_override = GazeVector3(-1000.0, -1000.0, -1000.0);
    double camera_tilt_override = -1000.0;

    double bias_pitch = 0.0;
    double bias_yaw = 0.0;
    double scale_yaw = 1.0;
    double scale_pitch = 1.0;
    double bias_pixel_x = 0.0;
    double bias_pixel_y = 0.0;

public:
    DeviceCalibration() = default;

    GazeVector2 get_pixel_size_mm(const GazeVector2& default_size) const override {
        if (pixel_size_mm_override.x > 0.0 && pixel_size_mm_override.y > 0.0) {
            return pixel_size_mm_override;
        }
        return default_size;
    }

    GazeVector3 get_camera_offset(const GazeVector3& default_offset) const override {
        if (camera_offset_override.x > -999.0) {
            return camera_offset_override;
        }
        return default_offset;
    }

    double get_camera_tilt(double default_tilt) const override {
        if (camera_tilt_override > -999.0) {
            return camera_tilt_override;
        }
        return default_tilt;
    }

    double get_bias_pitch() const override { return bias_pitch; }
    double get_bias_yaw() const override { return bias_yaw; }
    double get_scale_yaw() const override { return scale_yaw; }
    double get_scale_pitch() const override { return scale_pitch; }
    double get_bias_pixel_x() const override { return bias_pixel_x; }
    double get_bias_pixel_y() const override { return bias_pixel_y; }

    // Overrides getters/setters
    GazeVector2 get_pixel_size_mm_override() const { return pixel_size_mm_override; }
    void set_pixel_size_mm_override(const GazeVector2& val) { pixel_size_mm_override = val; }

    GazeVector3 get_camera_offset_override() const { return camera_offset_override; }
    void set_camera_offset_override(const GazeVector3& val) { camera_offset_override = val; }

    double get_camera_tilt_override() const { return camera_tilt_override; }
    void set_camera_tilt_override(double val) { camera_tilt_override = val; }

    void set_bias_pitch(double val) { bias_pitch = val; }
    void set_bias_yaw(double val) { bias_yaw = val; }
    void set_scale_yaw(double val) { scale_yaw = val; }
    void set_scale_pitch(double val) { scale_pitch = val; }
    void set_bias_pixel_x(double val) { bias_pixel_x = val; }
    void set_bias_pixel_y(double val) { bias_pixel_y = val; }
};

} // namespace Gaze
