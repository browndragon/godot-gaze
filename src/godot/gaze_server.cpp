#include "gaze_server.hpp"
#include "vision_server.hpp"
#include "log.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/templates/rid_owner.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <cmath>
#include <vector>
#include <algorithm>
#include "godot_files.hpp"
#include "../core/space_conversions.hpp"

#ifdef WEB_ENABLED
#include <godot_cpp/classes/java_script_bridge.hpp>
#include <godot_cpp/classes/java_script_object.hpp>
#endif

static_assert(sizeof(Gaze::GazeVector3) == sizeof(godot::Vector3), "Size of GazeVector3 must match godot::Vector3");
static_assert(alignof(Gaze::GazeVector3) == alignof(godot::Vector3), "Alignment of GazeVector3 must match godot::Vector3");

namespace godot {

// Structs definitions inside GazeServerImpl for Pimpl idiom
struct GazeServerImpl {
    struct DisplayInfo {
        Vector2 logical_size_px = Vector2(1920, 1080);
        Vector2 physical_size_mm = Vector2(345.0, 215.0);
        Ref<DeviceCalibration> device_calibration;
        Ref<BioCalibration> bio_calibration;

        struct BioCalibrationData {
            bool is_valid = false;
            double bias_pitch = 0.0;
            double bias_yaw = 0.0;
            double scale_yaw = 1.0;
            double scale_pitch = 1.0;
        } bio_data;

        Vector2 window_position_px = Vector2(0.0, 0.0);
        Transform2D viewport_transform;
    };

    struct CameraInfo {
        RID parent_display_rid;
        Vector3 offset = Vector3(0.0, 107.5, 0.0);
        double tilt = 0.0;
        RID vision_camera_rid;
    };

    struct FaceInfo {
        RID parent_camera_rid;
        Transform3D relative_transform;
        bool detected = false;
        
        Gaze::GazeVector3 head_pose_translation;
        Gaze::GazeVector3 head_pose_rotation;
    };

    struct EyeInfo {
        RID parent_face_rid;
        Transform3D relative_transform;
        
        Vector3 gaze_origin_cam;
        Vector3 gaze_direction_cam;
        
        Ref<Smoother> screen_smoother;
        Array smoother_state;
        
        Vector2 latest_projected_gaze;
        Vector2 latest_filtered_gaze;

        Ref<Image> left_eye_crop;
        Ref<Image> right_eye_crop;
        bool crop_requested = false;
    };

    RID_PtrOwner<DisplayInfo, true> display_owner;
    RID_PtrOwner<CameraInfo, true> camera_owner;
    RID_PtrOwner<FaceInfo, true> face_owner;
    RID_PtrOwner<EyeInfo, true> eye_owner;

    std::vector<RID> allocated_displays;
    std::vector<RID> allocated_cameras;
    std::vector<RID> allocated_faces;
    std::vector<RID> allocated_eyes;

