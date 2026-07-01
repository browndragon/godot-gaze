#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/rid.hpp>

namespace godot
{

    /**
     * @class CameraSensor
     * @brief Node3D managing camera capture, resolution settings, and focal length configurations.
     *
     * CameraSensor handles platform-specific camera inputs, fetches raw image frames, and manages
     * spatial properties (position/rotation) representing the camera's physical alignment on the display bezel.
     */
    class CameraSensor : public Node3D
    {
        GDCLASS(CameraSensor, Node3D);

    private:
        int camera_device_id = 0;
        int desired_width = 640;
        int desired_height = 480;
        double focal_length = -1.0;
        double camera_fov = 35.488537576579634;
        RID camera_rid;

    protected:
        static void _bind_methods();

    public:
        CameraSensor();
        virtual ~CameraSensor();

        bool initialize_sensor();
        void stop_sensor();
        Ref<Image> get_last_frame() const;

        void set_camera_device_id(int id);
        int get_camera_device_id() const;

        void set_resolution(int w, int h);

        void set_focal_length(double focal);
        double get_focal_length() const;

        void set_camera_fov(double fov);
        double get_camera_fov() const;

        RID get_camera_rid() const { return camera_rid; }

        void _on_gaze_data_ready(RID p_rid);
    };

} // namespace godot
