#include "display_profile.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/display_server.hpp>
#ifdef WEB_ENABLED
#include <emscripten.h>
#include <cstdlib>

static double em_eval_float(const char* script) {
    const char* res = emscripten_run_script_string(script);
    if (res) {
        return std::strtod(res, nullptr);
    }
    return 0.0;
}
#endif

namespace godot {

void DisplayProfile::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_logical_size_px"), &DisplayProfile::get_logical_size_px);
    ClassDB::bind_method(D_METHOD("set_logical_size_px", "size"), &DisplayProfile::set_logical_size_px);
    ClassDB::bind_method(D_METHOD("get_physical_size_mm"), &DisplayProfile::get_physical_size_mm);
    ClassDB::bind_method(D_METHOD("set_physical_size_mm", "size"), &DisplayProfile::set_physical_size_mm);
    ClassDB::bind_method(D_METHOD("get_dpi"), &DisplayProfile::get_dpi);

    ClassDB::bind_static_method("DisplayProfile", D_METHOD("estimate_from_os"), &DisplayProfile::estimate_from_os);
    ClassDB::bind_static_method("DisplayProfile", D_METHOD("get_screen_scale"), &DisplayProfile::get_screen_scale);
    ClassDB::bind_static_method("DisplayProfile", D_METHOD("get_screen_size_logical"), &DisplayProfile::get_screen_size_logical);
    ClassDB::bind_static_method("DisplayProfile", D_METHOD("get_window_size_logical"), &DisplayProfile::get_window_size_logical);

    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "logical_size_px"), "set_logical_size_px", "get_logical_size_px");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "physical_size_mm"), "set_physical_size_mm", "get_physical_size_mm");
}

Ref<DisplayProfile> DisplayProfile::estimate_from_os() {
    Ref<DisplayProfile> profile;
    profile.instantiate();

    Vector2i size_lpix = Vector2i(1920, 1080);
    Vector2 size_mm = Vector2(345.0, 194.0);

#ifdef WEB_ENABLED
    double w_lpix = em_eval_float("window.screen.width");
    double h_lpix = em_eval_float("window.screen.height");
    if (w_lpix > 0 && h_lpix > 0) {
        size_lpix = Vector2i((int)w_lpix, (int)h_lpix);
        double dpi_lpix = 172.0 - 0.03 * w_lpix;
        if (dpi_lpix < 96.0) {
            dpi_lpix = 96.0;
        }
        size_mm = Vector2((w_lpix / dpi_lpix) * 25.4, (h_lpix / dpi_lpix) * 25.4);
    }
#else
    DisplayServer* ds = DisplayServer::get_singleton();
    if (ds) {
        int screen_id = ds->window_get_current_screen();
        double scale = ds->screen_get_scale(screen_id);
        Vector2i size_ppix = ds->screen_get_size(screen_id);
        if (size_ppix.x > 0 && size_ppix.y > 0) {
            size_lpix = Vector2i((int)(size_ppix.x / scale), (int)(size_ppix.y / scale));
            double dpi = ds->screen_get_dpi(screen_id);
            if (dpi < 120.0 || dpi <= 0.0) {
                double w_lpix = size_lpix.x;
                double dpi_lpix = 172.0 - 0.03 * w_lpix;
                if (dpi_lpix < 96.0) {
                    dpi_lpix = 96.0;
                }
                dpi = dpi_lpix * scale;
            }
            if (dpi > 0.0) {
                size_mm = Vector2((size_ppix.x / dpi) * 25.4, (size_ppix.y / dpi) * 25.4);
            }
        }
    }
#endif

    profile->set_logical_size_px(size_lpix);
    profile->set_physical_size_mm(size_mm);
    return profile;
}

DisplayProfile::DisplayProfile() {}

DisplayProfile::~DisplayProfile() {}

void DisplayProfile::set_logical_size_px(Vector2i size) {
    logical_size_px = size;
}

Vector2i DisplayProfile::get_logical_size_px() const {
    return logical_size_px;
}

void DisplayProfile::set_physical_size_mm(Vector2 size) {
    physical_size_mm = size;
}

Vector2 DisplayProfile::get_physical_size_mm() const {
    return physical_size_mm;
}

Vector2 DisplayProfile::get_dpi() const {
    if (physical_size_mm.x <= 0.0 || physical_size_mm.y <= 0.0) {
        return Vector2(96.0, 96.0);
    }
    return Vector2(
        ((double)logical_size_px.x / physical_size_mm.x) * 25.4,
        ((double)logical_size_px.y / physical_size_mm.y) * 25.4
    );
}

Vector2 DisplayProfile::get_screen_scale() {
    double scale = 1.0;
#ifdef WEB_ENABLED
    double dpr = em_eval_float("window.devicePixelRatio");
    if (dpr > 0.0) {
        scale = dpr;
    }
#else
    DisplayServer *ds = DisplayServer::get_singleton();
    if (ds) {
        scale = ds->screen_get_scale(ds->window_get_current_screen());
    }
#endif
    return Vector2(scale, scale);
}

Vector2 DisplayProfile::get_screen_size_logical() {
#ifdef WEB_ENABLED
    double w_lpix = em_eval_float("window.screen.width");
    double h_lpix = em_eval_float("window.screen.height");
    if (w_lpix > 0 && h_lpix > 0) {
        return Vector2(w_lpix, h_lpix);
    }
#else
    DisplayServer *ds = DisplayServer::get_singleton();
    if (ds) {
        int screen_id = ds->window_get_current_screen();
        double scale = ds->screen_get_scale(screen_id);
        Vector2i size_ppix = ds->screen_get_size(screen_id);
        if (size_ppix.x > 0 && size_ppix.y > 0 && scale > 0.0) {
            return Vector2(size_ppix.x / scale, size_ppix.y / scale);
        }
    }
#endif
    return Vector2(1920, 1080);
}

Vector2 DisplayProfile::get_window_size_logical() {
#ifdef WEB_ENABLED
    double canvas_w = em_eval_float("(function() { var canvas = (typeof Module !== 'undefined' && Module.canvas) || document.getElementById('canvas') || document.querySelector('canvas'); return canvas ? canvas.getBoundingClientRect().width : window.innerWidth; })()");
    double canvas_h = em_eval_float("(function() { var canvas = (typeof Module !== 'undefined' && Module.canvas) || document.getElementById('canvas') || document.querySelector('canvas'); return canvas ? canvas.getBoundingClientRect().height : window.innerHeight; })()");
    
    double outer_w = em_eval_float("window.outerWidth");
    double outer_h = em_eval_float("window.outerHeight");
    double inner_w = em_eval_float("window.innerWidth");
    double inner_h = em_eval_float("window.innerHeight");
    
    if (canvas_w > 0 && canvas_h > 0 && outer_w > 0 && inner_w > 0 && outer_h > 0 && inner_h > 0) {
        double adj_w = outer_w * (canvas_w / inner_w);
        double adj_h = outer_h * (canvas_h / inner_h);
        return Vector2(adj_w, adj_h);
    }
    
    if (canvas_w > 0 && canvas_h > 0) {
        return Vector2(canvas_w, canvas_h);
    }
#else
    DisplayServer *ds = DisplayServer::get_singleton();
    if (ds) {
        int screen_id = ds->window_get_current_screen();
        double scale = ds->screen_get_scale(screen_id);
        Vector2i size_ppix = ds->window_get_size();
        if (size_ppix.x > 0 && size_ppix.y > 0 && scale > 0.0) {
            return Vector2(size_ppix.x / scale, size_ppix.y / scale);
        }
    }
#endif
    return Vector2(1280, 720);
}

} // namespace godot