    ~GazeServerImpl() {
        for (RID rid : allocated_eyes) {
            EyeInfo *info = eye_owner.get_or_null(rid);
            if (info) {
                memdelete(info);
            }
        }
        allocated_eyes.clear();

        for (RID rid : allocated_faces) {
            FaceInfo *info = face_owner.get_or_null(rid);
            if (info) {
                memdelete(info);
            }
        }
        allocated_faces.clear();

        for (RID rid : allocated_cameras) {
            CameraInfo *info = camera_owner.get_or_null(rid);
            if (info) {
                memdelete(info);
            }
        }
        allocated_cameras.clear();

        for (RID rid : allocated_displays) {
            DisplayInfo *info = display_owner.get_or_null(rid);
            if (info) {
                memdelete(info);
            }
        }
        allocated_displays.clear();
    }
};

// Aliases for shorter syntax inside this implementation file
using DisplayInfo = GazeServerImpl::DisplayInfo;
using CameraInfo = GazeServerImpl::CameraInfo;
using FaceInfo = GazeServerImpl::FaceInfo;
using EyeInfo = GazeServerImpl::EyeInfo;

GazeServer *GazeServer::singleton = nullptr;

void GazeServer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("display_create"), &GazeServer::display_create);
    ClassDB::bind_method(D_METHOD("display_set_geometry", "display_rid", "logical_size", "physical_size"), &GazeServer::display_set_geometry);
    ClassDB::bind_method(D_METHOD("display_set_device_calibration", "display_rid", "calibration"), &GazeServer::display_set_device_calibration);
    ClassDB::bind_method(D_METHOD("display_set_bio_calibration", "display_rid", "calibration"), &GazeServer::display_set_bio_calibration);
    ClassDB::bind_method(D_METHOD("display_set_window_parameters", "display_rid", "window_pos", "viewport_transform"), &GazeServer::display_set_window_parameters);
    ClassDB::bind_method(D_METHOD("display_free", "display_rid"), &GazeServer::display_free);

    ClassDB::bind_method(D_METHOD("camera_create", "display_rid"), &GazeServer::camera_create);
    ClassDB::bind_method(D_METHOD("camera_set_offsets", "camera_rid", "offset", "tilt"), &GazeServer::camera_set_offsets);
    ClassDB::bind_method(D_METHOD("camera_set_vision_rid", "camera_rid", "vision_camera_rid"), &GazeServer::camera_set_vision_rid);
    ClassDB::bind_method(D_METHOD("camera_free", "camera_rid"), &GazeServer::camera_free);

    ClassDB::bind_method(D_METHOD("face_tracker_create", "camera_rid"), &GazeServer::face_tracker_create);
    ClassDB::bind_method(D_METHOD("face_tracker_set_pose", "face_rid", "translation", "rotation", "detected"), &GazeServer::face_tracker_set_pose);
    ClassDB::bind_method(D_METHOD("face_tracker_free", "face_rid"), &GazeServer::face_tracker_free);
    ClassDB::bind_method(D_METHOD("get_head_rotation_from_face_tracker", "face_rid"), &GazeServer::get_head_rotation_from_face_tracker);
    ClassDB::bind_method(D_METHOD("get_head_translation_from_face_tracker", "face_rid"), &GazeServer::get_head_translation_from_face_tracker);

    ClassDB::bind_method(D_METHOD("eye_tracker_create", "face_rid"), &GazeServer::eye_tracker_create);
    ClassDB::bind_method(D_METHOD("eye_tracker_set_gaze", "eye_rid", "origin_cam", "direction_cam"), &GazeServer::eye_tracker_set_gaze);
    ClassDB::bind_method(D_METHOD("eye_tracker_set_smoother", "eye_rid", "smoother"), &GazeServer::eye_tracker_set_smoother);
    ClassDB::bind_method(D_METHOD("eye_tracker_set_crop_requested", "eye_rid", "requested"), &GazeServer::eye_tracker_set_crop_requested);
    ClassDB::bind_method(D_METHOD("eye_tracker_is_crop_requested", "eye_rid"), &GazeServer::eye_tracker_is_crop_requested);
    ClassDB::bind_method(D_METHOD("tracker_get_eye_crops", "eye_rid"), &GazeServer::tracker_get_eye_crops);
    ClassDB::bind_method(D_METHOD("eye_tracker_free", "eye_rid"), &GazeServer::eye_tracker_free);
    ClassDB::bind_method(D_METHOD("get_gaze_origin_from_eye_tracker", "eye_rid"), &GazeServer::get_gaze_origin_from_eye_tracker);
    ClassDB::bind_method(D_METHOD("get_gaze_direction_from_eye_tracker", "eye_rid"), &GazeServer::get_gaze_direction_from_eye_tracker);
    ClassDB::bind_method(D_METHOD("set_crops_on_eye_tracker", "eye_rid", "left_crop", "right_crop"), &GazeServer::set_crops_on_eye_tracker);
    ClassDB::bind_method(D_METHOD("reset_eye_tracker", "eye_rid"), &GazeServer::reset_eye_tracker);

    ClassDB::bind_method(D_METHOD("get_relative_transform", "entity_rid"), &GazeServer::get_relative_transform);
    ClassDB::bind_method(D_METHOD("get_gaze_screen", "display_rid", "smoothed"), &GazeServer::get_gaze_screen, DEFVAL(true));
    ClassDB::bind_method(D_METHOD("is_face_detected", "face_rid"), &GazeServer::is_face_detected);


    ClassDB::bind_method(D_METHOD("set_pipeline_config", "config"), &GazeServer::set_pipeline_config);
    ClassDB::bind_method(D_METHOD("start_processing"), &GazeServer::start_processing);
    ClassDB::bind_method(D_METHOD("stop_processing"), &GazeServer::stop_processing);
    ClassDB::bind_method(D_METHOD("trigger_process"), &GazeServer::trigger_process);

    ADD_SIGNAL(MethodInfo("gaze_data_ready", PropertyInfo(Variant::RID, "entity_rid")));
    ADD_SIGNAL(MethodInfo("gaze_frame_began", PropertyInfo(Variant::OBJECT, "frame", PROPERTY_HINT_RESOURCE_TYPE, "GazeFrame")));
    ADD_SIGNAL(MethodInfo("gaze_frame_ready", PropertyInfo(Variant::OBJECT, "frame", PROPERTY_HINT_RESOURCE_TYPE, "GazeFrame")));
}


