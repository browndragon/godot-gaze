#include "gaze_calibration_session.hpp"
#include "gaze_tracker.hpp"
#include "gaze_calibration_resource.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void GazeCalibrationSession::_bind_methods() {
    ClassDB::bind_method(D_METHOD("add_sample", "target_pixel", "left_origin", "left_direction", "right_origin", "right_direction"), &GazeCalibrationSession::add_sample);
    ClassDB::bind_method(D_METHOD("clear"), &GazeCalibrationSession::clear);
    ClassDB::bind_method(D_METHOD("get_sample_count"), &GazeCalibrationSession::get_sample_count);
    ClassDB::bind_method(D_METHOD("calculate_calibration", "tracker", "use_3d"), &GazeCalibrationSession::calculate_calibration);

    ClassDB::bind_method(D_METHOD("set_target_pixels", "arr"), &GazeCalibrationSession::set_target_pixels);
    ClassDB::bind_method(D_METHOD("get_target_pixels"), &GazeCalibrationSession::get_target_pixels);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "target_pixels"), "set_target_pixels", "get_target_pixels");

    ClassDB::bind_method(D_METHOD("set_left_origins", "arr"), &GazeCalibrationSession::set_left_origins);
    ClassDB::bind_method(D_METHOD("get_left_origins"), &GazeCalibrationSession::get_left_origins);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "left_origins"), "set_left_origins", "get_left_origins");

    ClassDB::bind_method(D_METHOD("set_left_directions", "arr"), &GazeCalibrationSession::set_left_directions);
    ClassDB::bind_method(D_METHOD("get_left_directions"), &GazeCalibrationSession::get_left_directions);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "left_directions"), "set_left_directions", "get_left_directions");

    ClassDB::bind_method(D_METHOD("set_right_origins", "arr"), &GazeCalibrationSession::set_right_origins);
    ClassDB::bind_method(D_METHOD("get_right_origins"), &GazeCalibrationSession::get_right_origins);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "right_origins"), "set_right_origins", "get_right_origins");

    ClassDB::bind_method(D_METHOD("set_right_directions", "arr"), &GazeCalibrationSession::set_right_directions);
    ClassDB::bind_method(D_METHOD("get_right_directions"), &GazeCalibrationSession::get_right_directions);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "right_directions"), "set_right_directions", "get_right_directions");
}

void GazeCalibrationSession::add_sample(Vector2 target_pixel, Vector3 left_origin, Vector3 left_direction, Vector3 right_origin, Vector3 right_direction) {
    target_pixels.push_back(target_pixel);
    left_origins.push_back(left_origin);
    left_directions.push_back(left_direction);
    right_origins.push_back(right_origin);
    right_directions.push_back(right_direction);
}

void GazeCalibrationSession::clear() {
    target_pixels.clear();
    left_origins.clear();
    left_directions.clear();
    right_origins.clear();
    right_directions.clear();
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
        if (i >= left_origins.size() || i >= left_directions.size() ||
            i >= right_origins.size() || i >= right_directions.size()) {
            break;
        }

        Vector2 target_val = target_pixels[i];
        Vector3 left_orig_val = left_origins[i];
        Vector3 left_dir_val = left_directions[i];
        Vector3 right_orig_val = right_origins[i];
        Vector3 right_dir_val = right_directions[i];

        Gaze::GazeVector2 target_pixel_gaze(target_val.x, target_val.y);

        // Calibrate left eye
        Gaze::GazeCalibration calib_l;
        bool success_l = false;
        if (use_3d) {
            success_l = engine.calibrate_3d_bias(
                Gaze::GazeVector3(left_orig_val.x, left_orig_val.y, left_orig_val.z),
                Gaze::GazeVector3(left_dir_val.x, left_dir_val.y, left_dir_val.z),
                target_pixel_gaze,
                calib_l
            );
        } else {
            success_l = engine.calibrate_2d_bias(
                Gaze::GazeVector3(left_orig_val.x, left_orig_val.y, left_orig_val.z),
                Gaze::GazeVector3(left_dir_val.x, left_dir_val.y, left_dir_val.z),
                target_pixel_gaze,
                calib_l
            );
        }

        // Calibrate right eye
        Gaze::GazeCalibration calib_r;
        bool success_r = false;
        if (use_3d) {
            success_r = engine.calibrate_3d_bias(
                Gaze::GazeVector3(right_orig_val.x, right_orig_val.y, right_orig_val.z),
                Gaze::GazeVector3(right_dir_val.x, right_dir_val.y, right_dir_val.z),
                target_pixel_gaze,
                calib_r
            );
        } else {
            success_r = engine.calibrate_2d_bias(
                Gaze::GazeVector3(right_orig_val.x, right_orig_val.y, right_orig_val.z),
                Gaze::GazeVector3(right_dir_val.x, right_dir_val.y, right_dir_val.z),
                target_pixel_gaze,
                calib_r
            );
        }

        if (success_l && success_r) {
            sum_pitch += calib_l.bias_pitch + calib_r.bias_pitch;
            sum_yaw += calib_l.bias_yaw + calib_r.bias_yaw;
            sum_px += calib_l.bias_pixel_x + calib_r.bias_pixel_x;
            sum_py += calib_l.bias_pixel_y + calib_r.bias_pixel_y;
            valid_samples += 2;
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
