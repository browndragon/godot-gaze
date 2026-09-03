/**
 * @file gaze_calibration_session.cpp
 * @brief Implement Godot wrapper for GazeCalibrationSession using the C++ core estimator
 */
#include "gaze_calibration_session.hpp"
#include "gaze_server.hpp"
#include "gaze_calibration_resource.hpp"
#include "gaze_calibration_estimator.hpp"
#include "log.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void GazeCalibrationSession::_bind_methods() {
    ClassDB::bind_method(D_METHOD("add_sample", "target_pixel_px", "gaze_origin", "gaze_direction"), &GazeCalibrationSession::add_sample);
    ClassDB::bind_method(D_METHOD("clear"), &GazeCalibrationSession::clear);
    ClassDB::bind_method(D_METHOD("get_sample_count"), &GazeCalibrationSession::get_sample_count);
    ClassDB::bind_method(D_METHOD("calculate_calibration", "initial_device_calib"), &GazeCalibrationSession::calculate_calibration, DEFVAL(Ref<DeviceCalibration>()));
    ClassDB::bind_method(D_METHOD("get_device_calibration"), &GazeCalibrationSession::get_device_calibration);
    ClassDB::bind_method(D_METHOD("get_bio_calibration"), &GazeCalibrationSession::get_bio_calibration);

    ClassDB::bind_method(D_METHOD("set_freeze_camera_params", "freeze"), &GazeCalibrationSession::set_freeze_camera_params);
    ClassDB::bind_method(D_METHOD("get_freeze_camera_params"), &GazeCalibrationSession::get_freeze_camera_params);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "freeze_camera_params"), "set_freeze_camera_params", "get_freeze_camera_params");

    ClassDB::bind_method(D_METHOD("set_target_pixels_px", "arr"), &GazeCalibrationSession::set_target_pixels_px);
    ClassDB::bind_method(D_METHOD("get_target_pixels_px"), &GazeCalibrationSession::get_target_pixels_px);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "target_pixels_px"), "set_target_pixels_px", "get_target_pixels_px");

    ClassDB::bind_method(D_METHOD("set_gaze_origins", "arr"), &GazeCalibrationSession::set_gaze_origins);
    ClassDB::bind_method(D_METHOD("get_gaze_origins"), &GazeCalibrationSession::get_gaze_origins);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "gaze_origins"), "set_gaze_origins", "get_gaze_origins");

    ClassDB::bind_method(D_METHOD("set_gaze_directions", "arr"), &GazeCalibrationSession::set_gaze_directions);
    ClassDB::bind_method(D_METHOD("get_gaze_directions"), &GazeCalibrationSession::get_gaze_directions);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "gaze_directions"), "set_gaze_directions", "get_gaze_directions");
}

void GazeCalibrationSession::add_sample(Vector2 target_pixel_px_val, Vector3 gaze_origin, Vector3 gaze_direction) {
    target_pixels_px.push_back(target_pixel_px_val);
    gaze_origins.push_back(gaze_origin);
    gaze_directions.push_back(gaze_direction);
}

void GazeCalibrationSession::clear() {
    target_pixels_px.clear();
    gaze_origins.clear();
    gaze_directions.clear();
}

int GazeCalibrationSession::get_sample_count() const {
    return target_pixels_px.size();
}

bool GazeCalibrationSession::calculate_calibration(const Ref<DeviceCalibration>& p_initial_device_calib) {
    Ref<StoredDeviceCalibration> dev_cal;
    dev_cal.instantiate();

    Ref<StoredBioCalibration> bio_cal;
    bio_cal.instantiate();

    result_device_calibration = dev_cal;
    result_bio_calibration = bio_cal;

    int count = get_sample_count();
    if (count == 0) {
        return false;
    }

    Ref<DeviceCalibration> initial_calib = p_initial_device_calib;
    if (!initial_calib.is_valid()) {
        Ref<GuessDeviceCalibration> guess;
        guess.instantiate();
        initial_calib = guess;
    }

    Vector2 screen_sz_mm = initial_calib->get_physical_size_mm();
    Vector2i screen_sz_px = initial_calib->get_logical_size_px();
    Vector3 init_off = initial_calib->get_camera_offset();
    double init_tilt = initial_calib->get_camera_tilt();

    if (screen_sz_px.x <= 0 || screen_sz_px.y <= 0) screen_sz_px = Vector2i(1920, 1080);
    if (screen_sz_mm.x <= 0.0 || screen_sz_mm.y <= 0.0) screen_sz_mm = Vector2(345.0, 215.0);

    dev_cal->set_physical_size_mm(screen_sz_mm);
    dev_cal->set_logical_size_px(screen_sz_px);

    std::vector<Gaze::CalibrationSample> core_samples;
    for (int i = 0; i < count; ++i) {
        if (i >= gaze_origins.size() || i >= gaze_directions.size()) {
            break;
        }

        Vector2 tgt_val = target_pixels_px[i];
        Vector3 orig_val = gaze_origins[i];
        Vector3 dir_val = gaze_directions[i];

        Gaze::CalibrationSample sample;
        sample.gaze_origin = Gaze::GodotCameraVector3(orig_val.x, orig_val.y, orig_val.z);
        sample.gaze_direction = Gaze::GodotCameraVector3(dir_val.x, dir_val.y, dir_val.z);
        sample.target_pos_mm = Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(
            (tgt_val.x / screen_sz_px.x) * screen_sz_mm.x,
            (tgt_val.y / screen_sz_px.y) * screen_sz_mm.y
        );
        core_samples.push_back(sample);
    }

    Gaze::GodotCameraVector3 out_off;
    double out_tilt = 0.0;
    double out_pitch = 0.0;
    double out_yaw = 0.0;

    bool success = Gaze::CalibrationEstimator::estimate(
        core_samples,
        Gaze::SpacedVector2<Gaze::Space::GodotDisplayMm>(screen_sz_mm.x, screen_sz_mm.y),
        Gaze::GodotCameraVector3(init_off.x, init_off.y, init_off.z),
        init_tilt,
        freeze_camera_params,
        out_off,
        out_tilt,
        out_pitch,
        out_yaw
    );

    if (success) {
        dev_cal->set_camera_offset(Vector3(out_off.x, out_off.y, out_off.z));
        dev_cal->set_camera_tilt(out_tilt);

        bio_cal->set_bias_pitch(out_pitch);
        bio_cal->set_bias_yaw(out_yaw);
        bio_cal->set_scale_pitch(1.0);
        bio_cal->set_scale_yaw(1.0);

        Gaze::log_info(1, "Calibration_Success",
                       "camera_offset_x", out_off.x,
                       "camera_offset_y", out_off.y,
                       "camera_offset_z", out_off.z,
                       "camera_tilt", out_tilt,
                       "bias_pitch", out_pitch,
                       "bias_yaw", out_yaw);
    } else {
        Gaze::log_warning("Calibration_Failed", "reason", "estimation failed to converge");
    }

    return success;
}

} // namespace godot
