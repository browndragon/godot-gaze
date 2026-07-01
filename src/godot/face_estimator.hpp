#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/rid.hpp>

namespace godot {

class FaceEstimator : public Node3D {
    GDCLASS(FaceEstimator, Node3D);

private:
    String yunet_model_prefix;
    double camera_focal_length_px = -1.0;
    double camera_fov_degrees = 35.488537576579634;
    bool has_detected_face_val = false;
    RID face_rid;

protected:
    static void _bind_methods();

public:
    FaceEstimator();
    virtual ~FaceEstimator();

    bool initialize_estimator();
    void stop_estimator();

    void set_yunet_model_prefix(String path);
    String get_yunet_model_prefix() const;

    void set_focal_length(double focal);
    double get_focal_length() const;

    void set_camera_fov(double fov);
    double get_camera_fov() const;

    void set_has_detected_face(bool detected);
    bool get_has_detected_face() const;

    RID get_face_rid() const { return face_rid; }
    void set_face_rid(RID p_rid) { face_rid = p_rid; }

    void _on_gaze_data_ready(RID p_rid);
};

} // namespace godot
