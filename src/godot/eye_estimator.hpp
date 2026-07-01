#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/rid.hpp>

namespace godot {

class GazeFrame;


class EyeEstimator : public Node3D {
    GDCLASS(EyeEstimator, Node3D);

private:
    String gaze_model_prefix;
    Ref<Image> left_eye_crop;
    Ref<Image> right_eye_crop;
    RID eye_rid;

protected:
    static void _bind_methods();

public:
    EyeEstimator();
    virtual ~EyeEstimator();

    bool initialize_estimator();
    void stop_estimator();

    void set_gaze_model_prefix(String path);
    String get_gaze_model_prefix() const;

    Ref<Image> get_left_eye_crop() const;
    Ref<Image> get_right_eye_crop() const;

    RID get_eye_rid() const { return eye_rid; }
    void set_eye_rid(RID p_rid) { eye_rid = p_rid; }

    void _on_gaze_data_ready(RID p_rid);
    void _on_gaze_frame_began(GazeFrame* frame);
};


} // namespace godot
