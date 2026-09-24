#include "register_types.hpp"
#include "input_event_gaze.hpp"
#include "gaze_profile.hpp"
#include "gaze_device_profile.hpp"
#include "gaze_pipeline_config.hpp"
#include "smoother.hpp"
#include "one_euro_smoother.hpp"
#include "vision_server.hpp"
#include "gaze_server.hpp"
#include "gaze_frame.hpp"
#include "gaze_event_factory.hpp"
#include "gaze_tracker.hpp"
#include "fill_accumulator_node.hpp"
#include "gaze_display_server.hpp"
#include "mock_gaze_display_server.hpp"




#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#ifdef WEB_ENABLED
#include <emscripten.h>
#include "../web/web_binding_state.hpp"
#endif
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include "log.hpp"
#include <cstring>

namespace Gaze {
bool g_is_exiting = false;
}

namespace godot {

static bool singletons_initialized = false;
static VisionServer* vision_server_singleton = nullptr;
static GazeServer* gaze_server_singleton = nullptr;
static GazeDisplayServer* gaze_display_server_singleton = nullptr;

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
            ps->set_setting("gaze/pointing/emulate_mouse_from_gaze", true);
        }
        Dictionary prop_emul_mouse;
        prop_emul_mouse["name"] = "gaze/pointing/emulate_mouse_from_gaze";
        prop_emul_mouse["type"] = Variant::BOOL;
        ps->add_property_info(prop_emul_mouse);
        ps->set_initial_value("gaze/pointing/emulate_mouse_from_gaze", true);

        if (!ps->has_setting("gaze/pointing/default_clamping")) {
            ps->set_setting("gaze/pointing/default_clamping", true);
        }
        Dictionary prop_def_clamp;
        prop_def_clamp["name"] = "gaze/pointing/default_clamping";
        prop_def_clamp["type"] = Variant::BOOL;
        ps->add_property_info(prop_def_clamp);
        ps->set_initial_value("gaze/pointing/default_clamping", true);

        if (!ps->has_setting("gaze/pointing/mouse_stillness_duration_sec")) {
            ps->set_setting("gaze/pointing/mouse_stillness_duration_sec", 1.5);
        }
        Dictionary prop_stillness;
        prop_stillness["name"] = "gaze/pointing/mouse_stillness_duration_sec";
        prop_stillness["type"] = Variant::FLOAT;
        ps->add_property_info(prop_stillness);
        ps->set_initial_value("gaze/pointing/mouse_stillness_duration_sec", 1.5);

        if (!ps->has_setting("gaze/pointing/mouse_stillness_threshold_px")) {
            ps->set_setting("gaze/pointing/mouse_stillness_threshold_px", 3.0);
        }
        Dictionary prop_still_thresh;
        prop_still_thresh["name"] = "gaze/pointing/mouse_stillness_threshold_px";
        prop_still_thresh["type"] = Variant::FLOAT;
        ps->add_property_info(prop_still_thresh);
        ps->set_initial_value("gaze/pointing/mouse_stillness_threshold_px", 3.0);

        if (!ps->has_setting("gaze/pointing/mouse_emulation_dwell_sec")) {
            ps->set_setting("gaze/pointing/mouse_emulation_dwell_sec", 1.5);
        }
        Dictionary prop_dwell;
        prop_dwell["name"] = "gaze/pointing/mouse_emulation_dwell_sec";
        prop_dwell["type"] = Variant::FLOAT;
        ps->add_property_info(prop_dwell);
        ps->set_initial_value("gaze/pointing/mouse_emulation_dwell_sec", 1.5);


        if (!ps->has_setting("gaze/pointing/mouse_emulation_transition_sec")) {
            ps->set_setting("gaze/pointing/mouse_emulation_transition_sec", 0.3);
        }
        Dictionary prop_trans;
        prop_trans["name"] = "gaze/pointing/mouse_emulation_transition_sec";
        prop_trans["type"] = Variant::FLOAT;
        ps->add_property_info(prop_trans);
        ps->set_initial_value("gaze/pointing/mouse_emulation_transition_sec", 0.3);

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

        if (!ps->has_setting("gaze/calibration/device_profile_path")) {
            ps->set_setting("gaze/calibration/device_profile_path", "user://calibrations/device_profile.cfg");
        }
        Dictionary prop_device;
        prop_device["name"] = "gaze/calibration/device_profile_path";
        prop_device["type"] = Variant::STRING;
        ps->add_property_info(prop_device);
        ps->set_initial_value("gaze/calibration/device_profile_path", "user://calibrations/device_profile.cfg");

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

        if (!ps->has_setting("gaze/vision/driver")) {
            ps->set_setting("gaze/vision/driver", "native");
        }
        Dictionary prop_driver;
        prop_driver["name"] = "gaze/vision/driver";
        prop_driver["type"] = Variant::STRING;
        ps->add_property_info(prop_driver);
        ps->set_initial_value("gaze/vision/driver", "native");

        int verbosity = ps->get_setting("gaze/debug/verbosity");
        Gaze::set_log_verbosity(verbosity);
    }
}

