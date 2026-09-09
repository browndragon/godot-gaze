#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/rect2i.hpp>

namespace godot {

class GazeDisplayServer : public Object {
    GDCLASS(GazeDisplayServer, Object);

private:
    static GazeDisplayServer *singleton;

protected:
    static void _bind_methods();

public:
    GazeDisplayServer();
    virtual ~GazeDisplayServer();

    static GazeDisplayServer *get_singleton() { return singleton; }

    virtual Vector2 get_screen_size_mm(int p_screen = -1) const;
    virtual Vector2 get_screen_size_pixels(int p_screen = -1) const;
    virtual Vector2 get_pixel_pitch_mm(int p_screen = -1) const;
    virtual double get_screen_scale(int p_screen = -1) const;
    virtual Rect2i get_window_rect_pixels(int p_window = 0) const;
    virtual Vector3 get_default_camera_offset_mm(int p_screen = -1) const;
};

} // namespace godot
