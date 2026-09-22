/**
 * @file register_types.hpp
 * @brief GDExtension module registration hooks
 *
 * Declares the initialization and teardown entry points for registering the
 * GazeServer and InputEventGaze classes into the Godot
 * engine ClassDB.
 */
#pragma once

#include <godot_cpp/core/class_db.hpp>

namespace godot {

void initialize_gaze_module(ModuleInitializationLevel p_level);
void uninitialize_gaze_module(ModuleInitializationLevel p_level);
void setup_gaze_singletons();

} // namespace godot
