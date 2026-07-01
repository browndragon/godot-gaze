#include "face_estimator.hpp"
#include "gaze_server.hpp"
#include "camera_sensor.hpp"
#include "log.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void FaceEstimator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("initialize_estimator"), &FaceEstimator::initialize_estimator);
    ClassDB::bind_method(D_METHOD("stop_estimator"), &FaceEstimator::stop_estimator);
    ClassDB::bind_method(D_METHOD("get_yunet_model_prefix"), &FaceEstimator::get_yunet_model_prefix);
    ClassDB::bind_method(D_METHOD("set_yunet_model_prefix", "prefix"), &FaceEstimator::set_yunet_model_prefix);
    ClassDB::bind_method(D_METHOD("get_focal_length"), &FaceEstimator::get_focal_length);
    ClassDB::bind_method(D_METHOD("set_focal_length", "focal"), &FaceEstimator::set_focal_length);
    ClassDB::bind_method(D_METHOD("get_camera_fov"), &FaceEstimator::get_camera_fov);
    ClassDB::bind_method(D_METHOD("set_camera_fov", "fov"), &FaceEstimator::set_camera_fov);
    ClassDB::bind_method(D_METHOD("get_has_detected_face"), &FaceEstimator::get_has_detected_face);
    ClassDB::bind_method(D_METHOD("set_has_detected_face", "detected"), &FaceEstimator::set_has_detected_face);
    ClassDB::bind_method(D_METHOD("get_face_rid"), &FaceEstimator::get_face_rid);
    ClassDB::bind_method(D_METHOD("_on_gaze_data_ready", "rid"), &FaceEstimator::_on_gaze_data_ready);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "yunet_model_prefix", PROPERTY_HINT_FILE), "set_yunet_model_prefix", "get_yunet_model_prefix");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "focal_length"), "set_focal_length", "get_focal_length");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "camera_fov"), "set_camera_fov", "get_camera_fov");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "has_detected_face"), "set_has_detected_face", "get_has_detected_face");

    ADD_SIGNAL(MethodInfo("face_detection_changed", PropertyInfo(Variant::BOOL, "detected")));
    ADD_SIGNAL(MethodInfo("pose_estimated"));
}

FaceEstimator::FaceEstimator() {}

FaceEstimator::~FaceEstimator() {
    stop_estimator();
}

bool FaceEstimator::initialize_estimator() {
    GazeServer *gs = GazeServer::get_singleton();
    if (gs) {
        gs->connect("gaze_data_ready", Callable(this, "_on_gaze_data_ready"));
    }
    Gaze::log_info("FaceEstimator_Initialized");
    return true;
}

void FaceEstimator::stop_estimator() {
    GazeServer *gs = GazeServer::get_singleton();
    if (gs) {
        if (gs->is_connected("gaze_data_ready", Callable(this, "_on_gaze_data_ready"))) {
            gs->disconnect("gaze_data_ready", Callable(this, "_on_gaze_data_ready"));
        }
    }
    face_rid = RID();
    has_detected_face_val = false;
}

void FaceEstimator::set_yunet_model_prefix(String prefix) {
    yunet_model_prefix = prefix;
    // Set in ProjectSettings directly so GazeServer picks it up
    ProjectSettings::get_singleton()->set_setting("gaze/models/yunet_prefix", prefix);
}

String FaceEstimator::get_yunet_model_prefix() const {
    return yunet_model_prefix;
}

void FaceEstimator::set_focal_length(double focal) {
    camera_focal_length_px = focal;
}

double FaceEstimator::get_focal_length() const {
    return camera_focal_length_px;
}

void FaceEstimator::set_camera_fov(double fov) {
    camera_fov_degrees = fov;
}

double FaceEstimator::get_camera_fov() const {
    return camera_fov_degrees;
}

void FaceEstimator::set_has_detected_face(bool detected) {
    has_detected_face_val = detected;
}

bool FaceEstimator::get_has_detected_face() const {
    return has_detected_face_val;
}

void FaceEstimator::_on_gaze_data_ready(RID p_rid) {
    if (p_rid == face_rid) {
        GazeServer *gs = GazeServer::get_singleton();
        if (gs) {
            Transform3D relative_xform = gs->get_relative_transform(face_rid);
            set_transform(relative_xform);

            bool detected = gs->is_face_detected(face_rid);
            bool state_changed = (detected != has_detected_face_val);
            has_detected_face_val = detected;

            if (detected) {
                emit_signal("pose_estimated");
            }
            if (state_changed) {
                emit_signal("face_detection_changed", has_detected_face_val);
            }
        }
    }
}

} // namespace godot
