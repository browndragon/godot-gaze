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

void GazeCalibration::_bind_methods() {
    ClassDB::bind_method(D_METHOD("add_event", "event", "target_px"), &GazeCalibration::add_event, DEFVAL(Vector2(NAN, NAN)));
    ClassDB::bind_method(D_METHOD("add_point", "target_px", "event"), &GazeCalibration::add_point);
    ClassDB::bind_method(D_METHOD("add_point_transforms", "target_px", "eye_transform", "head_transform"), &GazeCalibration::add_point_transforms);
    ClassDB::bind_method(D_METHOD("clear"), &GazeCalibration::clear);
    ClassDB::bind_method(D_METHOD("get_sample_count"), &GazeCalibration::get_sample_count);
    ClassDB::bind_method(D_METHOD("calibrate", "bio_profile"), &GazeCalibration::calibrate);
    ClassDB::bind_method(D_METHOD("install", "bio_profile", "path"), &GazeCalibration::install, DEFVAL(Variant()), DEFVAL(String("user://calibrations/bio_profile.cfg")));
}

void GazeCalibration::_add_sample(const Vector2 &p_target_px, const Transform3D &p_eye_transform, const Transform3D &p_head_transform) {
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
    // pitch: psi = asin(clamp(v_y, -1, 1))
    // yaw: phi = atan2(v_x, -v_z)
    Sample s;
    s.target_px = tgt;
    s.target_pitch = std::asin(std::clamp(d_target_head.y, -1.0, 1.0));
    s.target_yaw = std::atan2(d_target_head.x, -d_target_head.z);

    s.meas_pitch = std::asin(std::clamp(d_meas_head.y, -1.0, 1.0));
    s.meas_yaw = std::atan2(d_meas_head.x, -d_meas_head.z);

    samples.push_back(s);
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
