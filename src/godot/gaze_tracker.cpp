#include "gaze_tracker.hpp"
#include "gaze_server.hpp"

namespace godot {

GazeTracker::GazeTracker() {
}

GazeTracker::~GazeTracker() {
    if (is_tracking) {
        GazeServer *gs = GazeServer::get_singleton();
        if (gs) {
            gs->stop_tracking();
        }
        is_tracking = false;
    }
}

void GazeTracker::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &GazeTracker::set_enabled);
    ClassDB::bind_method(D_METHOD("is_enabled"), &GazeTracker::is_enabled);
    ClassDB::bind_method(D_METHOD("is_tracking_active"), &GazeTracker::is_tracking_active);
    ClassDB::bind_static_method("GazeTracker", D_METHOD("track_node", "node"), &GazeTracker::track_node);

    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
}

void GazeTracker::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            if (Engine::get_singleton() && Engine::get_singleton()->is_editor_hint()) {
                break;
            }
            if (enabled && !is_tracking) {
                GazeServer *gs = GazeServer::get_singleton();
                if (gs) {
                    gs->start_tracking();
                    is_tracking = true;
                }
            }
        } break;

        case NOTIFICATION_EXIT_TREE: {
            if (is_tracking) {
                GazeServer *gs = GazeServer::get_singleton();
                if (gs) {
                    gs->stop_tracking();
                }
                is_tracking = false;
            }
        } break;
    }
}

void GazeTracker::set_enabled(bool p_enabled) {
    if (enabled == p_enabled) {
        return;
    }
    enabled = p_enabled;

    if (Engine::get_singleton() && Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    if (is_inside_tree()) {
        GazeServer *gs = GazeServer::get_singleton();
        if (enabled && !is_tracking) {
            if (gs) {
                gs->start_tracking();
                is_tracking = true;
            }
        } else if (!enabled && is_tracking) {
            if (gs) {
                gs->stop_tracking();
                is_tracking = false;
            }
        }
    }
}

bool GazeTracker::is_enabled() const {
    return enabled;
}

bool GazeTracker::is_tracking_active() const {
    return is_tracking;
}

GazeTracker* GazeTracker::track_node(Node *p_node) {
    if (!p_node) {
        return nullptr;
    }
    // Check if an internal GazeTracker child already exists on this node
    TypedArray<Node> internal_children = p_node->get_children(true);
    for (int i = 0; i < internal_children.size(); ++i) {
        GazeTracker *existing = Object::cast_to<GazeTracker>(internal_children[i]);
        if (existing) {
            return existing;
        }
    }
    
    GazeTracker *tracker = memnew(GazeTracker);
    tracker->set_name("__GazeTrackerGuard__");
    p_node->add_child(tracker, false, Node::INTERNAL_MODE_FRONT);
    return tracker;
}

} // namespace godot
