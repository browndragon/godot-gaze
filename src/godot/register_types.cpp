#include "register_types.hpp"
#include "gaze_tracker.hpp"
#include "gaze_calibration_resource.hpp"
#include "gaze_calibration_session.hpp"
#include "gaze_pipeline_config.hpp"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include "log.hpp"

namespace godot {

static GazeDeviceEstimatedCalibration* default_calib = nullptr;

void initialize_gaze_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    // Register GDExtension classes so they are exposed to GDScript/Editor
    ClassDB::register_class<GazeCalibration>();
    ClassDB::register_class<GazeDeviceCalibration>();
    ClassDB::register_class<GazeCalibrationSession>();
    ClassDB::register_class<GazePipelineConfig>();
    ClassDB::register_class<GazeTracker>();
    ClassDB::register_class<GazeDeviceEstimatedCalibration>();

    // Register GazeDeviceEstimatedCalibration engine singleton
    default_calib = memnew(GazeDeviceEstimatedCalibration);
    Engine::get_singleton()->register_singleton("GazeDeviceEstimatedCalibration", default_calib);

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

    if (default_calib) {
        Engine::get_singleton()->unregister_singleton("GazeDeviceEstimatedCalibration");
        memdelete(default_calib);
        default_calib = nullptr;
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
