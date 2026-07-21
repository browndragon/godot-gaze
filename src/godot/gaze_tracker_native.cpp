#include "gaze_tracker.hpp"
#ifndef WEB_ENABLED
#include "../core/log.hpp"
#include "camera_sensor.hpp"
#include "face_estimator.hpp"
#include "eye_estimator.hpp"
#include "smoother.hpp"
#include "one_euro_smoother.hpp"
#include "../native/ort_yunet_pipeline.hpp"
#include "../native/ort_gaze_model.hpp"
#include "display_profile.hpp"
#include "../core/math_defs.hpp"
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void GazeTracker::platform_initialize() {
    // Check platform camera permission
    bool has_permission = false;
    OS *os = OS::get_singleton();
    if (os && os->has_feature("android")) {
        PackedStringArray granted_perms = os->get_granted_permissions();
        for (int i = 0; i < granted_perms.size(); ++i) {
            if (granted_perms[i] == "android.permission.CAMERA") {
                has_permission = true;
                break;
            }
        }
    } else {
        has_permission = true;
    }

    if (!has_permission) {
        set_lifecycle_state(LIFECYCLE_PERM_REQ);
        trigger_permission_request();
        return;
    }

    set_lifecycle_state(LIFECYCLE_INITIALIZING);
    complete_initialization();
}

void GazeTracker::platform_terminate() {
}

void GazeTracker::platform_process(double delta) {
    // Handled asynchronously in GazeServer's background thread
}

void GazeTracker::platform_on_permission_result(bool granted) {
    OS *os = OS::get_singleton();
    if (os && os->has_feature("android")) {
        PackedStringArray granted_perms = os->get_granted_permissions();
        bool has_camera = false;
        for (int i = 0; i < granted_perms.size(); ++i) {
            if (granted_perms[i] == "android.permission.CAMERA") {
                has_camera = true;
                break;
            }
        }
        if (!has_camera) {
            set_lifecycle_state(LIFECYCLE_ERROR);
        }
    }
}

void GazeTracker::platform_trigger_permission_request() {
    OS *os = OS::get_singleton();
    if (os) {
        if (os->has_feature("android")) {
            os->request_permission("android.permission.CAMERA");
        }
    }
}

PlatformGeometry GazeTracker::platform_get_geometry() const {
    PlatformGeometry geom;
    DisplayServer* ds = DisplayServer::get_singleton();
    if (ds) {
        int screen_id = ds->window_get_current_screen();
        double scale = ds->screen_get_scale(screen_id);

        OS* os = OS::get_singleton();
        Vector2i window_pos_ppix = ds->window_get_position();
        bool is_standalone = os ? os->has_feature("standalone") : true;
        DisplayServer::WindowMode mode = ds->window_get_mode();
        bool is_fullscreen = (mode == DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN || 
                              mode == DisplayServer::WINDOW_MODE_FULLSCREEN);

        // Fallback: If running windowed in the editor/test runner (non-standalone) 
        // and the position is masked as (0, 0), assume the window is centered on the screen.
        if (!is_fullscreen && window_pos_ppix == Vector2i(0, 0) && !is_standalone) {
            Vector2i screen_size_ppix = ds->screen_get_size(screen_id);
            Vector2i window_size_ppix = ds->window_get_size(); // Physical pixels in Godot 4
            window_pos_ppix = (screen_size_ppix - window_size_ppix) / 2;
        }

        geom.window_position_px = Vector2(window_pos_ppix.x, window_pos_ppix.y);
    }

    if (window_position_override.x >= 0.0 && window_position_override.y >= 0.0) {
        geom.window_position_px = window_position_override;
    }

    return geom;
}
} // namespace godot
#endif // WEB_ENABLED


