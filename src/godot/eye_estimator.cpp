#include "eye_estimator.hpp"
#include "log.hpp"
#ifndef WEB_ENABLED
#include "opencv_gaze_model.hpp"
#endif
#include "face_estimator.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>

namespace godot {

static std::vector<uint8_t> load_file_buffer(const String &path) {
    std::vector<uint8_t> buffer;
    if (path.is_empty()) {
        return buffer;
    }
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (file.is_null()) {
        Gaze::log_error("EyeEstimatorLoadFileBufferFailed_Open", "path", path.utf8().get_data());
        return buffer;
    }
    uint64_t length = file->get_length();
    if (length == 0) {
        Gaze::log_error("EyeEstimatorLoadFileBufferFailed_Empty", "path", path.utf8().get_data());
        return buffer;
    }
    PackedByteArray godot_buffer = file->get_buffer(length);
    buffer.resize(length);
    std::memcpy(buffer.data(), godot_buffer.ptr(), length);
    return buffer;
}

void EyeEstimator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("initialize_estimator"), &EyeEstimator::initialize_estimator);
    ClassDB::bind_method(D_METHOD("stop_estimator"), &EyeEstimator::stop_estimator);
    ClassDB::bind_method(D_METHOD("get_gaze_model_path"), &EyeEstimator::get_gaze_model_path);
    ClassDB::bind_method(D_METHOD("set_gaze_model_path", "path"), &EyeEstimator::set_gaze_model_path);
    ClassDB::bind_method(D_METHOD("get_left_eye_crop"), &EyeEstimator::get_left_eye_crop);
    ClassDB::bind_method(D_METHOD("get_right_eye_crop"), &EyeEstimator::get_right_eye_crop);
    ClassDB::bind_method(D_METHOD("feed_gaze_web", "direction"), &EyeEstimator::feed_gaze_web);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "gaze_model_path", PROPERTY_HINT_FILE), "set_gaze_model_path", "get_gaze_model_path");

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
#ifndef WEB_ENABLED
    if (!model) {
        String resolved_gaze = gaze_model_path.is_empty() ? "res://models/gaze-estimation-adas-0002.xml" : gaze_model_path;
        if (resolved_gaze.ends_with(".xml")) {
            String bin_path = resolved_gaze.replace(".xml", ".bin");
            std::vector<uint8_t> xml_buf = load_file_buffer(resolved_gaze);
            std::vector<uint8_t> bin_buf = load_file_buffer(bin_path);
            if (xml_buf.empty() || bin_buf.empty()) {
                Gaze::log_error("EyeEstimatorInitFailed_EmptyOpenVINOBuffer", "xml_path", resolved_gaze.utf8().get_data());
                return false;
            }
            model = new Gaze::OpenCVGazeModel(xml_buf, bin_buf);
        } else {
            std::vector<uint8_t> buffer = load_file_buffer(resolved_gaze);
            if (buffer.empty()) {
                Gaze::log_error("EyeEstimatorInitFailed_EmptyBuffer", "path", resolved_gaze.utf8().get_data());
                return false;
            }
            model = new Gaze::OpenCVGazeModel(buffer);
        }
    }
    return model->initialize();
#else
    return true;
#endif
}

void EyeEstimator::stop_estimator() {
    if (model) {
        delete model;
        model = nullptr;
    }
    left_eye_crop.unref();
    right_eye_crop.unref();
}

