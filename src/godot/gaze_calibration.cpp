/**
 * @file gaze_calibration.cpp
 * @brief Implement GazeCalibration worker
 */
#include "gaze_calibration.hpp"
#include "gaze_server.hpp"
#include "gaze_device_profile.hpp"
#include "../core/projection_engine.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <cmath>
#include <algorithm>

namespace godot {

GazeCalibration::GazeCalibration() {
    spring.set_response_sec(0.30);
    spring.set_damping_ratio(1.0);
    settle_accumulator = Gaze::FillAccumulator(0.40, 0.25);
    sample_accumulator = Gaze::FillAccumulator(0.40, 0.25);
}

void GazeCalibration::_bind_methods() {
    // Discrete snapshot API
    ClassDB::bind_method(D_METHOD("add_event", "event", "target_px"), &GazeCalibration::add_event, DEFVAL(Vector2(NAN, NAN)));
    ClassDB::bind_method(D_METHOD("add_point", "target_px", "event"), &GazeCalibration::add_point);
    ClassDB::bind_method(D_METHOD("add_point_transforms", "target_px", "eye_transform", "head_transform"), &GazeCalibration::add_point_transforms);
    ClassDB::bind_method(D_METHOD("clear"), &GazeCalibration::clear);
    ClassDB::bind_method(D_METHOD("get_sample_count"), &GazeCalibration::get_sample_count);

    // Continuous Two-Stage API
    ClassDB::bind_method(D_METHOD("set_target", "target_px", "radius_px"), &GazeCalibration::set_target, DEFVAL(60.0));
    ClassDB::bind_method(D_METHOD("add_sample", "event", "delta"), &GazeCalibration::add_sample);
    ClassDB::bind_method(D_METHOD("cancel_target"), &GazeCalibration::cancel_target);
    ClassDB::bind_method(D_METHOD("get_display_fill"), &GazeCalibration::get_display_fill);
    ClassDB::bind_method(D_METHOD("get_stage"), &GazeCalibration::get_stage);
    ClassDB::bind_method(D_METHOD("is_point_complete"), &GazeCalibration::is_point_complete);
    ClassDB::bind_method(D_METHOD("get_smoothed_gaze"), &GazeCalibration::get_smoothed_gaze);
    ClassDB::bind_method(D_METHOD("get_target_position"), &GazeCalibration::get_target_position);
    ClassDB::bind_method(D_METHOD("get_target_radius"), &GazeCalibration::get_target_radius);

    BIND_ENUM_CONSTANT(STAGE_IDLE);
    BIND_ENUM_CONSTANT(STAGE_SETTLING);
    BIND_ENUM_CONSTANT(STAGE_SAMPLING);

    ClassDB::bind_method(D_METHOD("calibrate", "bio_profile"), &GazeCalibration::calibrate);
    ClassDB::bind_method(D_METHOD("install", "bio_profile", "path"), &GazeCalibration::install, DEFVAL(Variant()), DEFVAL(String("user://calibrations/bio_profile.cfg")));
}

void GazeCalibration::_reset_active_sums() {
    active_sum_sin_pitch = 0.0;
    active_sum_cos_pitch = 0.0;
    active_sum_sin_yaw = 0.0;
    active_sum_cos_yaw = 0.0;
    active_sample_count = 0;
}

void GazeCalibration::_compute_angles(const Vector2 &p_target_px, const Transform3D &p_eye_transform, const Transform3D &p_head_transform,
                                     double &r_targ_pitch, double &r_targ_yaw, double &r_meas_pitch, double &r_meas_yaw) {
    GazeServer *server = GazeServer::get_singleton();
    if (!server) {
        return;
    }
    Ref<GazeDeviceProfile> device = server->get_device_profile();
    if (device.is_null()) {
        return;
    }

    Vector2 tgt = p_target_px;
    if (std::isnan(tgt.x) || std::isnan(tgt.y)) {
        tgt = Vector2(device->get_logical_size_px()) * 0.5;
    }

    // Target ray in camera space:
    Vector3 pt_target_cam = device->unproject_px_to_cam_mm(tgt);
    Vector3 eye_origin_cam = p_eye_transform.origin;
    Vector3 dir_target_cam = (pt_target_cam - eye_origin_cam).normalized();

    // Measured gaze ray in camera space (Godot forward is -basis.z):
    Vector3 dir_meas_cam = -p_eye_transform.basis.get_column(2).normalized();

    // Head basis (GodotFaceLocal -> GodotCamera):
    Basis hb = p_head_transform.basis;
    Gaze::SpacedBasis<Gaze::Space::GodotFaceLocal, Gaze::Space::GodotCamera> head_basis_cam(
        Gaze::GodotCameraVector3(hb.get_column(0).x, hb.get_column(0).y, hb.get_column(0).z),
        Gaze::GodotCameraVector3(hb.get_column(1).x, hb.get_column(1).y, hb.get_column(1).z),
        Gaze::GodotCameraVector3(hb.get_column(2).x, hb.get_column(2).y, hb.get_column(2).z)
    );

    // Transform rays to head space:
    Gaze::SpacedBasis<Gaze::Space::GodotCamera, Gaze::Space::GodotFaceLocal> cam_to_head = head_basis_cam.transposed();
    Gaze::GodotFaceVector3 d_target_head = cam_to_head.transform(
        Gaze::GodotCameraVector3(dir_target_cam.x, dir_target_cam.y, dir_target_cam.z)
    ).normalized();
    Gaze::GodotFaceVector3 d_meas_head = cam_to_head.transform(
        Gaze::GodotCameraVector3(dir_meas_cam.x, dir_meas_cam.y, dir_meas_cam.z)
    ).normalized();

    // Spherical angles in GodotFaceLocal (-Z forward, +X right, +Y up):
    r_targ_pitch = std::asin(std::clamp(d_target_head.y, -1.0, 1.0));
    r_targ_yaw = std::atan2(d_target_head.x, -d_target_head.z);
    r_meas_pitch = std::asin(std::clamp(d_meas_head.y, -1.0, 1.0));
    r_meas_yaw = std::atan2(d_meas_head.x, -d_meas_head.z);
}

void GazeCalibration::_add_sample(const Vector2 &p_target_px, const Transform3D &p_eye_transform, const Transform3D &p_head_transform) {
    Sample s;
    s.target_px = p_target_px;
    _compute_angles(p_target_px, p_eye_transform, p_head_transform, s.target_pitch, s.target_yaw, s.meas_pitch, s.meas_yaw);
    samples.push_back(s);
}

void GazeCalibration::set_target(const Vector2 &p_target_px, double p_radius_px) {
    current_target_px = p_target_px;
    target_radius_px = std::max(5.0, p_radius_px);
    has_active_target = true;
    current_stage = STAGE_SETTLING;
    settle_accumulator.reset();
    sample_accumulator.reset();
    _reset_active_sums();
    point_just_completed = false;
}

void GazeCalibration::cancel_target() {
    has_active_target = false;
    current_stage = STAGE_IDLE;
    settle_accumulator.reset();
    sample_accumulator.reset();
    _reset_active_sums();
    point_just_completed = false;
}

double GazeCalibration::get_display_fill() const {
    if (!has_active_target) {
        return 0.0;
    }
    if (current_stage == STAGE_SETTLING) {
        return 0.5 * settle_accumulator.get_fill();
    }
    if (current_stage == STAGE_SAMPLING) {
        return 0.5 + 0.5 * sample_accumulator.get_fill();
    }
    return 0.0;
}

Vector2 GazeCalibration::get_smoothed_gaze() const {
    Gaze::DampedSpring::Point2D p = spring.get_position();
    return Vector2(p.x, p.y);
}

bool GazeCalibration::add_sample(const Ref<InputEventGaze> &p_event, double p_delta) {
    point_just_completed = false;
    if (!has_active_target || p_event.is_null()) {
        return false;
    }

    Vector2 gaze_px = p_event->get_raw_eye_gaze();
    if (gaze_px == Vector2(0.0f, 0.0f)) {
        gaze_px = p_event->get_eye_gaze();
    }

    spring.update(p_delta, Gaze::DampedSpring::Point2D(gaze_px.x, gaze_px.y));
    Gaze::DampedSpring::Point2D cur_pos = spring.get_position();
    Vector2 smoothed(cur_pos.x, cur_pos.y);

    double dist = (smoothed - current_target_px).length();
    double error = std::max(0.0, dist - target_radius_px);

    if (current_stage == STAGE_SETTLING) {
        settle_accumulator.update(p_delta, error);
        if (settle_accumulator.is_filled()) {
            current_stage = STAGE_SAMPLING;
            sample_accumulator.reset();
            _reset_active_sums();
        }
    } else if (current_stage == STAGE_SAMPLING) {
        sample_accumulator.update(p_delta, error);

        if (sample_accumulator.is_empty()) {
            // Looked away and drained completely! Abort back to Stage 1 settling
            current_stage = STAGE_SETTLING;
            settle_accumulator.reset();
            _reset_active_sums();
        } else if (error <= 0.0 && p_event->is_face_tracked()) {
            // On target during sampling: accumulate running circular-mean sums
            Transform3D eye_xf = p_event->get_raw_eye_transform();
            if (eye_xf == Transform3D()) {
                eye_xf = p_event->get_eye_transform();
            }
            double tp = 0.0, ty = 0.0, mp = 0.0, my = 0.0;
            _compute_angles(current_target_px, eye_xf, p_event->get_head_transform(), tp, ty, mp, my);

            double dp = Gaze::wrap_angle_rad(tp - mp);
            double dy = Gaze::wrap_angle_rad(ty - my);

            active_sum_sin_pitch += std::sin(dp);
            active_sum_cos_pitch += std::cos(dp);
            active_sum_sin_yaw += std::sin(dy);
            active_sum_cos_yaw += std::cos(dy);
            active_sample_count++;
        }

        if (sample_accumulator.is_filled()) {
            // Point dwell complete! Lock in sample
            if (active_sample_count > 0) {
                double avg_bias_pitch = std::atan2(active_sum_sin_pitch, active_sum_cos_pitch);
                double avg_bias_yaw = std::atan2(active_sum_sin_yaw, active_sum_cos_yaw);

                Sample s;
                s.target_px = current_target_px;
                s.target_pitch = avg_bias_pitch;
                s.meas_pitch = 0.0;
                s.target_yaw = avg_bias_yaw;
                s.meas_yaw = 0.0;
                samples.push_back(s);
            }

            has_active_target = false;
            current_stage = STAGE_IDLE;
            point_just_completed = true;
            return true;
        }
    }

    return false;
}

void GazeCalibration::add_event(const Ref<InputEventGaze> &p_event, const Vector2 &p_target_px) {
    if (p_event.is_null()) {
        return;
    }
    Transform3D eye_xf = p_event->get_raw_eye_transform();
    if (eye_xf == Transform3D()) {
        eye_xf = p_event->get_eye_transform();
    }
    _add_sample(p_target_px, eye_xf, p_event->get_head_transform());
}

void GazeCalibration::add_point(const Vector2 &p_target_px, const Ref<InputEventGaze> &p_event) {
    add_event(p_event, p_target_px);
}

void GazeCalibration::add_point_transforms(const Vector2 &p_target_px, const Transform3D &p_eye_transform, const Transform3D &p_head_transform) {
    _add_sample(p_target_px, p_eye_transform, p_head_transform);
}

void GazeCalibration::clear() {
    samples.clear();
}

Error GazeCalibration::calibrate(const Ref<GazeBioProfile> &p_bio_profile) {
    if (p_bio_profile.is_null()) {
        return ERR_INVALID_PARAMETER;
    }
    if (samples.empty()) {
        return ERR_INVALID_DATA;
    }

    std::vector<double> meas_pitch;
    std::vector<double> targ_pitch;
    std::vector<double> meas_yaw;
    std::vector<double> targ_yaw;
    meas_pitch.reserve(samples.size());
    targ_pitch.reserve(samples.size());
    meas_yaw.reserve(samples.size());
    targ_yaw.reserve(samples.size());

    for (const auto &s : samples) {
        meas_pitch.push_back(s.meas_pitch);
        targ_pitch.push_back(s.target_pitch);
        meas_yaw.push_back(s.meas_yaw);
        targ_yaw.push_back(s.target_yaw);
    }

    double bias_pitch_deg = 0.0;
    if (!Gaze::solve_head_space_bias(meas_pitch, targ_pitch, bias_pitch_deg)) {
        return FAILED;
    }

    double bias_yaw_deg = 0.0;
    if (!Gaze::solve_head_space_bias(meas_yaw, targ_yaw, bias_yaw_deg)) {
        return FAILED;
    }

    p_bio_profile->set_bias_pitch_deg(bias_pitch_deg);
    p_bio_profile->set_bias_yaw_deg(bias_yaw_deg);
    return OK;
}

Ref<GazeBioProfile> GazeCalibration::install(const Ref<GazeBioProfile> &p_bio_profile, const String &p_path) {
    Ref<GazeBioProfile> bio = p_bio_profile;
    if (bio.is_null()) {
        GazeServer *server = GazeServer::get_singleton();
        if (server) {
            bio = server->get_bio_profile();
        }
        if (bio.is_null()) {
            bio.instantiate();
        }
    }

    Error err = calibrate(bio);
    if (err != OK) {
        return Ref<GazeBioProfile>();
    }

    GazeServer *server = GazeServer::get_singleton();
    if (server) {
        server->set_bio_profile(bio);
    }

    if (!p_path.is_empty()) {
        bio->save_to_file(p_path);
    }

    return bio;
}

} // namespace godot
