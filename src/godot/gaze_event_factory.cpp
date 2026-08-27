#include "gaze_event_factory.hpp"
#include "gaze_server.hpp"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

// --- GazeEventFactory ---

void GazeEventFactory::_bind_methods() {
    ClassDB::bind_method(D_METHOD("create_gaze_event"), &GazeEventFactory::create_gaze_event);
    ClassDB::bind_method(D_METHOD("create_missing_event", "reason"), &GazeEventFactory::create_missing_event);
}

Ref<InputEventGazeBase> GazeEventFactory::create_gaze_event() {
    Ref<InputEventGaze> evt;
    evt.instantiate();
    return evt;
}

Ref<InputEventGazeBase> GazeEventFactory::create_missing_event(int p_reason) {
    Ref<InputEventGazeMissing> evt;
    evt.instantiate();
    evt->set_reason(static_cast<InputEventGazeMissing::MissingReason>(p_reason));
    return evt;
}

// --- GazeServerEventFactory ---

void GazeServerEventFactory::_bind_methods() {
}

Ref<InputEventGazeBase> GazeServerEventFactory::create_gaze_event() {
    GazeServer *gs = GazeServer::get_singleton();
    if (gs) {
        return gs->create_default_event();
    }
    Ref<InputEventGaze> evt;
    evt.instantiate();
    return evt;
}

Ref<InputEventGazeBase> GazeServerEventFactory::create_missing_event(int p_reason) {
    GazeServer *gs = GazeServer::get_singleton();
    if (gs) {
        return gs->create_default_missing_event(p_reason);
    }
    Ref<InputEventGazeMissing> evt;
    evt.instantiate();
    evt->set_reason(static_cast<InputEventGazeMissing::MissingReason>(p_reason));
    return evt;
}

} // namespace godot