bool EyeEstimator::opencv_gaze(const Gaze::EyeCrops& crops, Gaze::GazeVector3& out_gaze_dir) {
    if (!model) return false;
    bool success = model->estimate_raw_gaze(crops, out_gaze_dir);
    if (success) {
        // Convert crops to Godot Image resources for preview
        cv::Mat left_bgr(60, 60, CV_8UC3, const_cast<unsigned char*>(crops.left_eye_data));
        cv::Mat left_rgb;
        cv::cvtColor(left_bgr, left_rgb, cv::COLOR_BGR2RGB);
        PackedByteArray left_data;
        left_data.resize(left_rgb.total() * left_rgb.elemSize());
        std::memcpy(left_data.ptrw(), left_rgb.data, left_data.size());
        left_eye_crop = Image::create_from_data(60, 60, false, Image::FORMAT_RGB8, left_data);

        cv::Mat right_bgr(60, 60, CV_8UC3, const_cast<unsigned char*>(crops.right_eye_data));
        cv::Mat right_rgb;
        cv::cvtColor(right_bgr, right_rgb, cv::COLOR_BGR2RGB);
        PackedByteArray right_data;
        right_data.resize(right_rgb.total() * right_rgb.elemSize());
        std::memcpy(right_data.ptrw(), right_rgb.data, right_data.size());
        right_eye_crop = Image::create_from_data(60, 60, false, Image::FORMAT_RGB8, right_data);

        // Update local Node3D transform representing estimated gaze ray relative to Head
        FaceEstimator* face_est = Object::cast_to<FaceEstimator>(get_parent());
        Transform3D head_xform;
        if (face_est) {
            head_xform = face_est->get_transform();
        }

        // Midpoint of eyes as gaze origin in OpenCV camera space, then convert to camera space (Y=-Y, Z=-Z)
        Gaze::GazeVector3 gaze_origin_cv = (crops.left_eye_center_cam + crops.right_eye_center_cam) * 0.5;
        Vector3 gaze_origin_cam(gaze_origin_cv.x, -gaze_origin_cv.y, -gaze_origin_cv.z);
        Vector3 gaze_dir_cam(out_gaze_dir.x, -out_gaze_dir.y, -out_gaze_dir.z);

        Vector3 local_origin = head_xform.affine_inverse().xform(gaze_origin_cam);
        Vector3 local_dir = head_xform.basis.inverse().xform(gaze_dir_cam).normalized();

        Basis local_basis;
        if (local_dir.length_squared() > 1e-4) {
            Vector3 up = Vector3(0, 1, 0);
            if (std::abs(local_dir.dot(up)) > 0.99) {
                up = Vector3(0, 0, 1);
            }
            Vector3 z_axis = -local_dir;
            Vector3 x_axis = up.cross(z_axis).normalized();
            Vector3 y_axis = z_axis.cross(x_axis).normalized();
            local_basis = Basis(x_axis, y_axis, z_axis);
        }
        Transform3D eye_xform(local_basis, local_origin);
        set_transform(eye_xform);

        emit_signal("gaze_estimated");
        emit_signal("eye_crops_ready", left_eye_crop, right_eye_crop);
    }
    return success;
}

void EyeEstimator::set_gaze_model_path(String path) {
    gaze_model_path = path;
}

String EyeEstimator::get_gaze_model_path() const {
    return gaze_model_path;
}

void EyeEstimator::set_pipeline_config(const Gaze::PipelineConfig& config) {
    if (model) {
        model->set_config(config);
    }
}

Ref<Image> EyeEstimator::get_left_eye_crop() const {
    return left_eye_crop;
}

Ref<Image> EyeEstimator::get_right_eye_crop() const {
    return right_eye_crop;
}

void EyeEstimator::feed_gaze_web(Vector3 direction) {
    // On Web, update the Node3D local transform rotation using the received gaze direction relative to Head
    FaceEstimator* face_est = Object::cast_to<FaceEstimator>(get_parent());
    Transform3D head_xform;
    if (face_est) {
        head_xform = face_est->get_transform();
    }

    // Direction vector in camera space (Y=-Y, Z=-Z)
    Vector3 gaze_dir_cam(direction.x, -direction.y, -direction.z);
    
    // In web callbacks, we might not receive the exact left/right eye centers separately,
    // so we assume eye midpoint position is (0, 0, 0) or offset in head space.
    Vector3 local_origin(0, 0, 0);
    Vector3 local_dir = head_xform.basis.inverse().xform(gaze_dir_cam).normalized();

    Basis local_basis;
    if (local_dir.length_squared() > 1e-4) {
        Vector3 up = Vector3(0, 1, 0);
        if (std::abs(local_dir.dot(up)) > 0.99) {
            up = Vector3(0, 0, 1);
        }
        Vector3 z_axis = -local_dir;
        Vector3 x_axis = up.cross(z_axis).normalized();
        Vector3 y_axis = z_axis.cross(x_axis).normalized();
        local_basis = Basis(x_axis, y_axis, z_axis);
    }
    Transform3D eye_xform(local_basis, local_origin);
    set_transform(eye_xform);

    emit_signal("gaze_estimated");
}

} // namespace godot
