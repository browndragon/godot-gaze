/**
 * @file gaze_profile.cpp
 * @brief Implement GazeProfile base resource class
 */
#include "gaze_profile.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>

namespace godot {

void GazeProfile::_bind_methods() {
    ClassDB::bind_method(D_METHOD("save_to_file", "path"), &GazeProfile::save_to_file);
    ClassDB::bind_method(D_METHOD("load_from_file", "path"), &GazeProfile::load_from_file);
}

Error GazeProfile::save_to_file(const String &p_path) {
    if (p_path.is_empty()) {
        return ERR_INVALID_PARAMETER;
    }
    String base_dir = p_path.get_base_dir();
    if (!base_dir.is_empty() && !DirAccess::dir_exists_absolute(base_dir)) {
        Error err = DirAccess::make_dir_recursive_absolute(base_dir);
        if (err != OK) {
            return err;
        }
    }
    Ref<ConfigFile> cfg;
    cfg.instantiate();
    if (FileAccess::file_exists(p_path)) {
        cfg->load(p_path);
    }
    _write_to_config(cfg);
    return cfg->save(p_path);
}

Error GazeProfile::load_from_file(const String &p_path) {
    if (p_path.is_empty() || !FileAccess::file_exists(p_path)) {
        return ERR_FILE_NOT_FOUND;
    }
    Ref<ConfigFile> cfg;
    cfg.instantiate();
    Error err = cfg->load(p_path);
    if (err != OK) {
        return err;
    }
    return _read_from_config(cfg);
}

} // namespace godot
