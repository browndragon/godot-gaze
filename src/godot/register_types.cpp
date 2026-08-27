#include "register_types.hpp"
#include "input_event_gaze.hpp"
#include "gaze_calibration_resource.hpp"
#include "gaze_calibration_session.hpp"
#include "gaze_pipeline_config.hpp"
#include "camera_sensor.hpp"
#include "face_estimator.hpp"
#include "eye_estimator.hpp"
#include "smoother.hpp"
#include "one_euro_smoother.hpp"
#include "display_profile.hpp"
#include "vision_server.hpp"
#include "gaze_server.hpp"
#include "gaze_frame.hpp"
#include "gaze_event_factory.hpp"




#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#ifdef WEB_ENABLED
#include <emscripten.h>
#include "../web/web_binding_state.hpp"
#endif
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include "log.hpp"

namespace Gaze {
bool g_is_exiting = false;
}

namespace godot {

static GazeDeviceEstimatedCalibration* default_calib = nullptr;
static VisionServer* vision_server_singleton = nullptr;
static GazeServer* gaze_server_singleton = nullptr;

static void register_gaze_project_settings() {
    ProjectSettings *ps = ProjectSettings::get_singleton();
    if (ps) {
        if (!ps->has_setting("gaze/general/autostart")) {
            ps->set_setting("gaze/general/autostart", true);
        }
        Dictionary prop_autostart;
        prop_autostart["name"] = "gaze/general/autostart";
        prop_autostart["type"] = Variant::BOOL;
        ps->add_property_info(prop_autostart);
        ps->set_initial_value("gaze/general/autostart", true);

        if (!ps->has_setting("gaze/pointing/emulate_gaze_from_mouse")) {
            ps->set_setting("gaze/pointing/emulate_gaze_from_mouse", true);
        }
        Dictionary prop_emul_gaze;
        prop_emul_gaze["name"] = "gaze/pointing/emulate_gaze_from_mouse";
        prop_emul_gaze["type"] = Variant::BOOL;
        ps->add_property_info(prop_emul_gaze);
        ps->set_initial_value("gaze/pointing/emulate_gaze_from_mouse", true);

        if (!ps->has_setting("gaze/pointing/emulate_mouse_from_gaze")) {
            ps->set_setting("gaze/pointing/emulate_mouse_from_gaze", false);
        }
        Dictionary prop_emul_mouse;
        prop_emul_mouse["name"] = "gaze/pointing/emulate_mouse_from_gaze";
        prop_emul_mouse["type"] = Variant::BOOL;
        ps->add_property_info(prop_emul_mouse);
        ps->set_initial_value("gaze/pointing/emulate_mouse_from_gaze", false);

        if (!ps->has_setting("gaze/models/search_paths")) {
            ps->set_setting("gaze/models/search_paths", "res://models,res://addons/godot-gaze/models");
        }
        Dictionary prop_search_paths;
        prop_search_paths["name"] = "gaze/models/search_paths";
        prop_search_paths["type"] = Variant::STRING;
        ps->add_property_info(prop_search_paths);
        ps->set_initial_value("gaze/models/search_paths", "res://models,res://addons/godot-gaze/models");

        if (!ps->has_setting("gaze/models/yunet_prefix")) {
            ps->set_setting("gaze/models/yunet_prefix", "face_detection_yunet_2023mar");
        }
        Dictionary prop_yunet;
        prop_yunet["name"] = "gaze/models/yunet_prefix";
        prop_yunet["type"] = Variant::STRING;
        ps->add_property_info(prop_yunet);
        ps->set_initial_value("gaze/models/yunet_prefix", "face_detection_yunet_2023mar");

        if (!ps->has_setting("gaze/models/face_detector_prefix")) {
            ps->set_setting("gaze/models/face_detector_prefix", "face_detection_yunet_2023mar");
        }
        Dictionary prop_detector;
        prop_detector["name"] = "gaze/models/face_detector_prefix";
        prop_detector["type"] = Variant::STRING;
        ps->add_property_info(prop_detector);
        ps->set_initial_value("gaze/models/face_detector_prefix", "face_detection_yunet_2023mar");

        if (!ps->has_setting("gaze/models/eye_openness_prefix")) {
            ps->set_setting("gaze/models/eye_openness_prefix", "open_closed_eye");
        }
        Dictionary prop_openness;
        prop_openness["name"] = "gaze/models/eye_openness_prefix";
        prop_openness["type"] = Variant::STRING;
        ps->add_property_info(prop_openness);
        ps->set_initial_value("gaze/models/eye_openness_prefix", "open_closed_eye");

        if (!ps->has_setting("gaze/models/gaze_prefix")) {
            ps->set_setting("gaze/models/gaze_prefix", "gaze-estimation-adas-0002");
        }
        Dictionary prop_gaze;
        prop_gaze["name"] = "gaze/models/gaze_prefix";
        prop_gaze["type"] = Variant::STRING;
        ps->add_property_info(prop_gaze);
        ps->set_initial_value("gaze/models/gaze_prefix", "gaze-estimation-adas-0002");

        if (!ps->has_setting("gaze/models/landmarks_prefix")) {
            ps->set_setting("gaze/models/landmarks_prefix", "facial-landmarks-35-adas-0002");
        }
        Dictionary prop_landmarks;
        prop_landmarks["name"] = "gaze/models/landmarks_prefix";
        prop_landmarks["type"] = Variant::STRING;
        ps->add_property_info(prop_landmarks);
        ps->set_initial_value("gaze/models/landmarks_prefix", "facial-landmarks-35-adas-0002");

        if (!ps->has_setting("gaze/models/acceptable_suffixes")) {
            PackedStringArray suffixes;
            suffixes.push_back(".ort");
            suffixes.push_back(".xml");
            ps->set_setting("gaze/models/acceptable_suffixes", suffixes);
        }
        Dictionary prop_suffixes;
        prop_suffixes["name"] = "gaze/models/acceptable_suffixes";
        prop_suffixes["type"] = Variant::PACKED_STRING_ARRAY;
        ps->add_property_info(prop_suffixes);
        PackedStringArray suffixes_init;
        suffixes_init.push_back(".ort");
        suffixes_init.push_back(".xml");
        ps->set_initial_value("gaze/models/acceptable_suffixes", suffixes_init);

        // Gaze Pipeline configurations
        auto reg_double = [ps](const String& name, double val) {
            if (!ps->has_setting(name)) {
                ps->set_setting(name, val);
            }
            Dictionary prop;
            prop["name"] = name;
            prop["type"] = Variant::FLOAT;
            ps->add_property_info(prop);
            ps->set_initial_value(name, val);
        };
        auto reg_int = [ps](const String& name, int val) {
            if (!ps->has_setting(name)) {
                ps->set_setting(name, val);
            }
            Dictionary prop;
            prop["name"] = name;
            prop["type"] = Variant::INT;
            ps->add_property_info(prop);
            ps->set_initial_value(name, val);
        };

        reg_double("gaze/config/pitch_t_gain", 0.0);
        reg_double("gaze/config/yaw_t_gain", 0.0);
        reg_double("gaze/config/nose_y", -0.5);
        reg_double("gaze/config/nose_z", -52.0);
        reg_double("gaze/config/ipd_mm", 63.0);
 
        reg_int("gaze/config/debug_image_throttle_interval", 1);
 
        reg_int("gaze/config/desired_camera_width", 640);
        reg_int("gaze/config/desired_camera_height", 480);

        if (!ps->has_setting("gaze/config/force_cpu")) {
            ps->set_setting("gaze/config/force_cpu", false);
        }
        Dictionary prop_force_cpu;
        prop_force_cpu["name"] = "gaze/config/force_cpu";
        prop_force_cpu["type"] = Variant::BOOL;
        ps->add_property_info(prop_force_cpu);
        ps->set_initial_value("gaze/config/force_cpu", false);

        if (!ps->has_setting("gaze/calibration/device_calibration_path")) {
            ps->set_setting("gaze/calibration/device_calibration_path", "user://calibrations/device_calibration.tres");
        }
        Dictionary prop_device;
        prop_device["name"] = "gaze/calibration/device_calibration_path";
        prop_device["type"] = Variant::STRING;
        ps->add_property_info(prop_device);
        ps->set_initial_value("gaze/calibration/device_calibration_path", "user://calibrations/device_calibration.tres");

        if (!ps->has_setting("gaze/calibration/bio_calibration_path")) {
            ps->set_setting("gaze/calibration/bio_calibration_path", "user://calibrations/bio_calibration.tres");
        }
        Dictionary prop_bio;
        prop_bio["name"] = "gaze/calibration/bio_calibration_path";
        prop_bio["type"] = Variant::STRING;
        ps->add_property_info(prop_bio);
        ps->set_initial_value("gaze/calibration/bio_calibration_path", "user://calibrations/bio_calibration.tres");

        if (!ps->has_setting("gaze/debug/overlay_scene_path")) {
            ps->set_setting("gaze/debug/overlay_scene_path", "res://addons/godot-gaze/debug_cam_feed.tscn");
        }
        Dictionary prop_overlay;
        prop_overlay["name"] = "gaze/debug/overlay_scene_path";
        prop_overlay["type"] = Variant::STRING;
        ps->add_property_info(prop_overlay);
        ps->set_initial_value("gaze/debug/overlay_scene_path", "res://addons/godot-gaze/debug_cam_feed.tscn");

        reg_double("gaze/blink/openness_threshold", 0.50);
        if (!ps->has_setting("gaze/blink/allow_single_eye")) {
            ps->set_setting("gaze/blink/allow_single_eye", true);
        }
        Dictionary prop_allow_single;
        prop_allow_single["name"] = "gaze/blink/allow_single_eye";
        prop_allow_single["type"] = Variant::BOOL;
        ps->add_property_info(prop_allow_single);
        ps->set_initial_value("gaze/blink/allow_single_eye", true);

        reg_double("gaze/blink/min_duration", 0.05);

        if (!ps->has_setting("gaze/debug/verbosity")) {
            ps->set_setting("gaze/debug/verbosity", 1);
        }
        Dictionary prop_verbosity;
        prop_verbosity["name"] = "gaze/debug/verbosity";
        prop_verbosity["type"] = Variant::INT;
        ps->add_property_info(prop_verbosity);
        ps->set_initial_value("gaze/debug/verbosity", 1);

        if (!ps->has_setting("gaze/events/event_factory_path")) {
            ps->set_setting("gaze/events/event_factory_path", "");
        }
        Dictionary prop_factory_path;
        prop_factory_path["name"] = "gaze/events/event_factory_path";
        prop_factory_path["type"] = Variant::STRING;
        ps->add_property_info(prop_factory_path);
        ps->set_initial_value("gaze/events/event_factory_path", "");

        int verbosity = ps->get_setting("gaze/debug/verbosity");
        Gaze::set_log_verbosity(verbosity);
    }
}

void initialize_gaze_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
        // Redirect gaze library logging messages to Godot output console
        Gaze::register_log_handler([](bool is_error, const char* msg) {
            String godot_msg = String(msg);
            if (is_error) {
                UtilityFunctions::printerr(godot_msg);
            } else {
                UtilityFunctions::print(godot_msg);
            }
        });

