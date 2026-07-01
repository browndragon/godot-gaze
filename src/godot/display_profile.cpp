#include "display_profile.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/display_server.hpp>
#ifdef WEB_ENABLED
#include <godot_cpp/classes/java_script_bridge.hpp>
#endif

namespace godot {

void DisplayProfile::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_logical_size_px"), &DisplayProfile::get_logical_size_px);
    ClassDB::bind_method(D_METHOD("set_logical_size_px", "size"), &DisplayProfile::set_logical_size_px);
    ClassDB::bind_method(D_METHOD("get_physical_size_mm"), &DisplayProfile::get_physical_size_mm);
    ClassDB::bind_method(D_METHOD("set_physical_size_mm", "size"), &DisplayProfile::set_physical_size_mm);
    ClassDB::bind_method(D_METHOD("get_dpi"), &DisplayProfile::get_dpi);

    ClassDB::bind_static_method("DisplayProfile", D_METHOD("estimate_from_os"), &DisplayProfile::estimate_from_os);

    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "logical_size_px"), "set_logical_size_px", "get_logical_size_px");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "physical_size_mm"), "set_physical_size_mm", "get_physical_size_mm");
}

Ref<DisplayProfile> DisplayProfile::estimate_from_os() {
    Ref<DisplayProfile> profile;
    profile.instantiate();

    Vector2i size_lpix = Vector2i(1920, 1080);
    Vector2 size_mm = Vector2(345.0, 194.0);

#ifdef WEB_ENABLED
    JavaScriptBridge *js = JavaScriptBridge::get_singleton();
    if (js) {
        double w_lpix = js->eval("window.screen.width").stringify().to_float();
        double h_lpix = js->eval("window.screen.height").stringify().to_float();
        if (w_lpix > 0 && h_lpix > 0) {
            size_lpix = Vector2i((int)w_lpix, (int)h_lpix);
            double dpi_lpix = 172.0 - 0.03 * w_lpix;
            if (dpi_lpix < 96.0) {
                dpi_lpix = 96.0;
            }
            size_mm = Vector2((w_lpix / dpi_lpix) * 25.4, (h_lpix / dpi_lpix) * 25.4);
        }
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

} // namespace godot
