/**
 * @file gaze_calibration_session.cpp
 * @brief Implement Godot wrapper for GazeCalibrationSession using the C++ core estimator
 */
#include "gaze_calibration_session.hpp"
#include "gaze_tracker.hpp"
#include "gaze_calibration_resource.hpp"
#include "gaze_calibration_estimator.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void GazeCalibrationSession::_bind_methods() {
    ClassDB::bind_method(D_METHOD("add_sample", "target_pixel_ppix", "gaze_origin", "gaze_direction"), &GazeCalibrationSession::add_sample);
    ClassDB::bind_method(D_METHOD("clear"), &GazeCalibrationSession::clear);
    ClassDB::bind_method(D_METHOD("get_sample_count"), &GazeCalibrationSession::get_sample_count);
    ClassDB::bind_method(D_METHOD("calculate_calibration", "tracker"), &GazeCalibrationSession::calculate_calibration);

    ClassDB::bind_method(D_METHOD("set_target_pixels_ppix", "arr"), &GazeCalibrationSession::set_target_pixels_ppix);
    ClassDB::bind_method(D_METHOD("get_target_pixels_ppix"), &GazeCalibrationSession::get_target_pixels_ppix);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "target_pixels_ppix"), "set_target_pixels_ppix", "get_target_pixels_ppix");

    ClassDB::bind_method(D_METHOD("set_gaze_origins", "arr"), &GazeCalibrationSession::set_gaze_origins);
    ClassDB::bind_method(D_METHOD("get_gaze_origins"), &GazeCalibrationSession::get_gaze_origins);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "gaze_origins"), "set_gaze_origins", "get_gaze_origins");

    ClassDB::bind_method(D_METHOD("set_gaze_directions", "arr"), &GazeCalibrationSession::set_gaze_directions);
    ClassDB::bind_method(D_METHOD("get_gaze_directions"), &GazeCalibrationSession::get_gaze_directions);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "gaze_directions"), "set_gaze_directions", "get_gaze_directions");
}

void GazeCalibrationSession::add_sample(Vector2 target_pixel_ppix_val, Vector3 gaze_origin, Vector3 gaze_direction) {
    target_pixels_ppix.push_back(target_pixel_ppix_val);
    gaze_origins.push_back(gaze_origin);
    gaze_directions.push_back(gaze_direction);
}

void GazeCalibrationSession::clear() {
    target_pixels_ppix.clear();
    gaze_origins.clear();
    gaze_directions.clear();
}

int GazeCalibrationSession::get_sample_count() const {
    return target_pixels_ppix.size();
}

Ref<GazeCalibration> GazeCalibrationSession::calculate_calibration(GazeTracker *tracker) {
    Ref<GazeCalibration> res;
    res.instantiate();

    if (!tracker) {
        return res;
    }

    int count = get_sample_count();
    if (count == 0) {
        return res;
    }

    std::vector<Gaze::CalibrationSample> core_samples;
    for (int i = 0; i < count; ++i) {
        if (i >= gaze_origins.size() || i >= gaze_directions.size()) {
            break;
        }

        Vector2 tgt_val = target_pixels_ppix[i];
        Vector3 orig_val = gaze_origins[i];
        Vector3 dir_val = gaze_directions[i];

        Gaze::CalibrationSample sample;
        sample.gaze_origin = Gaze::GazeVector3(orig_val.x, orig_val.y, orig_val.z);
        sample.gaze_direction = Gaze::GazeVector3(dir_val.x, dir_val.y, dir_val.z);
        sample.target_pixel_ppix = Gaze::GazeVector2(tgt_val.x, tgt_val.y);
        core_samples.push_back(sample);
    }

    Vector2 init_sz = tracker->get_derived_pixel_size_mm();
    Vector3 init_off = tracker->get_camera_offset();
    double init_tilt = tracker->get_camera_tilt();
    Vector2i scr_px = tracker->platform_get_geometry().screen_size_ppix;

    Gaze::GazeVector2 out_sz;
    Gaze::GazeVector3 out_off;
    double out_tilt = 0.0;
    double out_pitch = 0.0;
    double out_yaw = 0.0;

    bool success = Gaze::CalibrationEstimator::estimate(
        core_samples,
        Gaze::GazeVector2(scr_px.x, scr_px.y),
        Gaze::GazeVector2(init_sz.x, init_sz.y),
        Gaze::GazeVector3(init_off.x, init_off.y, init_off.z),
        init_tilt,
        out_sz,
        out_off,
        out_tilt,
        out_pitch,
        out_yaw
    );

    if (success) {
        res->set_pixel_size_mm(Vector2(out_sz.x, out_sz.y));
        res->set_camera_offset(Vector3(out_off.x, out_off.y, out_off.z));
        res->set_camera_tilt(out_tilt);
        res->set_bias_pitch(out_pitch);
        res->set_bias_yaw(out_yaw);
        res->set_bias_pixel_x(0.0);
        res->set_bias_pixel_y(0.0);

        UtilityFunctions::print("[Calibration] Success. Solved parameters:");
        UtilityFunctions::print("  Pixel Size: (", out_sz.x, ", ", out_sz.y, ") mm");
        UtilityFunctions::print("  Camera Offset: (", out_off.x, ", ", out_off.y, ", ", out_off.z, ") mm");
        UtilityFunctions::print("  Camera Tilt: ", out_tilt, " degrees");
        UtilityFunctions::print("  Bias Pitch/Yaw: (", out_pitch, ", ", out_yaw, ") radians");
    } else {
        UtilityFunctions::printerr("[Calibration] Estimation failed to converge!");
    }

    return res;
}

} // namespace godot
