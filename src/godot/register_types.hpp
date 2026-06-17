// src/godot/register_types.hpp
#pragma once

#include <godot_cpp/core/class_db.hpp>

namespace godot {

void initialize_gaze_module(ModuleInitializationLevel p_level);
void uninitialize_gaze_module(ModuleInitializationLevel p_level);

} // namespace godot
