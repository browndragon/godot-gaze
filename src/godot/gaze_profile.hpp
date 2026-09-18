/**
 * @file gaze_profile.hpp
 * @brief Common base class for serializable Godot Gaze profiles (device and biological)
 */
#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class GazeProfile : public Resource {
    GDCLASS(GazeProfile, Resource);

protected:
    static void _bind_methods();

    virtual void _write_to_config(Ref<ConfigFile> &p_cfg) const {}
    virtual Error _read_from_config(const Ref<ConfigFile> &p_cfg) { return OK; }

public:
    GazeProfile() = default;
    virtual ~GazeProfile() = default;

    Error save_to_file(const String &p_path);
    Error load_from_file(const String &p_path);
};

} // namespace godot
