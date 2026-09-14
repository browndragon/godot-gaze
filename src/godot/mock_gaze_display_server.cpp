#include "mock_gaze_display_server.hpp"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void MockGazeDisplayServer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_screen_size_mm", "size_mm"), &MockGazeDisplayServer::set_screen_size_mm);
    ClassDB::bind_method(D_METHOD("set_screen_size_pixels", "size_pixels"), &MockGazeDisplayServer::set_screen_size_pixels);
    ClassDB::bind_method(D_METHOD("set_window_rect_pixels", "rect"), &MockGazeDisplayServer::set_window_rect_pixels);
    ClassDB::bind_method(D_METHOD("set_screen_scale", "scale"), &MockGazeDisplayServer::set_screen_scale);
    ClassDB::bind_method(D_METHOD("set_input_mouse_position", "position"), &MockGazeDisplayServer::set_input_mouse_position);
    ClassDB::bind_method(D_METHOD("set_input_mouse_button_mask", "mask"), &MockGazeDisplayServer::set_input_mouse_button_mask);
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

Vector2 MockGazeDisplayServer::get_input_mouse_position() const {
    return mock_mouse_position;
}

int64_t MockGazeDisplayServer::get_input_mouse_button_mask() const {
    return mock_mouse_button_mask;
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

void MockGazeDisplayServer::set_input_mouse_position(const Vector2 &p_pos) {
    mock_mouse_position = p_pos;
}

void MockGazeDisplayServer::set_input_mouse_button_mask(int64_t p_mask) {
    mock_mouse_button_mask = p_mask;
}

} // namespace godot