void setup_gaze_singletons() {
    if (singletons_initialized) {
        return;
    }
    singletons_initialized = true;

    bool is_headless = false;
    if (Engine::get_singleton()->has_singleton("DisplayServer")) {
        DisplayServer *ds = DisplayServer::get_singleton();
        if (ds && ds->get_name() == "headless") {
            is_headless = true;
        }
    }

    bool use_mock_display = false;
    bool use_mock_vision = is_headless;
    bool explicit_native = false;
    bool explicit_mock = false;

    // Parse custom command line flags and environment variables
    OS *os = OS::get_singleton();
    if (os) {
        PackedStringArray args = os->get_cmdline_args();
        for (int i = 0; i < args.size(); ++i) {
            if (args[i] == "--mock-gaze-display") {
                use_mock_display = true;
            } else if (args[i] == "--mock-camera" || args[i] == "--mock-vision" || args[i] == "--mock-vision-server") {
                explicit_mock = true;
            } else if (args[i] == "--native-camera" || args[i] == "--real-camera") {
                explicit_native = true;
            }
        }

        if (os->has_environment("GAZE_VISION_DRIVER")) {
            String env_driver = os->get_environment("GAZE_VISION_DRIVER");
            if (env_driver == "mock") {
                explicit_mock = true;
            } else if (env_driver == "native") {
                explicit_native = true;
            }
        } else if (os->has_environment("GODOT_GAZE_VISION_DRIVER")) {
            String env_driver = os->get_environment("GODOT_GAZE_VISION_DRIVER");
            if (env_driver == "mock") {
                explicit_mock = true;
            } else if (env_driver == "native") {
                explicit_native = true;
            }
        }
    }

    ProjectSettings *ps = ProjectSettings::get_singleton();
    if (ps && ps->has_setting("gaze/vision/driver")) {
        String driver = ps->get_setting("gaze/vision/driver");
        if (driver == "mock") {
            use_mock_vision = true;
        } else if (driver == "native" && !is_headless) {
            use_mock_vision = false;
        }
    }

    if (explicit_mock) {
        use_mock_vision = true;
    }
    if (explicit_native) {
        use_mock_vision = false;
    }

    if (use_mock_display) {
        gaze_display_server_singleton = memnew(MockGazeDisplayServer);
    } else {
        gaze_display_server_singleton = memnew(GazeDisplayServer);
    }
    Engine::get_singleton()->register_singleton("GazeDisplayServer", gaze_display_server_singleton);

    if (use_mock_vision) {
        vision_server_singleton = memnew(MockVisionServer);
        Gaze::log_info("VisionServer_Init_Strategy", "type", "MockVisionServer");
    } else {
        vision_server_singleton = memnew(VisionServer);
        Gaze::log_info("VisionServer_Init_Strategy", "type", "VisionServer");
    }
    Engine::get_singleton()->register_singleton("VisionServer", vision_server_singleton);
    Engine::get_singleton()->register_singleton("GazeVisionServer", vision_server_singleton);

    gaze_server_singleton = memnew(GazeServer);
    Engine::get_singleton()->register_singleton("GazeServer", gaze_server_singleton);
}

