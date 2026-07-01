/**
 * @file godot_files.hpp
 * @brief Shared file loading utilities for Godot classes
 */
#pragma once

#include <vector>
#include <cstring>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include "log.hpp"

namespace godot {

/**
 * @brief Resolves a model path by searching configurable search paths.
 */
inline String resolve_model_path(const String &p_path) {
    if (p_path.is_empty()) {
        return "";
    }

    ProjectSettings *ps = ProjectSettings::get_singleton();
    PackedStringArray suffixes;
    if (ps && ps->has_setting("gaze/models/acceptable_suffixes")) {
        suffixes = ps->get_setting("gaze/models/acceptable_suffixes");
    } else {
        suffixes.push_back(".ort");
        suffixes.push_back(".xml");
    }

    // Get the base prefix by stripping any trailing extension if present.
    String prefix = p_path.get_basename();
    String filename_prefix = prefix.get_file();

    // 1. Direct path checking with suffixes (for res:// or user:// absolute paths)
    if (p_path.begins_with("res://") || p_path.begins_with("user://")) {
        for (int j = 0; j < suffixes.size(); j++) {
            String candidate = prefix + suffixes[j];
            if (FileAccess::file_exists(candidate)) {
                return candidate;
            }
        }
        if (FileAccess::file_exists(p_path)) {
            return p_path;
        }
    }

    // 2. Check sequentially in search paths
    String search_paths_str = "res://models,res://addons/godot-gaze/models";
    if (ps && ps->has_setting("gaze/models/search_paths")) {
        search_paths_str = ps->get_setting("gaze/models/search_paths");
    }

    PackedStringArray paths = search_paths_str.split(",");
    for (int i = 0; i < paths.size(); i++) {
        String dir = paths[i].strip_edges();
        if (dir.is_empty()) continue;
        if (!dir.ends_with("/")) {
            dir += "/";
        }
        for (int j = 0; j < suffixes.size(); j++) {
            String candidate = dir + filename_prefix + suffixes[j];
            if (FileAccess::file_exists(candidate)) {
                return candidate;
            }
        }
        String exact_candidate = dir + p_path.get_file();
        if (FileAccess::file_exists(exact_candidate)) {
            return exact_candidate;
        }
    }

    // 3. Direct prefix and path fallbacks
    for (int j = 0; j < suffixes.size(); j++) {
        String candidate = prefix + suffixes[j];
        if (FileAccess::file_exists(candidate)) {
            return candidate;
        }
    }
    if (FileAccess::file_exists(p_path)) {
        return p_path;
    }

    return p_path;
}

/**
 * @brief Reads a file at path into a C++ std::vector<uint8_t> buffer using Godot's FileAccess API.
 */
inline std::vector<uint8_t> load_file_buffer(const String &path) {
    std::vector<uint8_t> buffer;
    if (path.is_empty()) {
        return buffer;
    }
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (file.is_null()) {
        Gaze::log_error("LoadFileBufferFailed_Open", "path", path.utf8().get_data());
        return buffer;
    }
    uint64_t length = file->get_length();
    if (length == 0) {
        Gaze::log_error("LoadFileBufferFailed_Empty", "path", path.utf8().get_data());
        return buffer;
    }
    PackedByteArray godot_buffer = file->get_buffer(length);
    buffer.resize(length);
    std::memcpy(buffer.data(), godot_buffer.ptr(), length);
    return buffer;
}

} // namespace godot