#ifndef WEB_ENABLED
GazeServer::GazeServer() : impl(std::make_unique<GazeServerImpl>()), pipeline(std::make_unique<Gaze::GazeTrackingPipeline>()) {
#else
GazeServer::GazeServer() : impl(std::make_unique<GazeServerImpl>()) {
#endif
    singleton = this;
    ProjectSettings *ps = ProjectSettings::get_singleton();
    if (ps) {
        if (ps->has_setting("gaze/config/pitch_t_gain")) active_config.pitch_t_gain = ps->get_setting("gaze/config/pitch_t_gain");
        if (ps->has_setting("gaze/config/yaw_t_gain")) active_config.yaw_t_gain = ps->get_setting("gaze/config/yaw_t_gain");
        if (ps->has_setting("gaze/config/nose_y")) active_config.nose_y = ps->get_setting("gaze/config/nose_y");
        if (ps->has_setting("gaze/config/nose_z")) active_config.nose_z = ps->get_setting("gaze/config/nose_z");
        if (ps->has_setting("gaze/config/ipd_mm")) active_config.ipd_mm = ps->get_setting("gaze/config/ipd_mm");
        if (ps->has_setting("gaze/config/debug_image_throttle_interval")) active_config.debug_image_throttle_interval = ps->get_setting("gaze/config/debug_image_throttle_interval");

        if (ps->has_setting("gaze/config/desired_camera_width")) active_config.desired_camera_width = ps->get_setting("gaze/config/desired_camera_width");
        if (ps->has_setting("gaze/config/desired_camera_height")) active_config.desired_camera_height = ps->get_setting("gaze/config/desired_camera_height");
    }

#ifndef WEB_ENABLED
    if (pipeline) {
        for (size_t i = 0; i < pipeline->frame_pool.size; ++i) {
            Gaze::GazeFrameData* data = pipeline->frame_pool.get_frame(i);
            if (data) {
                GazeFrame* wrapper = memnew(GazeFrame);
                data->userdata = wrapper;
            }
        }
    }
#endif
}

GazeServer::~GazeServer() {
    Gaze::log_info("GazeServer_Destructor_Began");
    {
        std::lock_guard<std::recursive_mutex> lock(state_mutex);
        active_trackers = 0;
    }
    stop_processing();
#ifndef WEB_ENABLED
    if (pipeline) {
        for (size_t i = 0; i < pipeline->frame_pool.size; ++i) {
            Gaze::GazeFrameData* data = pipeline->frame_pool.get_frame(i);

            if (data && data->userdata) {
                GazeFrame* wrapper = static_cast<GazeFrame*>(data->userdata);
                memdelete(wrapper);
                data->userdata = nullptr;
            }
        }
    }
    Gaze::log_info("GazeServer_Destructor_PipelineReset_Began");
    pipeline.reset();
    Gaze::log_info("GazeServer_Destructor_PipelineReset_Finished");
#endif
    singleton = nullptr;
    Gaze::log_info("GazeServer_Destructor_Finished");
}


// Display RID Resource Management
RID GazeServer::display_create() {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    DisplayInfo *info = memnew(DisplayInfo);
    RID rid = impl->display_owner.make_rid(info);
    impl->allocated_displays.push_back(rid);
    return rid;
}

void GazeServer::display_set_geometry(RID p_display, Vector2 p_logical_size, Vector2 p_physical_size) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    DisplayInfo *info = impl->display_owner.get_or_null(p_display);
    ERR_FAIL_NULL(info);
    info->logical_size_px = p_logical_size;
    info->physical_size_mm = p_physical_size;
}

void GazeServer::display_set_device_calibration(RID p_display, const Ref<DeviceCalibration>& p_calibration) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    DisplayInfo *info = impl->display_owner.get_or_null(p_display);
    ERR_FAIL_NULL(info);
    info->device_calibration = p_calibration;
}

void GazeServer::display_set_bio_calibration(RID p_display, const Ref<BioCalibration>& p_calibration) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    DisplayInfo *info = impl->display_owner.get_or_null(p_display);
    ERR_FAIL_NULL(info);
    info->bio_calibration = p_calibration;

    if (p_calibration.is_valid()) {
        info->bio_data.is_valid = true;
        info->bio_data.bias_pitch = p_calibration->get_bias_pitch();
        info->bio_data.bias_yaw = p_calibration->get_bias_yaw();
        info->bio_data.scale_yaw = p_calibration->get_scale_yaw();
        info->bio_data.scale_pitch = p_calibration->get_scale_pitch();
    } else {
        info->bio_data.is_valid = false;
        info->bio_data.bias_pitch = 0.0;
        info->bio_data.bias_yaw = 0.0;
        info->bio_data.scale_yaw = 1.0;
        info->bio_data.scale_pitch = 1.0;
    }
}

void GazeServer::display_set_window_parameters(RID p_display, Vector2 p_window_pos, Transform2D p_viewport_transform) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    DisplayInfo *info = impl->display_owner.get_or_null(p_display);
    ERR_FAIL_NULL(info);
    info->window_position_px = p_window_pos;
    info->viewport_transform = p_viewport_transform;
}

