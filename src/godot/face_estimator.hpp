#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "face_pipeline.hpp"

namespace godot {

class FaceEstimator : public Node3D {
    GDCLASS(FaceEstimator, Node3D);

private:
    String yunet_model_path;
    double camera_focal_length_px = 1000.0;
    bool has_detected_face_val = false;

    Gaze::FacePipeline* pipeline = nullptr;

protected:
    static void _bind_methods();

public:
    FaceEstimator();
    virtual ~FaceEstimator();

    bool initialize_estimator();
    void stop_estimator();
    bool process_frame(const Gaze::Frame& frame, Gaze::EyeCrops& out_crops);

    void set_yunet_model_path(String path);
    String get_yunet_model_path() const;

    void set_focal_length(double focal);
    double get_focal_length() const;

    void set_has_detected_face(bool detected);
    bool get_has_detected_face() const;

    void set_pipeline_config(const Gaze::PipelineConfig& config);

    void feed_pose_web(bool detected, Vector3 translation, Vector3 rotation);
};

} // namespace godot
