#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>

namespace godot {

/**
 * GazeTracker
 *
 * Declarative Scene Node for managing GazeServer tracking lifecycle.
 *
 * Attaching this node to a scene tree increments GazeServer's reference counter
 * on tree entry and decrements it on tree exit. Multiple GazeTracker nodes can
 * safely coexist in the scene hierarchy; tracking is active as long as at least
 * one tracker is in the active scene tree.
 *
 * It also provides the static helper `GazeTracker.track_node(node)` for programmatic
 * lifecycle attachment from custom GDScript scripts.
 */
class GazeTracker : public Node {
    GDCLASS(GazeTracker, Node);

private:
    bool enabled = true;
    bool is_tracking = false;

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    GazeTracker();
    virtual ~GazeTracker();

    /**
     * Toggles tracker activation.
     * If disabled while inside the scene tree, releases its tracking reference count.
     * If re-enabled while inside the scene tree, acquires a tracking reference count.
     */
    void set_enabled(bool p_enabled);
    bool is_enabled() const;

    /**
     * Returns true if this specific GazeTracker instance is currently holding
     * an active reference count in GazeServer.
     */
    bool is_tracking_active() const;

    /**
     * Programmatic helper to attach an internal GazeTracker child to an arbitrary node.
     *
     * In GDScript:
     *   func _init() -> void:
     *       GazeTracker.track_node(self)
     *
     * Uses Node::INTERNAL_MODE_FRONT so the child node is invisible to get_children(),
     * not shown in the editor scene tree, and excluded from scene serialization.
     */
    static GazeTracker* track_node(Node *p_node);
};

} // namespace godot