void GazeServer::display_free(RID p_display) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    DisplayInfo *info = impl->display_owner.get_or_null(p_display);
    if (info) {
        auto it = std::find(impl->allocated_displays.begin(), impl->allocated_displays.end(), p_display);
        if (it != impl->allocated_displays.end()) {
            impl->allocated_displays.erase(it);
        }
        impl->display_owner.free(p_display);
        memdelete(info);
    }
}

// Camera RID Resource Management
RID GazeServer::camera_create(RID p_display) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    CameraInfo *info = memnew(CameraInfo);
    info->parent_display_rid = p_display;
    RID rid = impl->camera_owner.make_rid(info);
    impl->allocated_cameras.push_back(rid);
    return rid;
}

void GazeServer::camera_set_offsets(RID p_camera, Vector3 p_offset, double p_tilt) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    CameraInfo *info = impl->camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL(info);
    info->offset = p_offset;
    info->tilt = p_tilt;
}

void GazeServer::camera_set_vision_rid(RID p_camera, RID p_vision_camera) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    CameraInfo *info = impl->camera_owner.get_or_null(p_camera);
    ERR_FAIL_NULL(info);
    info->vision_camera_rid = p_vision_camera;
}

void GazeServer::camera_free(RID p_camera) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    CameraInfo *info = impl->camera_owner.get_or_null(p_camera);
    if (info) {
        auto it = std::find(impl->allocated_cameras.begin(), impl->allocated_cameras.end(), p_camera);
        if (it != impl->allocated_cameras.end()) {
            impl->allocated_cameras.erase(it);
        }
        impl->camera_owner.free(p_camera);
        memdelete(info);
    }
}

// Face RID Resource Management
RID GazeServer::face_tracker_create(RID p_camera) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    FaceInfo *info = memnew(FaceInfo);
    info->parent_camera_rid = p_camera;
    RID rid = impl->face_owner.make_rid(info);
    impl->allocated_faces.push_back(rid);
    return rid;
}

void GazeServer::face_tracker_set_pose(RID p_face, Vector3 p_translation, Vector3 p_rotation, bool p_detected) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    FaceInfo *face = impl->face_owner.get_or_null(p_face);
    ERR_FAIL_NULL(face);

    bool pose_changed = (face->detected != p_detected);
    face->detected = p_detected;
    face->head_pose_translation = Gaze::GazeVector3(p_translation.x, p_translation.y, p_translation.z);
    face->head_pose_rotation = Gaze::GazeVector3(p_rotation.x, p_rotation.y, p_rotation.z);

    if (p_detected) {
        Gaze::GazeTransform3D core_xform = Gaze::Inference::get_head_transform_in_camera_space(
            face->head_pose_translation,
            face->head_pose_rotation
        );

        // Convert the core GazeTransform3D (using custom structs) to Godot's Basis and Transform3D.
        // Godot Basis(Vector3, Vector3, Vector3) constructor takes the row vectors representing the X, Y, Z local axes.
        Basis f_basis(
            Vector3(core_xform.basis.x.x, core_xform.basis.x.y, core_xform.basis.x.z), // Row 0
            Vector3(core_xform.basis.y.x, core_xform.basis.y.y, core_xform.basis.y.z), // Row 1
            Vector3(core_xform.basis.z.x, core_xform.basis.z.y, core_xform.basis.z.z)  // Row 2
        );
        Vector3 translation_cam(core_xform.origin.x, core_xform.origin.y, core_xform.origin.z);
        face->relative_transform = Transform3D(f_basis, translation_cam);
    } else {
        face->relative_transform = Transform3D();
    }

    call_deferred("emit_signal", "gaze_data_ready", p_face);
}

void GazeServer::face_tracker_free(RID p_face) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    FaceInfo *info = impl->face_owner.get_or_null(p_face);
    if (info) {
        auto it = std::find(impl->allocated_faces.begin(), impl->allocated_faces.end(), p_face);
        if (it != impl->allocated_faces.end()) {
            impl->allocated_faces.erase(it);
        }
        impl->face_owner.free(p_face);
        memdelete(info);
    }
}

Vector3 GazeServer::get_head_rotation_from_face_tracker(RID p_face) const {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    FaceInfo *face = impl->face_owner.get_or_null(p_face);
    if (!face) return Vector3();
    return Vector3(face->head_pose_rotation.x, face->head_pose_rotation.y, face->head_pose_rotation.z);
}

Vector3 GazeServer::get_head_translation_from_face_tracker(RID p_face) const {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    FaceInfo *face = impl->face_owner.get_or_null(p_face);
    if (!face) return Vector3();
    return Vector3(face->head_pose_translation.x, face->head_pose_translation.y, face->head_pose_translation.z);
}

// Eye RID Resource Management
RID GazeServer::eye_tracker_create(RID p_face) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *info = memnew(EyeInfo);
    info->parent_face_rid = p_face;
    RID rid = impl->eye_owner.make_rid(info);
    impl->allocated_eyes.push_back(rid);
    return rid;
}

