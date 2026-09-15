#include "gaze_display_server.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "../core/gaze_display_types.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
#include "../ios/gaze_display_ios.hpp"
#define NATIVE_GET_METRICS Gaze::gaze_ios_get_display_metrics
#define NATIVE_GET_WINDOW_RECT Gaze::gaze_ios_get_window_rect
#else
#include "../macos/gaze_display_macos.hpp"
#define NATIVE_GET_METRICS Gaze::gaze_macos_get_display_metrics
#define NATIVE_GET_WINDOW_RECT Gaze::gaze_macos_get_window_rect
#endif
#elif defined(_WIN32) || defined(WINDOWS_ENABLED)
#include "../windows/gaze_display_windows.hpp"
#define NATIVE_GET_METRICS Gaze::gaze_windows_get_display_metrics
#define NATIVE_GET_WINDOW_RECT Gaze::gaze_windows_get_window_rect
#elif defined(__ANDROID__) || defined(ANDROID_ENABLED)
#include "../android/gaze_display_android.hpp"
#define NATIVE_GET_METRICS Gaze::gaze_android_get_display_metrics
#define NATIVE_GET_WINDOW_RECT Gaze::gaze_android_get_window_rect
#elif defined(WEB_ENABLED)
#include "../web/gaze_display_server_web.hpp"
#define NATIVE_GET_METRICS Gaze::gaze_web_get_display_metrics
#define NATIVE_GET_WINDOW_RECT Gaze::gaze_web_get_window_rect
#else
#include "../native/gaze_display_fallback.hpp"
#define NATIVE_GET_METRICS Gaze::gaze_fallback_get_display_metrics
#define NATIVE_GET_WINDOW_RECT Gaze::gaze_fallback_get_window_rect
#endif

namespace godot {

GazeDisplayServer *GazeDisplayServer::singleton = nullptr;

void GazeDisplayServer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_screen_size_mm", "screen"), &GazeDisplayServer::get_screen_size_mm, DEFVAL(-1));
    ClassDB::bind_method(D_METHOD("get_screen_size_pixels", "screen"), &GazeDisplayServer::get_screen_size_pixels, DEFVAL(-1));
    ClassDB::bind_method(D_METHOD("get_pixel_pitch_mm", "screen"), &GazeDisplayServer::get_pixel_pitch_mm, DEFVAL(-1));
    ClassDB::bind_method(D_METHOD("get_screen_scale", "screen"), &GazeDisplayServer::get_screen_scale, DEFVAL(-1));
    ClassDB::bind_method(D_METHOD("get_window_rect_pixels", "window"), &GazeDisplayServer::get_window_rect_pixels, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("get_default_camera_offset_mm", "screen"), &GazeDisplayServer::get_default_camera_offset_mm, DEFVAL(-1));
    ClassDB::bind_method(D_METHOD("mouse_get_position"), &GazeDisplayServer::mouse_get_position);
    ClassDB::bind_method(D_METHOD("mouse_get_button_state"), &GazeDisplayServer::mouse_get_button_state);
    ClassDB::bind_method(D_METHOD("parse_mouse_motion", "position", "relative", "velocity"), &GazeDisplayServer::parse_mouse_motion);
    ClassDB::bind_method(D_METHOD("parse_mouse_button", "button", "pressed", "position"), &GazeDisplayServer::parse_mouse_button);
}

GazeDisplayServer *GazeDisplayServer::get_singleton() {
    Engine *engine = Engine::get_singleton();
    if (engine && engine->has_singleton("GazeDisplayServer")) {
        return Object::cast_to<GazeDisplayServer>(engine->get_singleton("GazeDisplayServer"));
    }
    return singleton;
}

GazeDisplayServer::GazeDisplayServer() {
    if (singleton == nullptr) {
        singleton = this;
    }
}

GazeDisplayServer::~GazeDisplayServer() {
    if (singleton == this) {
        singleton = nullptr;
    }
}

Vector2 GazeDisplayServer::get_screen_size_mm(int p_screen) const {
    DisplayServer *ds = nullptr;
    if (Engine::get_singleton()->has_singleton("DisplayServer")) {
        ds = DisplayServer::get_singleton();
    }
    int screen_idx = p_screen;
    if (screen_idx < 0 && ds) {
        screen_idx = ds->window_get_current_screen();
    }
    if (screen_idx < 0) screen_idx = 0;

    Gaze::GazeDisplayMetrics metrics = NATIVE_GET_METRICS(screen_idx);
    return Vector2(metrics.width_mm, metrics.height_mm);
}

