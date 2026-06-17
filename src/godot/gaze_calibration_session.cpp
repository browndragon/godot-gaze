#include "gaze_calibration_session.hpp"
#include "gaze_tracker.hpp"
#include "gaze_calibration_resource.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void GazeCalibrationSession::_bind_methods() {
    ClassDB::bind_method(D_METHOD("add_sample", "target_pixel", "gaze_origin", "gaze_direction"), &GazeCalibrationSession::add_sample);
    ClassDB::bind_method(D_METHOD("clear"), &GazeCalibrationSession::clear);
    ClassDB::bind_method(D_METHOD("get_sample_count"), &GazeCalibrationSession::get_sample_count);
    ClassDB::bind_method(D_METHOD("calculate_calibration", "tracker", "use_3d"), &GazeCalibrationSession::calculate_calibration);

    ClassDB::bind_method(D_METHOD("set_target_pixels", "arr"), &GazeCalibrationSession::set_target_pixels);
    ClassDB::bind_method(D_METHOD("get_target_pixels"), &GazeCalibrationSession::get_target_pixels);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "target_pixels"), "set_target_pixels", "get_target_pixels");

    ClassDB::bind_method(D_METHOD("set_gaze_origins", "arr"), &GazeCalibrationSession::set_gaze_origins);
    ClassDB::bind_method(D_METHOD("get_gaze_origins"), &GazeCalibrationSession::get_gaze_origins);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "gaze_origins"), "set_gaze_origins", "get_gaze_origins");

    ClassDB::bind_method(D_METHOD("set_gaze_directions", "arr"), &GazeCalibrationSession::set_gaze_directions);
    ClassDB::bind_method(D_METHOD("get_gaze_directions"), &GazeCalibrationSession::get_gaze_directions);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "gaze_directions"), "set_gaze_directions", "get_gaze_directions");
}

void GazeCalibrationSession::add_sample(Vector2 target_pixel, Vector3 gaze_origin, Vector3 gaze_direction) {
    target_pixels.push_back(target_pixel);
    gaze_origins.push_back(gaze_origin);
    gaze_directions.push_back(gaze_direction);
}

void GazeCalibrationSession::clear() {
    target_pixels.clear();
    gaze_origins.clear();
    gaze_directions.clear();
}

int GazeCalibrationSession::get_sample_count() const {
    return target_pixels.size();
}

Ref<GazeCalibrationResource> GazeCalibrationSession::calculate_calibration(GazeTracker *tracker, bool use_3d) {
    Ref<GazeCalibrationResource> res;
    res.instantiate();

    if (!tracker) {
        return res;
    }

    int count = get_sample_count();
    if (count == 0) {
        return res;
    }

    double sum_pitch = 0.0;
    double sum_yaw = 0.0;
    double sum_px = 0.0;
    double sum_py = 0.0;
    int valid_samples = 0;

    const Gaze::ProjectionEngine& engine = tracker->get_projection_engine();

    for (int i = 0; i < count; ++i) {
        if (i >= gaze_origins.size() || i >= gaze_directions.size()) {
            break;
        }

        Vector2 target_val = target_pixels[i];
        Vector3 gaze_orig_val = gaze_origins[i];
        Vector3 gaze_dir_val = gaze_directions[i];

        Gaze::GazeVector2 target_pixel_gaze(target_val.x, target_val.y);

        Gaze::GazeCalibration calib;
        bool success = false;
        if (use_3d) {
            success = engine.calibrate_3d_bias(
                Gaze::GazeVector3(gaze_orig_val.x, gaze_orig_val.y, gaze_orig_val.z),
                Gaze::GazeVector3(gaze_dir_val.x, gaze_dir_val.y, gaze_dir_val.z),
                target_pixel_gaze,
                calib
            );
        } else {
            success = engine.calibrate_2d_bias(
                Gaze::GazeVector3(gaze_orig_val.x, gaze_orig_val.y, gaze_orig_val.z),
                Gaze::GazeVector3(gaze_dir_val.x, gaze_dir_val.y, gaze_dir_val.z),
                target_pixel_gaze,
                calib
            );
        }

        if (success) {
            sum_pitch += calib.bias_pitch;
            sum_yaw += calib.bias_yaw;
            sum_px += calib.bias_pixel_x;
            sum_py += calib.bias_pixel_y;
            valid_samples += 1;
        }
    }

    if (valid_samples > 0) {
        Gaze::GazeCalibration final_calib;
        if (use_3d) {
            final_calib.bias_pitch = sum_pitch / valid_samples;
            final_calib.bias_yaw = sum_yaw / valid_samples;
            final_calib.bias_pixel_x = 0.0;
            final_calib.bias_pixel_y = 0.0;
        } else {
            final_calib.bias_pitch = 0.0;
            final_calib.bias_yaw = 0.0;
            final_calib.bias_pixel_x = sum_px / valid_samples;
            final_calib.bias_pixel_y = sum_py / valid_samples;
        }
        res->set_calibration(final_calib);
    }

    return res;
}

} // namespace godot