void GazeServer::eye_tracker_set_gaze(RID p_eye, Vector3 p_origin_cam, Vector3 p_direction_cam) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL(eye);

    eye->gaze_origin_cam = p_origin_cam;
    eye->gaze_direction_cam = p_direction_cam;

    FaceInfo *face = impl->face_owner.get_or_null(eye->parent_face_rid);
    if (face) {
        Vector3 f_pos = face->relative_transform.origin;
        Vector3 eye_origin_in_face = p_origin_cam - f_pos;
        
        Vector3 forward = -p_direction_cam.normalized();
        Vector3 up(0.0, 1.0, 0.0);
        Vector3 right = up.cross(forward).normalized();
        up = forward.cross(right).normalized();
        Basis eye_basis(right, up, forward);

        eye->relative_transform = Transform3D(eye_basis, eye_origin_in_face);

        CameraInfo *cam = impl->camera_owner.get_or_null(face->parent_camera_rid);
        if (cam) {
            DisplayInfo *disp = impl->display_owner.get_or_null(cam->parent_display_rid);
            if (disp) {
                Vector3 calibrated_dir = p_direction_cam;
                if (disp->bio_data.is_valid) {
                    double bias_pitch = disp->bio_data.bias_pitch;
                    double bias_yaw = disp->bio_data.bias_yaw;
                    double scale_yaw = disp->bio_data.scale_yaw;
                    double scale_pitch = disp->bio_data.scale_pitch;

                    Gaze::GazeVector3 raw_dir(p_direction_cam.x, p_direction_cam.y, p_direction_cam.z);
                    Gaze::GazeVector3 calib_v = Gaze::apply_3d_bias_vector(
                        raw_dir,
                        Gaze::GazeVector2(bias_pitch, bias_yaw),
                        Gaze::GazeVector2(scale_pitch, scale_yaw)
                    );
                    calibrated_dir = Vector3(calib_v.x, calib_v.y, calib_v.z);
                }

                Gaze::GazeVector2 pos_mm;
                if (Gaze::project_ray_to_screen_mm(
                        Gaze::GazeVector3(p_origin_cam.x, p_origin_cam.y, p_origin_cam.z),
                        Gaze::GazeVector3(calibrated_dir.x, calibrated_dir.y, calibrated_dir.z),
                        Gaze::GazeVector3(cam->offset.x, cam->offset.y, cam->offset.z),
                        cam->tilt,
                        Gaze::GazeVector2(disp->physical_size_mm.x, disp->physical_size_mm.y),
                        pos_mm
                    )) {
                    double scale_x = disp->logical_size_px.x / disp->physical_size_mm.x;
                    double scale_y = disp->logical_size_px.y / disp->physical_size_mm.y;
                    Vector2 px(pos_mm.x * scale_x, pos_mm.y * scale_y);

                    px = disp->viewport_transform.xform_inv(px);

                    eye->latest_projected_gaze = px;
                    
                    if (eye->screen_smoother.is_valid()) {
                        auto now = std::chrono::steady_clock::now();
                        double tstamp = std::chrono::duration<double>(now.time_since_epoch()).count();
                        eye->latest_filtered_gaze = eye->screen_smoother->_smoother_next(eye->smoother_state, tstamp, px);
                    } else {
                        eye->latest_filtered_gaze = px;
                    }
                }
            }
        }
    }

    call_deferred("emit_signal", "gaze_data_ready", p_eye);
}

void GazeServer::eye_tracker_set_smoother(RID p_eye, const Ref<Smoother>& p_smoother) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *info = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL(info);
    info->screen_smoother = p_smoother;
    if (info->screen_smoother.is_valid()) {
        info->smoother_state = info->screen_smoother->_smoother_init();
    }
}

void GazeServer::eye_tracker_set_crop_requested(RID p_eye, bool p_requested) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *info = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL(info);
    info->crop_requested = p_requested;
#ifdef WEB_ENABLED
    JavaScriptBridge *js = JavaScriptBridge::get_singleton();
    if (js) {
        Ref<JavaScriptObject> window = js->get_interface("window");
        if (window.is_valid()) {
            Ref<JavaScriptObject> godotGaze = window->get("godotGaze");
            if (godotGaze.is_valid()) {
                godotGaze->set("cropRequested", p_requested);
            }
        }
    }
#endif
}

bool GazeServer::eye_tracker_is_crop_requested(RID p_eye) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *info = impl->eye_owner.get_or_null(p_eye);
    ERR_FAIL_NULL_V(info, false);
    return info->crop_requested;
}

Array GazeServer::tracker_get_eye_crops(RID p_eye) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *info = impl->eye_owner.get_or_null(p_eye);
    Array arr;
    if (info) {
        arr.push_back(info->left_eye_crop);
        arr.push_back(info->right_eye_crop);
    } else {
        arr.push_back(Ref<Image>());
        arr.push_back(Ref<Image>());
    }
    return arr;
}

