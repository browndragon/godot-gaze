#include "mock_gaze_display_server.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>

namespace godot {

void MockGazeDisplayServer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_screen_size_mm", "size_mm"), &MockGazeDisplayServer::set_screen_size_mm);
    ClassDB::bind_method(D_METHOD("set_screen_size_pixels", "size_pixels"), &MockGazeDisplayServer::set_screen_size_pixels);
    ClassDB::bind_method(D_METHOD("set_window_rect_pixels", "rect"), &MockGazeDisplayServer::set_window_rect_pixels);
    ClassDB::bind_method(D_METHOD("set_screen_scale", "scale"), &MockGazeDisplayServer::set_screen_scale);
    ClassDB::bind_method(D_METHOD("set_mouse_position", "position"), &MockGazeDisplayServer::set_mouse_position);
    ClassDB::bind_method(D_METHOD("set_mouse_button_state", "mask"), &MockGazeDisplayServer::set_mouse_button_state);
    ClassDB::bind_method(D_METHOD("get_emitted_mouse_events"), &MockGazeDisplayServer::get_emitted_mouse_events);
    ClassDB::bind_method(D_METHOD("clear_emitted_mouse_events"), &MockGazeDisplayServer::clear_emitted_mouse_events);
}

Vector2 MockGazeDisplayServer::get_screen_size_mm(int p_screen) const {
    return mock_screen_size_mm;
}

Vector2 MockGazeDisplayServer::get_screen_size_pixels(int p_screen) const {
    return mock_screen_size_pixels;
}

Vector2 MockGazeDisplayServer::get_pixel_pitch_mm(int p_screen) const {
    double px = (mock_screen_size_pixels.x > 0.0) ? (mock_screen_size_mm.x / mock_screen_size_pixels.x) : 0.20;
    double py = (mock_screen_size_pixels.y > 0.0) ? (mock_screen_size_mm.y / mock_screen_size_pixels.y) : 0.20;
    return Vector2(px, py);
}

double MockGazeDisplayServer::get_screen_scale(int p_screen) const {
    return mock_screen_scale;
}

Rect2i MockGazeDisplayServer::get_window_rect_pixels(int p_window) const {
    return mock_window_rect;
}

Vector2 MockGazeDisplayServer::mouse_get_position() const {
    return mock_mouse_position;
}

int64_t MockGazeDisplayServer::mouse_get_button_state() const {
    return mock_mouse_button_state;
}

void MockGazeDisplayServer::parse_mouse_motion(const Vector2 &p_position, const Vector2 &p_relative, const Vector2 &p_velocity) {
    GazeDisplayServer::parse_mouse_motion(p_position, p_relative, p_velocity);
    Ref<InputEventMouseMotion> mm;
    mm.instantiate();
    mm->set_device(-1);
    mm->set_meta("synthetic_gaze", true);
    mm->set_position(p_position);
    mm->set_global_position(p_position);
    mm->set_relative(p_relative);
    mm->set_velocity(p_velocity);
    emitted_mouse_events.push_back(mm);
}

void MockGazeDisplayServer::parse_mouse_button(int64_t p_button, bool p_pressed, const Vector2 &p_position) {
    GazeDisplayServer::parse_mouse_button(p_button, p_pressed, p_position);
    Ref<InputEventMouseButton> mb;
    mb.instantiate();
    mb->set_device(-1);
    mb->set_meta("synthetic_gaze", true);
    mb->set_button_index((MouseButton)p_button);
    mb->set_pressed(p_pressed);
    mb->set_position(p_position);
    mb->set_global_position(p_position);
    emitted_mouse_events.push_back(mb);
}

void MockGazeDisplayServer::set_screen_size_mm(const Vector2 &p_size_mm) {
    mock_screen_size_mm = p_size_mm;
}

void MockGazeDisplayServer::set_screen_size_pixels(const Vector2 &p_size_pixels) {
    mock_screen_size_pixels = p_size_pixels;
}

void MockGazeDisplayServer::set_window_rect_pixels(const Rect2i &p_rect) {
    mock_window_rect = p_rect;
}

void MockGazeDisplayServer::set_screen_scale(double p_scale) {
    mock_screen_scale = p_scale;
}

void MockGazeDisplayServer::set_mouse_position(const Vector2 &p_pos) {
    mock_mouse_position = p_pos;
}

void MockGazeDisplayServer::set_mouse_button_state(int64_t p_mask) {
    mock_mouse_button_state = p_mask;
}

TypedArray<InputEvent> MockGazeDisplayServer::get_emitted_mouse_events() const {
    return emitted_mouse_events;
}

void MockGazeDisplayServer::clear_emitted_mouse_events() {
    emitted_mouse_events.clear();
}

} // namespace godot
