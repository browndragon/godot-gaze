#include "eye_estimator.hpp"
#include "gaze_server.hpp"
#include "gaze_frame.hpp"
#include "face_estimator.hpp"

#include "log.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void EyeEstimator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("initialize_estimator"), &EyeEstimator::initialize_estimator);
    ClassDB::bind_method(D_METHOD("stop_estimator"), &EyeEstimator::stop_estimator);
    ClassDB::bind_method(D_METHOD("get_gaze_model_prefix"), &EyeEstimator::get_gaze_model_prefix);
    ClassDB::bind_method(D_METHOD("set_gaze_model_prefix", "prefix"), &EyeEstimator::set_gaze_model_prefix);
    ClassDB::bind_method(D_METHOD("get_left_eye_crop"), &EyeEstimator::get_left_eye_crop);
    ClassDB::bind_method(D_METHOD("get_right_eye_crop"), &EyeEstimator::get_right_eye_crop);
    ClassDB::bind_method(D_METHOD("get_eye_rid"), &EyeEstimator::get_eye_rid);
    ClassDB::bind_method(D_METHOD("_on_gaze_data_ready", "rid"), &EyeEstimator::_on_gaze_data_ready);
    ClassDB::bind_method(D_METHOD("_on_gaze_frame_began", "frame"), &EyeEstimator::_on_gaze_frame_began);


    ADD_PROPERTY(PropertyInfo(Variant::STRING, "gaze_model_prefix", PROPERTY_HINT_FILE), "set_gaze_model_prefix", "get_gaze_model_prefix");

    ADD_SIGNAL(MethodInfo("gaze_estimated"));
    ADD_SIGNAL(MethodInfo("eye_crops_ready", 
                          PropertyInfo(Variant::OBJECT, "left_eye", PROPERTY_HINT_RESOURCE_TYPE, "Image"), 
                          PropertyInfo(Variant::OBJECT, "right_eye", PROPERTY_HINT_RESOURCE_TYPE, "Image")));
}

EyeEstimator::EyeEstimator() {}

EyeEstimator::~EyeEstimator() {
    stop_estimator();
}

bool EyeEstimator::initialize_estimator() {
    GazeServer *gs = GazeServer::get_singleton();
    if (gs) {
        gs->connect("gaze_data_ready", Callable(this, "_on_gaze_data_ready"));
        gs->connect("gaze_frame_began", Callable(this, "_on_gaze_frame_began"));
    }
    Gaze::log_info("EyeEstimator_Initialized");
    return true;
}

void EyeEstimator::stop_estimator() {
    GazeServer *gs = GazeServer::get_singleton();
    if (gs) {
        if (gs->is_connected("gaze_data_ready", Callable(this, "_on_gaze_data_ready"))) {
            gs->disconnect("gaze_data_ready", Callable(this, "_on_gaze_data_ready"));
        }
        if (gs->is_connected("gaze_frame_began", Callable(this, "_on_gaze_frame_began"))) {
            gs->disconnect("gaze_frame_began", Callable(this, "_on_gaze_frame_began"));
        }
    }
    eye_rid = RID();

    left_eye_crop.unref();
    right_eye_crop.unref();
}

void EyeEstimator::set_gaze_model_prefix(String prefix) {
    gaze_model_prefix = prefix;
    // Set in ProjectSettings directly so GazeServer picks it up
    ProjectSettings::get_singleton()->set_setting("gaze/models/gaze_prefix", prefix);
}

String EyeEstimator::get_gaze_model_prefix() const {
    return gaze_model_prefix;
}

Ref<Image> EyeEstimator::get_left_eye_crop() const {
    return left_eye_crop;
}

Ref<Image> EyeEstimator::get_right_eye_crop() const {
    return right_eye_crop;
}

void EyeEstimator::_on_gaze_data_ready(RID p_rid) {
    if (p_rid == eye_rid) {
        GazeServer *gs = GazeServer::get_singleton();
        if (gs) {
            Transform3D relative_xform = gs->get_relative_transform(eye_rid);
            set_transform(relative_xform);

            emit_signal("gaze_estimated");
        }
    }
}


void EyeEstimator::_on_gaze_frame_began(GazeFrame* frame) {
    if (frame) {
        GazeServer *gs = GazeServer::get_singleton();
        if (gs) {
            Transform3D relative_xform = gs->get_relative_transform(eye_rid);
            set_transform(relative_xform);

            // Fetch preallocated crops directly from GazeFrame
            left_eye_crop = frame->get_left_eye_crop();
            right_eye_crop = frame->get_right_eye_crop();

            Gaze::log_info(4, "EyeEstimator_GazeFrameBegan", "left_crop_valid", left_eye_crop.is_valid());

            emit_signal("gaze_estimated");
            emit_signal("eye_crops_ready", left_eye_crop, right_eye_crop);
        }
    }
}



} // namespace godot
