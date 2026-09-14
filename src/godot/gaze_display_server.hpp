#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/rect2i.hpp>

namespace godot {

/**
 * @class GazeDisplayServer
 * @brief Singleton query server providing physical display geometry, scale factors, and hardware camera offsets.
 */
class GazeDisplayServer : public Object {
    GDCLASS(GazeDisplayServer, Object);

private:
    static GazeDisplayServer *singleton;

protected:
    static void _bind_methods();

public:
    GazeDisplayServer();
    virtual ~GazeDisplayServer();

    /**
     * @brief Get the GazeDisplayServer global singleton instance.
     * @return Pointer to the singleton instance.
     */
    static GazeDisplayServer *get_singleton();

    /**
     * @brief Retrieve physical screen dimensions in millimeters.
     * @param p_screen Screen index (-1 for default/current screen).
     * @return Screen size Vector2(width_mm, height_mm).
     */
    virtual Vector2 get_screen_size_mm(int p_screen = -1) const;

    /**
     * @brief Retrieve screen logical pixel dimensions.
     * @param p_screen Screen index (-1 for default/current screen).
     * @return Screen resolution Vector2(width_px, height_px).
     */
    virtual Vector2 get_screen_size_pixels(int p_screen = -1) const;

    /**
     * @brief Retrieve physical pixel pitch in millimeters per pixel.
     * @param p_screen Screen index (-1 for default/current screen).
     * @return Vector2(pitch_x_mm, pitch_y_mm).
     */
    virtual Vector2 get_pixel_pitch_mm(int p_screen = -1) const;

    /**
     * @brief Retrieve screen HiDPI scale factor.
     * @param p_screen Screen index (-1 for default/current screen).
     * @return Scale factor double.
     */
    virtual double get_screen_scale(int p_screen = -1) const;

    /**
     * @brief Retrieve application window bounding rectangle in logical pixels.
     * @param p_window Window index.
     * @return Window Rect2i(x, y, width, height).
     */
    virtual Rect2i get_window_rect_pixels(int p_window = 0) const;

    /**
     * @brief Retrieve the default hardware camera placement offset for the display.
     * @param p_screen Screen index (-1 for default/current screen).
     * @return Camera offset Vector3 in millimeters.
     */
    virtual Vector3 get_default_camera_offset_mm(int p_screen = -1) const;

    /**
     * @brief Retrieve the current mouse cursor position in logical screen coordinates.
     * @return Screen mouse position Vector2(x, y).
     */
    virtual Vector2 get_input_mouse_position() const;

    /**
     * @brief Retrieve the bitmask of currently pressed mouse buttons.
     * @return Bitmask of pressed mouse buttons.
     */
    virtual int64_t get_input_mouse_button_mask() const;
};

} // namespace godot
