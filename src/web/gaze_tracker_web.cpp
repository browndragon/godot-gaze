#include "gaze_tracker.hpp"
#include "log.hpp"
#include "math_defs.hpp"
#include "camera_sensor.hpp"
#include "face_estimator.hpp"
#include "eye_estimator.hpp"
#include "smoother.hpp"
#include "one_euro_smoother.hpp"

#ifdef WEB_ENABLED
#include "web_binding_state.hpp"
#include "godot_files.hpp"
#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot
{

    void GazeTracker::platform_initialize()
    {
        if (!opaque)
        {
            Ref<WebBindingState> state = memnew(WebBindingState);
            state->reference(); // Increment reference count
            opaque = state.ptr();
            state->setup_callbacks(this);
        }

        set_lifecycle_state(LIFECYCLE_INITIALIZING);
        complete_initialization();
    }

    void GazeTracker::platform_terminate()
    {
        if (opaque)
        {
            WebBindingState *state = static_cast<WebBindingState *>(opaque);
            state->cleanup();
            state->unreference(); // Safely decrement reference count and free
            opaque = nullptr;
        }
    }

    void GazeTracker::platform_process(double delta)
    {
        // Web handles updates asynchronously via callback, so no CPU polling is needed here
    }

    void GazeTracker::platform_on_permission_result(bool granted)
    {
        // MediaDevices.getUserMedia handles browser permission prompts asynchronously, so no-op here
    }

    void GazeTracker::platform_trigger_permission_request()
    {
        // Handled browser-side via standard getUserMedia prompt when starting tracking loop
    }

    PlatformGeometry GazeTracker::platform_get_geometry() const
    {
        PlatformGeometry geom;
        geom.window_position_px = web_canvas_pos;

        if (window_position_override.x >= 0.0 && window_position_override.y >= 0.0)
        {
            geom.window_position_px = window_position_override;
        }

        if (log_this_frame)
        {
            UtilityFunctions::print("[GazeTracker Web Debug] platform_get_geometry return: window_pos_lpix: ", geom.window_position_px);
        }

        return geom;
    }

    void GazeTracker::on_sidecar_ready(const Array& args)
    {
        if (args.size() == 0) return;
        Ref<JavaScriptObject> sidecar = args[0];
        if (sidecar.is_null() || !sidecar.is_valid()) return;

        String resolved_yunet = face_estimator ? face_estimator->get_yunet_model_prefix() : "";
        resolved_yunet = resolve_model_path(resolved_yunet);
        if (resolved_yunet.is_empty()) {
            resolved_yunet = resolve_model_path("face_detection_yunet_2023mar");
        }
        Ref<FileAccess> f_yunet = FileAccess::open(resolved_yunet, FileAccess::READ);
        PackedByteArray yunet_bytes;
        if (f_yunet.is_valid()) {
            yunet_bytes = f_yunet->get_buffer(f_yunet->get_length());
        }

        String resolved_gaze = eye_estimator ? eye_estimator->get_gaze_model_prefix() : "";
        resolved_gaze = resolve_model_path(resolved_gaze);
        if (resolved_gaze.is_empty()) {
            resolved_gaze = resolve_model_path("gaze-estimation-adas-0002");
        }
        Ref<FileAccess> f_gaze = FileAccess::open(resolved_gaze, FileAccess::READ);
        PackedByteArray gaze_bytes;
        if (f_gaze.is_valid()) {
            gaze_bytes = f_gaze->get_buffer(f_gaze->get_length());
        }

        String hex_yunet = yunet_bytes.hex_encode();
        String hex_gaze = gaze_bytes.hex_encode();

        double focal = 1000.0;
        if (camera_sensor) {
            double f_val = camera_sensor->get_focal_length();
            if (f_val > 0.0) {
                focal = f_val;
            } else {
                double fov = camera_sensor->get_camera_fov();
                int cam_w = 640;
                Ref<Image> img = camera_sensor->get_last_frame();
                if (img.is_valid() && img->get_width() > 0) {
                    cam_w = img->get_width();
                }
                focal = Gaze::get_focal_length_px(cam_w, fov);
            }
        }
        sidecar->call("setModels", hex_yunet, hex_gaze, focal);
    }

} // namespace godot
#endif
