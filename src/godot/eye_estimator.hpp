#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "gaze_model.hpp"

namespace godot {

class EyeEstimator : public Node3D {
    GDCLASS(EyeEstimator, Node3D);

private:
    String gaze_model_path;
    Ref<Image> left_eye_crop;
    Ref<Image> right_eye_crop;

    Gaze::GazeModel* model = nullptr;

protected:
    static void _bind_methods();

public:
    EyeEstimator();
    virtual ~EyeEstimator();

    bool initialize_estimator();
    void stop_estimator();
    bool opencv_gaze(const Gaze::EyeCrops& crops, Gaze::GazeVector3& out_gaze_dir);

    void set_gaze_model_path(String path);
    String get_gaze_model_path() const;

    void set_pipeline_config(const Gaze::PipelineConfig& config);

    Ref<Image> get_left_eye_crop() const;
    Ref<Image> get_right_eye_crop() const;

    void feed_gaze_web(Vector3 direction);
};

} // namespace godot
