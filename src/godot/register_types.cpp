#include "register_types.hpp"
#include "gaze_tracker.hpp"
#include "gaze_calibration_resource.hpp"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "log.hpp"

namespace godot {

void initialize_gaze_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    // Register GDExtension classes so they are exposed to GDScript/Editor
    ClassDB::register_class<GazeCalibrationResource>();
    ClassDB::register_class<GazeTracker>();

    // Redirect gaze library logging messages to Godot output console
    Gaze::register_log_handler([](bool is_error, const char* msg) {
        String godot_msg = String(msg);
        if (is_error) {
            UtilityFunctions::printerr(godot_msg);
        } else {
            UtilityFunctions::print(godot_msg);
        }
    });
}

void uninitialize_gaze_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    // Clean up registry
    Gaze::register_log_handler(nullptr);
}

extern "C" {
// GDExtension entry point called by Godot when loading the dynamic library
GDExtensionBool GDE_EXPORT gaze_library_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address, 
    const GDExtensionClassLibraryPtr p_library, 
    GDExtensionInitialization *r_initialization) {
    
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_gaze_module);
    init_obj.register_terminator(uninitialize_gaze_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
}

} // namespace godot