Vector2 GazeDisplayServer::get_screen_size_pixels(int p_screen) const {
    int screen_idx = p_screen;
    if (screen_idx < 0 && Engine::get_singleton()->has_singleton("DisplayServer")) {
        DisplayServer *ds = DisplayServer::get_singleton();
        if (ds) {
            screen_idx = ds->window_get_current_screen();
        }
    }
    if (screen_idx < 0) screen_idx = 0;

    Gaze::GazeDisplayMetrics metrics = NATIVE_GET_METRICS(screen_idx);
    if (metrics.pixel_width > 0 && metrics.pixel_height > 0) {
        return Vector2(metrics.pixel_width, metrics.pixel_height);
    }

    if (Engine::get_singleton()->has_singleton("DisplayServer")) {
        DisplayServer *ds = DisplayServer::get_singleton();
        if (ds) {
            Vector2i sz = ds->screen_get_size(screen_idx);
            double scale = ds->screen_get_scale(screen_idx);
            if (scale > 0.0) {
                return Vector2(sz.x / scale, sz.y / scale);
            }
            return Vector2(sz.x, sz.y);
        }
    }
    return Vector2(1920, 1080);
}

Vector2 GazeDisplayServer::get_pixel_pitch_mm(int p_screen) const {
    Vector2 mm = get_screen_size_mm(p_screen);
    Vector2 px = get_screen_size_pixels(p_screen);
    double pitch_x = (px.x > 0.0) ? (mm.x / px.x) : 0.20;
    double pitch_y = (px.y > 0.0) ? (mm.y / px.y) : 0.20;
    return Vector2(pitch_x, pitch_y);
}

double GazeDisplayServer::get_screen_scale(int p_screen) const {
    int screen_idx = p_screen;
    if (screen_idx < 0 && Engine::get_singleton()->has_singleton("DisplayServer")) {
        DisplayServer *ds = DisplayServer::get_singleton();
        if (ds) {
            screen_idx = ds->window_get_current_screen();
        }
    }
    if (screen_idx < 0) screen_idx = 0;

    Gaze::GazeDisplayMetrics metrics = NATIVE_GET_METRICS(screen_idx);
    if (metrics.scale_factor > 0.0) {
        return metrics.scale_factor;
    }

    if (Engine::get_singleton()->has_singleton("DisplayServer")) {
        DisplayServer *ds = DisplayServer::get_singleton();
        if (ds) {
            return ds->screen_get_scale(screen_idx);
        }
    }
    return 1.0;
}


Rect2i GazeDisplayServer::get_window_rect_pixels(int p_window) const {
    if (Engine::get_singleton()->has_singleton("DisplayServer")) {
        DisplayServer *ds = DisplayServer::get_singleton();
        if (ds && ds->get_name() == "headless") {
            Vector2 scr_sz = get_screen_size_pixels(0);
            return Rect2i(0, 0, (int)scr_sz.x, (int)scr_sz.y);
        }
    }
    Gaze::GazeWindowRect rect = NATIVE_GET_WINDOW_RECT(p_window);
    if (rect.width > 0 && rect.height > 0) {
        return Rect2i(rect.x, rect.y, rect.width, rect.height);
    }
    if (Engine::get_singleton()->has_singleton("DisplayServer")) {
        DisplayServer *ds = DisplayServer::get_singleton();
        if (ds) {
            return Rect2i(ds->window_get_position(p_window), ds->window_get_size(p_window));
        }
    }
    Vector2 scr_sz = get_screen_size_pixels(0);
    return Rect2i(0, 0, (int)scr_sz.x, (int)scr_sz.y);
}



Vector3 GazeDisplayServer::get_default_camera_offset_mm(int p_screen) const {
    return Vector3(0.0, 0.0, 0.0);
}

Vector2 GazeDisplayServer::mouse_get_position() const {
    DisplayServer *ds = nullptr;
    if (Engine::get_singleton()->has_singleton("DisplayServer")) {
        ds = DisplayServer::get_singleton();
    }
    if (ds && ds->get_name() != "headless") {
        return Vector2(ds->mouse_get_position());
    }
    return Vector2(0, 0);
}

int64_t GazeDisplayServer::mouse_get_button_state() const {
    DisplayServer *ds = nullptr;
    if (Engine::get_singleton()->has_singleton("DisplayServer")) {
        ds = DisplayServer::get_singleton();
    }
    if (ds && ds->get_name() != "headless") {
        return (int64_t)ds->mouse_get_button_state();
    }
    return 0;
}

void GazeDisplayServer::parse_mouse_motion(const Vector2 &p_position, const Vector2 &p_relative, const Vector2 &p_velocity) {
    Input *input = Input::get_singleton();
    if (!input) return;
    Ref<InputEventMouseMotion> mm;
    mm.instantiate();
    mm->set_device(-1);
    mm->set_meta("synthetic_gaze", true);
    mm->set_position(p_position);
    mm->set_global_position(p_position);
    mm->set_relative(p_relative);
    mm->set_velocity(p_velocity);
    input->parse_input_event(mm);
}

void GazeDisplayServer::parse_mouse_button(int64_t p_button, bool p_pressed, const Vector2 &p_position) {
    Input *input = Input::get_singleton();
    if (!input) return;
    Ref<InputEventMouseButton> mb;
    mb.instantiate();
    mb->set_device(-1);
    mb->set_meta("synthetic_gaze", true);
    mb->set_button_index((MouseButton)p_button);
    mb->set_pressed(p_pressed);
    mb->set_position(p_position);
    mb->set_global_position(p_position);
    input->parse_input_event(mb);
}

} // namespace godot