        // Register all classes needed by GazeServer, VisionServer, and GDScript
        ClassDB::register_class<DisplayProfile>();

        ClassDB::register_class<InputEventGazeBase>();
        ClassDB::register_class<InputEventGaze>();
        ClassDB::register_class<InputEventGazeMissing>();

        ClassDB::register_class<GazeEventFactory>();
        ClassDB::register_class<GazeServerEventFactory>();

        ClassDB::register_class<Smoother>();
        ClassDB::register_class<OneEuroSmoother>();
        ClassDB::register_internal_class<OneEuroFilterState>();

        ClassDB::register_class<DeviceCalibration>();
        ClassDB::register_class<GuessDeviceCalibration>();
        ClassDB::register_class<StoredDeviceCalibration>();
        ClassDB::register_class<DefaultDeviceCalibration>();

        ClassDB::register_class<BioCalibration>();
        ClassDB::register_class<GuessBioCalibration>();
        ClassDB::register_class<StoredBioCalibration>();
        ClassDB::register_class<DefaultBioCalibration>();

        ClassDB::register_class<GazeCalibrationSession>();
        ClassDB::register_class<GazePipelineConfig>();
        ClassDB::register_class<GazeDeviceEstimatedCalibration>();

        ClassDB::register_class<VisionServer>();
        ClassDB::register_class<MockVisionServer>();
        ClassDB::register_class<GazeFrame>();
        ClassDB::register_class<GazeServer>();