static void on_startup_callback() {
    if (!singletons_initialized) {
        setup_gaze_singletons();
        return;
    }
    if (Engine::get_singleton()->has_singleton("DisplayServer")) {
        DisplayServer *ds = DisplayServer::get_singleton();
        if (ds && ds->get_name() == "headless") {
            if (vision_server_singleton && vision_server_singleton->get_class() != "MockVisionServer") {
                bool explicit_native = false;
                OS *os = OS::get_singleton();
                if (os) {
                    PackedStringArray args = os->get_cmdline_args();
                    for (int i = 0; i < args.size(); ++i) {
                        if (args[i] == "--native-camera" || args[i] == "--real-camera") {
                            explicit_native = true;
                            break;
                        }
                    }
                    if (os->has_environment("GAZE_VISION_DRIVER") && os->get_environment("GAZE_VISION_DRIVER") == "native") {
                        explicit_native = true;
                    }
                }
                if (!explicit_native) {
                    Gaze::log_info("VisionServer_Init_Strategy", "type", "MockVisionServer (dynamic headless fallback)");
                    Engine::get_singleton()->unregister_singleton("VisionServer");
                    Engine::get_singleton()->unregister_singleton("GazeVisionServer");
                    memdelete(vision_server_singleton);
                    vision_server_singleton = memnew(MockVisionServer);
                    Engine::get_singleton()->register_singleton("VisionServer", vision_server_singleton);
                    Engine::get_singleton()->register_singleton("GazeVisionServer", vision_server_singleton);
                }
            }
        }
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
        ClassDB::register_class<InputEventGazeBase>();
        ClassDB::register_class<InputEventGaze>();
        ClassDB::register_class<InputEventGazeMissing>();

        ClassDB::register_class<GazeEventFactory>();
        ClassDB::register_class<GazeServerEventFactory>();

        ClassDB::register_class<Smoother>();
        ClassDB::register_class<OneEuroSmoother>();
        ClassDB::register_internal_class<OneEuroFilterState>();

        ClassDB::register_class<GazeProfile>();
        ClassDB::register_class<GazeDeviceProfile>();
        ClassDB::register_class<GazePipelineConfig>();

        ClassDB::register_class<VisionServer>();
        ClassDB::register_class<MockVisionServer>();
        ClassDB::register_class<GazeDisplayServer>();
        ClassDB::register_class<MockGazeDisplayServer>();
        ClassDB::register_class<GazeFrame>();
        ClassDB::register_class<GazeServer>();

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
    ClassDB::register_class<GazeTracker>();
    ClassDB::register_class<FillAccumulator>();
#ifdef WEB_ENABLED
    ClassDB::register_class<WebBindingState>();
#endif

    setup_gaze_singletons();

    ProjectSettings *ps = ProjectSettings::get_singleton();
    // On Web, if run-tests=true is passed via URL search parameters, override the boot scene dynamically
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
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        Gaze::log_info("uninitialize_gaze_module_level_scene_began");
        Gaze::g_is_exiting = true;

        Input *input = Input::get_singleton();
        if (input) {
            input->flush_buffered_events();
        }

        if (GazeServer::get_singleton()) {
            Engine::get_singleton()->unregister_singleton("GazeServer");
            memdelete(GazeServer::get_singleton());
            gaze_server_singleton = nullptr;
        }

        if (VisionServer::get_singleton()) {
            Engine::get_singleton()->unregister_singleton("VisionServer");
            Engine::get_singleton()->unregister_singleton("GazeVisionServer");
            memdelete(VisionServer::get_singleton());
            vision_server_singleton = nullptr;
        }

        if (GazeDisplayServer::get_singleton()) {
            Engine::get_singleton()->unregister_singleton("GazeDisplayServer");
            memdelete(GazeDisplayServer::get_singleton());
            gaze_display_server_singleton = nullptr;
        }
        singletons_initialized = false;
        Gaze::log_info("uninitialize_gaze_module_level_scene_finished");
        return;
    }

    if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
        Gaze::log_info("uninitialize_gaze_module_level_servers_began");
        // Clean up registry
        Gaze::register_log_handler(nullptr);
        Gaze::log_info("uninitialize_gaze_module_level_servers_finished");
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
#if GODOT_VERSION_MINOR >= 5
    init_obj.register_startup_callback(on_startup_callback);
#endif

    return init_obj.init();
}
}

} // namespace godot

