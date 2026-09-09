# Gaze Tracking Lifecycle & Reference-Counted Architecture

This document explains how tracking lifecycle is managed in `godot-gaze`, the reference-counted tracker model in `GazeServer`, and how downstream consumer projects should integrate `GazeTracker` nodes or `GazeTracker.track_node()`.

---

## 1. Overview: The Reference-Counted Lifecycle Model

`godot-gaze` uses an active reference-counter (`active_trackers`) on the engine singleton `GazeServer`:

```
active_trackers == 0 : Camera pipeline & ML worker threads are STOPPED (hardware LED off).
active_trackers >= 1 : Camera pipeline & ML worker threads are RUNNING (hardware LED on).
```

### Why Reference Counting?
1. **Privacy & Battery**: Webcams and heavy neural inference models (face detection, facial landmarks, gaze vector regression, and eye openness) should only run when an active scene or gameplay mechanic requires gaze.
2. **Web / Browser Compliance**: Modern browsers require user interaction before granting camera permissions. Starting tracking prematurely on engine boot triggers immediate permission dialogs before the user interacts with the canvas.
3. **Headless Unit Testing**: Fast unit tests (e.g. testing math, event hierarchies, or UI layouts) do not need to query or lock physical camera devices.

---

## 2. Integration Patterns

### Pattern A: Declarative Scene Node (`GazeTracker`)

Add a `GazeTracker` node as a child of any scene or component that relies on gaze input:

```
TutorialLevel (Node2D)
├── GazeTracker (Node)
├── Paddle (CharacterBody2D)
└── Ball (RigidBody2D)
```

#### Invariants & Behavior
* **Tree Entry (`NOTIFICATION_ENTER_TREE`)**: Increments `GazeServer.active_trackers`. If transitioning from `0 -> 1`, starts the camera capture feed and ML worker thread.
* **Tree Exit (`NOTIFICATION_EXIT_TREE`)**: Decrements `GazeServer.active_trackers`. If dropping to `0`, stops the camera capture feed and ML worker thread.
* **Dynamic Property (`enabled`)**:
  * Toggling `enabled = false` while in tree releases its reference count.
  * Toggling `enabled = true` while in tree acquires its reference count.
* **Multiple Inclusion Safety**: If multiple subcomponents in a scene each contain a `GazeTracker` node (e.g. both a `Paddle` component and a `FluidField` visualizer), the reference count safely increments to 2. When one component is freed, tracking remains active for the other until all trackers exit the tree.

---

### Pattern B: Programmatic GDScript Attachment (`GazeTracker.track_node`)

For custom GDScript scenes or standalone root nodes that don't want to manually edit `.tscn` hierarchies:

```gdscript
extends Control

func _init() -> void:
    # Attaches an internal GazeTracker child that mirrors this node's tree lifecycle
    GazeTracker.track_node(self)

func _input(event: InputEvent) -> void:
    if event is InputEventGaze:
        # Handles gaze position, eye openness, and blink triggers
        pass
```

#### How `track_node()` Works
* Inserts an internal `GazeTracker` child using `Node.INTERNAL_MODE_FRONT`.
* Because it is an internal child:
  * It is completely hidden from `get_children()`.
  * It does not clutter the Godot editor inspector or scene dock.
  * It is not serialized into `.tscn` file saves.
* When the parent node is added to the scene tree via `add_child()`, the internal tracker enters the tree and starts tracking. When the parent leaves the tree or is freed (`queue_free()`), the internal tracker exits the tree and releases the reference count.
* Idempotent: Calling `GazeTracker.track_node(self)` multiple times on the same node is safely deduplicated.

---

## 3. Direct Server APIs (Advanced)

For specialized tools, calibration wizards, or diagnostics that need manual control outside of scene trees:

```gdscript
var gs = Engine.get_singleton("GazeServer")

# Manually start tracking (increments active_trackers)
gs.start_tracking()

# Query tracking state
var is_active = gs.is_tracking_active()
var tracker_count = gs.get_active_tracker_count()

# Manually stop tracking (decrements active_trackers)
gs.stop_tracking()
```
