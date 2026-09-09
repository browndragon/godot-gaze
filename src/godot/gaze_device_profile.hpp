/**
 * @file gaze_device_profile.hpp
 * @brief Serializable Godot Resource holding physical display geometry, camera offset, roll, and intrinsics
 */
#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/classes/ref.hpp>

namespace godot {

class GazeDeviceProfile : public Resource {
    GDCLASS(GazeDeviceProfile, Resource);

private:
    Vector2 pixel_pitch_mm = Vector2(0.25, 0.25);
    Vector2i logical_size_px = Vector2i(1920, 1080);
    Vector3 camera_offset_mm = Vector3(0.0, 107.5, 0.0);
    double camera_roll_deg = 0.0;
    double camera_hfov_deg = 65.0;

protected:
    static void _bind_methods();

public:
    GazeDeviceProfile() = default;
    virtual ~GazeDeviceProfile() = default;

    Vector2 get_pixel_pitch_mm() const { return pixel_pitch_mm; }
    void set_pixel_pitch_mm(Vector2 p_pitch);

    Vector2i get_logical_size_px() const { return logical_size_px; }
    void set_logical_size_px(Vector2i p_size);

    Vector3 get_camera_offset_mm() const { return camera_offset_mm; }
    void set_camera_offset_mm(Vector3 p_offset);

    double get_camera_roll_deg() const { return camera_roll_deg; }
    void set_camera_roll_deg(double p_roll);

    double get_camera_hfov_deg() const { return camera_hfov_deg; }
    void set_camera_hfov_deg(double p_hfov);

    Vector2 get_physical_size_mm() const;
    void set_physical_size_mm(Vector2 p_size);

    Vector2 get_dpi() const;
    double get_focal_length_px(double frame_width_px) const;

    void calibrate_from_card_width(double card_width_lpix, double card_width_mm = 85.603);

    static Ref<GazeDeviceProfile> create_system_guess();

    static double get_focal_length_under_scaling_static(double f_original, double original_dim, double new_dim);
    static double get_card_width_px_static(double fov_degrees, double card_distance_mm, double frame_width, double card_width_mm = 85.603);
    static double diagonal_to_horizontal_fov_static(double diagonal_fov_degrees, double width, double height);
};

} // namespace godot