void GazeServer::eye_tracker_free(RID p_eye) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *info = impl->eye_owner.get_or_null(p_eye);
    if (info) {
        auto it = std::find(impl->allocated_eyes.begin(), impl->allocated_eyes.end(), p_eye);
        if (it != impl->allocated_eyes.end()) {
            impl->allocated_eyes.erase(it);
        }
        impl->eye_owner.free(p_eye);
        memdelete(info);
    }
}

Vector3 GazeServer::get_gaze_origin_from_eye_tracker(RID p_eye) const {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    if (!eye) return Vector3();
    return eye->gaze_origin_cam;
}

Vector3 GazeServer::get_gaze_direction_from_eye_tracker(RID p_eye) const {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    if (!eye) return Vector3(0.0, 0.0, -1.0);
    return eye->gaze_direction_cam;
}

void GazeServer::set_crops_on_eye_tracker(RID p_eye, const Ref<Image>& p_left_crop, const Ref<Image>& p_right_crop) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    if (eye) {
        eye->left_eye_crop = p_left_crop;
        eye->right_eye_crop = p_right_crop;
    }
}

void GazeServer::reset_eye_tracker(RID p_eye) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    EyeInfo *eye = impl->eye_owner.get_or_null(p_eye);
    if (eye) {
        eye->latest_projected_gaze = Vector2();
        eye->latest_filtered_gaze = Vector2();
        eye->relative_transform = Transform3D();
        call_deferred("emit_signal", "gaze_data_ready", p_eye);
    }
}

void GazeServer::emit_camera_frame_ready(RID p_vision_camera) {
    call_deferred("emit_signal", "gaze_data_ready", p_vision_camera);
}

// Unified Spatial and Coordinate queries
Transform3D GazeServer::get_relative_transform(RID p_entity) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    
    // 1. Display Transform (Identity relative to itself)
    if (impl->display_owner.owns(p_entity)) {
        return Transform3D();
    }

    // 2. Camera Transform (offset & tilt relative to display Z=0 plane)
    if (impl->camera_owner.owns(p_entity)) {
        CameraInfo *cam = impl->camera_owner.get_or_null(p_entity);
        if (cam) {
            Transform3D xform;
            xform.origin = cam->offset;
            xform.rotate(Vector3(1.0, 0.0, 0.0), cam->tilt * (M_PI / 180.0));
            return xform;
        }
    }

    // 3. Face Transform (computed PnP head pose relative to camera)
    if (impl->face_owner.owns(p_entity)) {
        FaceInfo *face = impl->face_owner.get_or_null(p_entity);
        if (face) {
            return face->relative_transform;
        }
    }

    // 4. Eye Transform (midpoint and gaze direction basis relative to face)
    if (impl->eye_owner.owns(p_entity)) {
        EyeInfo *eye = impl->eye_owner.get_or_null(p_entity);
        if (eye) {
            return eye->relative_transform;
        }
    }

    return Transform3D();
}

Vector2 GazeServer::get_gaze_screen(RID p_display, bool p_smoothed) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    DisplayInfo *disp = impl->display_owner.get_or_null(p_display);
    ERR_FAIL_NULL_V(disp, Vector2());

    // Resolve the first camera, face, and eye child chain
    RID target_camera;
    List<RID> cameras;
    impl->camera_owner.get_owned_list(&cameras);
    for (const RID &r : cameras) {
        CameraInfo *c = impl->camera_owner.get_or_null(r);
        if (c && c->parent_display_rid == p_display) {
            target_camera = r;
            break;
        }
    }
    if (!target_camera.is_valid()) return Vector2();

    RID target_face;
    List<RID> faces;
    impl->face_owner.get_owned_list(&faces);
    for (const RID &r : faces) {
        FaceInfo *f = impl->face_owner.get_or_null(r);
        if (f && f->parent_camera_rid == target_camera) {
            target_face = r;
            break;
        }
    }
    if (!target_face.is_valid()) return Vector2();

    RID target_eye;
    List<RID> eyes;
    impl->eye_owner.get_owned_list(&eyes);
    for (const RID &r : eyes) {
        EyeInfo *e = impl->eye_owner.get_or_null(r);
        if (e && e->parent_face_rid == target_face) {
            target_eye = r;
            break;
        }
    }
    if (!target_eye.is_valid()) return Vector2();

    EyeInfo *eye = impl->eye_owner.get_or_null(target_eye);
    if (!eye) return Vector2();

    return p_smoothed ? eye->latest_filtered_gaze : eye->latest_projected_gaze;
}

bool GazeServer::is_face_detected(RID p_face) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    FaceInfo *face = impl->face_owner.get_or_null(p_face);
    ERR_FAIL_NULL_V(face, false);
    return face->detected;
}

