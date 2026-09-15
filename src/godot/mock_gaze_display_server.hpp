#pragma once

#include "gaze_display_server.hpp"
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace godot {

class MockGazeDisplayServer : public GazeDisplayServer {
    GDCLASS(MockGazeDisplayServer, GazeDisplayServer);

private:
    Vector2 mock_screen_size_mm{301.59, 188.49};
    Vector2 mock_screen_size_pixels{1512.0, 982.0};
    Rect2i mock_window_rect{Vector2i(100, 100), Vector2i(1312, 782)};
    double mock_screen_scale = 2.0;
    Vector2 mock_mouse_position{0.0, 0.0};
    int64_t mock_mouse_button_state{0};
    TypedArray<InputEvent> emitted_mouse_events;

protected:
    static void _bind_methods();

public:
    MockGazeDisplayServer() = default;
    virtual ~MockGazeDisplayServer() = default;

    virtual Vector2 get_screen_size_mm(int p_screen = -1) const override;
    virtual Vector2 get_screen_size_pixels(int p_screen = -1) const override;
    virtual Vector2 get_pixel_pitch_mm(int p_screen = -1) const override;
    virtual double get_screen_scale(int p_screen = -1) const override;
    virtual Rect2i get_window_rect_pixels(int p_window = 0) const override;
    virtual Vector2 mouse_get_position() const override;
    virtual int64_t mouse_get_button_state() const override;

    virtual void parse_mouse_motion(const Vector2 &p_position, const Vector2 &p_relative, const Vector2 &p_velocity) override;
    virtual void parse_mouse_button(int64_t p_button, bool p_pressed, const Vector2 &p_position) override;

    void set_screen_size_mm(const Vector2 &p_size_mm);
    void set_screen_size_pixels(const Vector2 &p_size_pixels);
    void set_window_rect_pixels(const Rect2i &p_rect);
    void set_screen_scale(double p_scale);
    void set_mouse_position(const Vector2 &p_pos);
    void set_mouse_button_state(int64_t p_mask);

    TypedArray<InputEvent> get_emitted_mouse_events() const;
    void clear_emitted_mouse_events();
};

} // namespace godot
