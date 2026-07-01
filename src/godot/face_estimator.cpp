#include "face_estimator.hpp"
#include "log.hpp"
#ifndef WEB_ENABLED
#include "yunet_pipeline.hpp"
#endif
#include "math_defs.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

static std::vector<uint8_t> load_file_buffer(const String &path) {
    std::vector<uint8_t> buffer;
    if (path.is_empty()) {
        return buffer;
    }
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (file.is_null()) {
        Gaze::log_error("FaceEstimatorLoadFileBufferFailed_Open", "path", path.utf8().get_data());
        return buffer;
    }
    uint64_t length = file->get_length();
    if (length == 0) {
        Gaze::log_error("FaceEstimatorLoadFileBufferFailed_Empty", "path", path.utf8().get_data());
        return buffer;
    }
    PackedByteArray godot_buffer = file->get_buffer(length);
    buffer.resize(length);
    std::memcpy(buffer.data(), godot_buffer.ptr(), length);
    return buffer;
}

static inline Transform3D get_head_transform_godot(const Gaze::GazeVector3& translation, const Gaze::GazeVector3& rotation) {
    Gaze::GazeBasis3D gt_basis = Gaze::rodrigues_to_basis(rotation);
    
    Basis basis(
        Vector3(gt_basis.x.x, gt_basis.x.y, gt_basis.x.z),
        Vector3(gt_basis.y.x, gt_basis.y.y, gt_basis.y.z),
        Vector3(gt_basis.z.x, gt_basis.z.y, gt_basis.z.z)
    );

    Vector3 pos(translation.x, translation.y, translation.z);

    return Transform3D(basis, pos);
}

void FaceEstimator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("initialize_estimator"), &FaceEstimator::initialize_estimator);
    ClassDB::bind_method(D_METHOD("stop_estimator"), &FaceEstimator::stop_estimator);
    ClassDB::bind_method(D_METHOD("get_yunet_model_path"), &FaceEstimator::get_yunet_model_path);
    ClassDB::bind_method(D_METHOD("set_yunet_model_path", "path"), &FaceEstimator::set_yunet_model_path);
    ClassDB::bind_method(D_METHOD("get_focal_length"), &FaceEstimator::get_focal_length);
    ClassDB::bind_method(D_METHOD("set_focal_length", "focal"), &FaceEstimator::set_focal_length);
    ClassDB::bind_method(D_METHOD("get_has_detected_face"), &FaceEstimator::get_has_detected_face);
    ClassDB::bind_method(D_METHOD("set_has_detected_face", "detected"), &FaceEstimator::set_has_detected_face);
    ClassDB::bind_method(D_METHOD("feed_pose_web", "detected", "translation", "rotation"), &FaceEstimator::feed_pose_web);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "yunet_model_path", PROPERTY_HINT_FILE), "set_yunet_model_path", "get_yunet_model_path");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "focal_length"), "set_focal_length", "get_focal_length");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "has_detected_face"), "set_has_detected_face", "get_has_detected_face");

    ADD_SIGNAL(MethodInfo("face_detection_changed", PropertyInfo(Variant::BOOL, "detected")));
    ADD_SIGNAL(MethodInfo("pose_estimated"));
}

FaceEstimator::FaceEstimator() {}

FaceEstimator::~FaceEstimator() {
    stop_estimator();
}

bool FaceEstimator::initialize_estimator() {
#ifndef WEB_ENABLED
    if (!pipeline) {
        String resolved_yunet = yunet_model_path.is_empty() ? "res://models/face_detection_yunet_2023mar.onnx" : yunet_model_path;
        std::vector<uint8_t> buffer = load_file_buffer(resolved_yunet);
        if (buffer.empty()) {
            Gaze::log_error("FaceEstimatorInitFailed_EmptyBuffer", "path", resolved_yunet.utf8().get_data());
            return false;
        }
        pipeline = new Gaze::YuNetPipeline(buffer);
    }
    pipeline->set_camera_focal_length_px(camera_focal_length_px);
    return pipeline->initialize();
#else
    return true;
#endif
}

void FaceEstimator::stop_estimator() {
    if (pipeline) {
        delete pipeline;
        pipeline = nullptr;
    }
    has_detected_face_val = false;
}

bool FaceEstimator::process_frame(const Gaze::Frame& frame, Gaze::EyeCrops& out_crops) {
    if (!pipeline) return false;
    bool success = pipeline->process_frame(frame, out_crops);
    
    bool state_changed = (out_crops.face_detected != has_detected_face_val);
    has_detected_face_val = out_crops.face_detected;

    if (has_detected_face_val) {
        Transform3D head_xform = get_head_transform_godot(out_crops.head_pose_translation, out_crops.head_pose_rotation);
        set_transform(head_xform);
        emit_signal("pose_estimated");
    }

    if (state_changed) {
        emit_signal("face_detection_changed", has_detected_face_val);
    }

    return success;
}

void FaceEstimator::set_yunet_model_path(String path) {
    yunet_model_path = path;
}

String FaceEstimator::get_yunet_model_path() const {
    return yunet_model_path;
}

void FaceEstimator::set_focal_length(double focal) {
    camera_focal_length_px = focal;
    if (pipeline) {
        pipeline->set_camera_focal_length_px(focal);
    }
}

double FaceEstimator::get_focal_length() const {
    return camera_focal_length_px;
}

void FaceEstimator::set_has_detected_face(bool detected) {
    has_detected_face_val = detected;
}

bool FaceEstimator::get_has_detected_face() const {
    return has_detected_face_val;
}

void FaceEstimator::set_pipeline_config(const Gaze::PipelineConfig& config) {
    if (pipeline) {
        pipeline->set_config(config);
    }
}

void FaceEstimator::feed_pose_web(bool detected, Vector3 translation, Vector3 rotation) {
    bool state_changed = (detected != has_detected_face_val);
    has_detected_face_val = detected;

    if (detected) {
        Gaze::GazeVector3 trans(translation.x, translation.y, translation.z);
        Gaze::GazeVector3 rot(rotation.x, rotation.y, rotation.z);
        Transform3D head_xform = get_head_transform_godot(trans, rot);
        set_transform(head_xform);
        emit_signal("pose_estimated");
    }

    if (state_changed) {
        emit_signal("face_detection_changed", has_detected_face_val);
    }
}

} // namespace godot