        vision_server_singleton = memnew(VisionServer);
        Engine::get_singleton()->register_singleton("VisionServer", vision_server_singleton);

        gaze_server_singleton = memnew(GazeServer);
        Engine::get_singleton()->register_singleton("GazeServer", gaze_server_singleton);

        default_calib = memnew(GazeDeviceEstimatedCalibration);
        Engine::get_singleton()->register_singleton("GazeDeviceEstimatedCalibration", default_calib);

        register_gaze_project_settings();
        return;
    }

    if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        if (Engine::get_singleton()->is_editor_hint()) {
            EditorInterface* editor = EditorInterface::get_singleton();
            if (editor) {
                Variant help = editor->call("get_editor_help_plugin");
                if (help.get_type() == Variant::OBJECT) {
                    Object* help_obj = help;
                    if (help_obj) {
                        help_obj->call("add_doc_folder", "res://docs/classref");
                    }
                }
            }
        }
        return;
    }

    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    // Register Scene / Node classes
    ClassDB::register_class<CameraSensor>();
    ClassDB::register_class<FaceEstimator>();
    ClassDB::register_class<EyeEstimator>();
#ifdef WEB_ENABLED
    ClassDB::register_class<WebBindingState>();
#endif

    // On Web, if run-tests=true is passed via URL search parameters, override the boot scene dynamically
    ProjectSettings* ps = ProjectSettings::get_singleton();
    if (ps) {
        bool should_run_tests = false;
#ifdef WEB_ENABLED
        should_run_tests = emscripten_run_script_int("window.location.search.includes('run-tests=true') ? 1 : 0") != 0;
#endif
        if (should_run_tests) {
            Gaze::log_info("GodotGaze_E2ETestOverride", "msg", "E2E test run requested in URL, dynamically overriding main scene to headless_test.tscn...");
            ps->set_setting("application/run/main_scene", "res://addons/godot-gaze/tests/headless_test.tscn");
        }
    }
}

void uninitialize_gaze_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
        Gaze::log_info("uninitialize_gaze_module_level_servers_began");
        Gaze::g_is_exiting = true;

        Input *input = Input::get_singleton();
        if (input) {
            input->flush_buffered_events();
        }

        if (default_calib) {
            Engine::get_singleton()->unregister_singleton("GazeDeviceEstimatedCalibration");
            memdelete(default_calib);
            default_calib = nullptr;
        }

        if (GazeServer::get_singleton()) {
            Engine::get_singleton()->unregister_singleton("GazeServer");
            memdelete(GazeServer::get_singleton());
            gaze_server_singleton = nullptr;
        }

        if (VisionServer::get_singleton()) {
            Engine::get_singleton()->unregister_singleton("VisionServer");
            memdelete(VisionServer::get_singleton());
            vision_server_singleton = nullptr;
        }

        // Clean up registry
        Gaze::register_log_handler(nullptr);
        Gaze::log_info("uninitialize_gaze_module_level_servers_finished");
        return;
    }

    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
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
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SERVERS);

    return init_obj.init();
}
}

} // namespace godot