// Background Worker Loop
void GazeServer::start_processing() {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    active_trackers++;
    if (active_trackers > 1) {
        return; // Already started
    }
#ifndef WEB_ENABLED
    if (pipeline) {
        ProjectSettings *ps = ProjectSettings::get_singleton();
        if (ps) {
            String yunet_path = ps->get_setting("gaze/models/yunet_prefix");
            yunet_path = resolve_model_path(yunet_path);
            if (yunet_path.is_empty()) {
                yunet_path = resolve_model_path("face_detection_yunet_2023mar");
            }

            String gaze_path = ps->get_setting("gaze/models/gaze_prefix");
            gaze_path = resolve_model_path(gaze_path);
            if (gaze_path.is_empty()) {
                gaze_path = resolve_model_path("gaze-estimation-adas-0002");
            }

            std::vector<uint8_t> yunet_buffer = load_file_buffer(yunet_path);
            std::vector<uint8_t> gaze_buffer = load_file_buffer(gaze_path);

            pipeline->initialize(yunet_buffer, gaze_buffer);
        }
        pipeline->set_config(active_config);
        pipeline->start();
    }
#endif
}

void GazeServer::stop_processing() {
    Gaze::log_info("GazeServer_StopProcessing_Began", "active_trackers", active_trackers);
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    if (active_trackers > 0) {
        active_trackers--;
    }
    Gaze::log_info("GazeServer_StopProcessing_AfterDec", "active_trackers", active_trackers);
    if (active_trackers > 0) {
        return; // Other active trackers are still using it
    }
#ifndef WEB_ENABLED
    if (pipeline) {
        Gaze::log_info("GazeServer_StopProcessing_PipelineStop_Began");
        pipeline->stop();
        Gaze::log_info("GazeServer_StopProcessing_PipelineStop_Finished");
    }
#endif
    Gaze::log_info("GazeServer_StopProcessing_Finished");
}

void GazeServer::trigger_process() {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);

#ifndef WEB_ENABLED
    Gaze::GazeFrameData* completed_data = nullptr;
    while (pipeline && pipeline->pop_result(&completed_data)) {
        if (completed_data && completed_data->userdata) {
            GazeFrame* gaze_frame = static_cast<GazeFrame*>(completed_data->userdata);

            // Copy coordinates/metrics from native GazeFrameData to the wrapper GazeFrame
            gaze_frame->set_face_detected(completed_data->face_detected);
            gaze_frame->set_gaze_success(completed_data->gaze_success);
            gaze_frame->set_timestamp(completed_data->timestamp);

            const Vector3& head_t = reinterpret_cast<const Vector3&>(completed_data->head_translation);
            const Vector3& head_r = reinterpret_cast<const Vector3&>(completed_data->head_rotation);
            gaze_frame->set_head_translation(head_t);
            gaze_frame->set_head_rotation(head_r);

            const Vector3& gaze_o = reinterpret_cast<const Vector3&>(completed_data->gaze_origin);
            const Vector3& gaze_d = reinterpret_cast<const Vector3&>(completed_data->gaze_direction);
            gaze_frame->set_gaze_origin(gaze_o);
            gaze_frame->set_gaze_direction(gaze_d);

            gaze_frame->post_process();


            // Update spatial poses/coordinates for Godot's RID structures
            RID face_rid;
            std::memcpy(&face_rid, &completed_data->face_rid_val, sizeof(uint64_t));
            RID eye_rid;
            std::memcpy(&eye_rid, &completed_data->eye_rid_val, sizeof(uint64_t));

            if (completed_data->face_detected) {
                face_tracker_set_pose(face_rid, head_t, head_r, true);
                if (completed_data->gaze_success) {
                    eye_tracker_set_gaze(eye_rid, gaze_o, gaze_d);
                }
            } else {
                face_tracker_set_pose(face_rid, Vector3(), Vector3(), false);
                reset_eye_tracker(eye_rid);
            }

            // Emit the began signal (internal node processing updates begin)
            emit_signal("gaze_frame_began", gaze_frame);

            // Release the previous active read frame back to the pool
            if (active_read_data) {
                pipeline->frame_pool.release(active_read_data);
            }
            active_read_data = completed_data;

            // Emit the user-level event that the frame is ready
            emit_signal("gaze_frame_ready", gaze_frame);
        }
    }

    // 2. Grab current frames from VisionServer and push requests to GazeTrackingPipeline
    if (pipeline) {
        if (pipeline->is_busy()) {
            return;
        }
        List<RID> cameras;
        impl->camera_owner.get_owned_list(&cameras);
        for (const RID &r : cameras) {
            CameraInfo *cam = impl->camera_owner.get_or_null(r);
            if (cam && cam->vision_camera_rid.is_valid()) {
                Gaze::Frame current_frame;
                bool has_frame = VisionServer::get_singleton()->get_camera_current_frame(cam->vision_camera_rid, current_frame);
                if (has_frame) {
                    // Try to checkout a frame from the pool
                    Gaze::GazeFrameData* write_data = pipeline->frame_pool.take();
                    if (write_data) {
                        // Copy the raw camera feed to the write buffer
                        size_t frame_bytes = current_frame.width * current_frame.height * 3;
                        write_data->camera_raw_bgr.resize(frame_bytes);
                        std::memcpy(write_data->camera_raw_bgr.data(), current_frame.data, frame_bytes);
                        write_data->camera_width = current_frame.width;
                        write_data->camera_height = current_frame.height;
                        write_data->timestamp = current_frame.timestamp;
                        write_data->camera_focal_length_px = VisionServer::get_singleton()->camera_get_focal_length(cam->vision_camera_rid);
                        write_data->camera_fov_degrees = VisionServer::get_singleton()->camera_get_fov(cam->vision_camera_rid);

                        // Resolve the face RID associated with this camera
                        RID face_rid;
                        List<RID> faces;
                        impl->face_owner.get_owned_list(&faces);
                        for (const RID &f : faces) {
                            FaceInfo *face = impl->face_owner.get_or_null(f);
                            if (face && face->parent_camera_rid == r) {
                                face_rid = f;
                                break;
                            }
                        }

                        // Resolve the eye RID associated with the face
                        RID eye_rid;
                        if (face_rid.is_valid()) {
                            List<RID> eyes;
                            impl->eye_owner.get_owned_list(&eyes);
                            for (const RID &e : eyes) {
                                EyeInfo *eye = impl->eye_owner.get_or_null(e);
                                if (eye && eye->parent_face_rid == face_rid) {
                                    eye_rid = e;
                                    break;
                                }
                            }
                        }

                        write_data->face_rid_val = face_rid.get_id();
                        write_data->eye_rid_val = eye_rid.get_id();

                        // Setup zero-copy pointers to the wrapper's PackedByteArray buffers
                        GazeFrame* wrapper = static_cast<GazeFrame*>(write_data->userdata);
                        if (wrapper) {
                            wrapper->set_camera_size(Vector2i(current_frame.width, current_frame.height));
                            write_data->left_eye_buffer = wrapper->get_left_eye_buffer_ptr();
                            write_data->right_eye_buffer = wrapper->get_right_eye_buffer_ptr();
                            
                            // Initialize/resize the full crop image
                            wrapper->resize_full_crop(160, 128);
                            write_data->full_crop_buffer = wrapper->get_full_crop_buffer_ptr();
                            write_data->full_crop_bytes = wrapper->get_full_crop_bytes_size();
                        }

                        pipeline->push_frame_request(write_data);
                        emit_camera_frame_ready(cam->vision_camera_rid);
                    }
                }
            }
        }
    }
#endif
}



void GazeServer::set_pipeline_config(const Ref<GazePipelineConfig>& p_config) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex);
    if (p_config.is_valid()) {
        active_config = p_config->get_config();
#ifndef WEB_ENABLED
        if (pipeline) {
            pipeline->set_config(active_config);
        }
#endif
    }
}

#ifdef WEB_ENABLED
// Web Callback raw frame data injector (see gaze_tracker_web.cpp)
void GazeServer::feed_gaze_web_raw(const Array& args) {
    if (args.size() == 0) return;

    bool face_detected = args[0];
    
    // Query active Face/Eye RIDs
    RID active_face;
    List<RID> faces;
    impl->face_owner.get_owned_list(&faces);
    if (!faces.is_empty()) active_face = faces.front()->get();

    RID active_eye;
    List<RID> eyes;
    impl->eye_owner.get_owned_list(&eyes);
    if (!eyes.is_empty()) active_eye = eyes.front()->get();

    if (!face_detected) {
        face_tracker_set_pose(active_face, Vector3(), Vector3(), false);
        return;
    }

    if (args.size() >= 18 && active_face.is_valid() && active_eye.is_valid()) {
        // Unpack raw Inference space coordinates (in mm)
        Gaze::GazeVector3 left_eye_cv(args[1], args[2], args[3]);
        Gaze::GazeVector3 right_eye_cv(args[4], args[5], args[6]);
        Gaze::GazeVector3 dir_cv(args[7], args[8], args[9]);

        Gaze::GazeVector3 origin_cv = (left_eye_cv + right_eye_cv) * 0.5;

        // Build Head Transform
        Gaze::GazeVector3 head_trans(args[10], args[11], args[12]);
        Gaze::GazeVector3 head_rot(args[13], args[14], args[15]);

        Vector3 origin_cam(origin_cv.x, -origin_cv.y, -origin_cv.z);
        Vector3 dir_cam(dir_cv.x, -dir_cv.y, -dir_cv.z);

        face_tracker_set_pose(active_face, Vector3(head_trans.x, head_trans.y, head_trans.z), Vector3(head_rot.x, head_rot.y, head_rot.z), true);
        eye_tracker_set_gaze(active_eye, origin_cam, dir_cam);
    }
}
#endif

} // namespace godot

