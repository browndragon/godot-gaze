extends "res://addons/gut/test.gd"

func before_each():
	var gs = Engine.get_singleton("GazeServer")
	if gs:
		while gs.get_active_tracker_count() > 0:
			gs.stop_tracking(true)

func after_each():
	var gs = Engine.get_singleton("GazeServer")
	if gs:
		while gs.get_active_tracker_count() > 0:
			gs.stop_tracking(true)

func test_gaze_tracker_node_tree_lifecycle():
	var gs = Engine.get_singleton("GazeServer")
	assert_not_null(gs, "GazeServer singleton must exist")
	assert_eq(gs.get_active_tracker_count(), 0, "Initial tracker count must be zero")

	var tracker = GazeTracker.new()
	assert_true(tracker.is_enabled(), "GazeTracker enabled by default")
	assert_false(tracker.is_tracking_active(), "GazeTracker inactive before entering tree")
	assert_eq(gs.get_active_tracker_count(), 0, "Server tracker count unchanged before tree entry")

	# Add to tree -> enters tree -> starts tracking
	add_child(tracker)
	assert_true(tracker.is_tracking_active(), "GazeTracker active after entering tree")
	assert_eq(gs.get_active_tracker_count(), 1, "Server tracker count incremented to 1")

	# Dynamic toggle: disable while in tree
	tracker.set_enabled(false)
	assert_false(tracker.is_enabled(), "GazeTracker enabled property is false")
	assert_false(tracker.is_tracking_active(), "GazeTracker inactive after set_enabled(false)")
	assert_eq(gs.get_active_tracker_count(), 0, "Server tracker count decremented to 0")

	# Dynamic toggle: re-enable while in tree
	tracker.set_enabled(true)
	assert_true(tracker.is_enabled(), "GazeTracker enabled property is true")
	assert_true(tracker.is_tracking_active(), "GazeTracker active after set_enabled(true)")
	assert_eq(gs.get_active_tracker_count(), 1, "Server tracker count incremented to 1")

	# Remove from tree -> exits tree -> stops tracking
	remove_child(tracker)
	assert_false(tracker.is_tracking_active(), "GazeTracker inactive after exiting tree")
	assert_eq(gs.get_active_tracker_count(), 0, "Server tracker count decremented to 0")

	tracker.free()

func test_gaze_tracker_multiple_instances():
	var gs = Engine.get_singleton("GazeServer")
	assert_not_null(gs, "GazeServer singleton must exist")
	assert_eq(gs.get_active_tracker_count(), 0, "Initial tracker count must be zero")

	var tracker1 = GazeTracker.new()
	var tracker2 = GazeTracker.new()

	add_child(tracker1)
	assert_eq(gs.get_active_tracker_count(), 1, "1 active tracker in server")

	add_child(tracker2)
	assert_eq(gs.get_active_tracker_count(), 2, "2 active trackers in server")

	# Remove tracker1 -> tracker2 keeps server active
	remove_child(tracker1)
	assert_false(tracker1.is_tracking_active(), "tracker1 inactive after exit")
	assert_true(tracker2.is_tracking_active(), "tracker2 remains active")
	assert_eq(gs.get_active_tracker_count(), 1, "1 active tracker remains in server")

	# Remove tracker2 -> server tracker count drops to 0
	remove_child(tracker2)
	assert_false(tracker2.is_tracking_active(), "tracker2 inactive after exit")
	assert_eq(gs.get_active_tracker_count(), 0, "Server tracker count drops to 0")

	tracker1.free()
	tracker2.free()

func test_gaze_tracker_track_node_static_helper():
	var gs = Engine.get_singleton("GazeServer")
	assert_not_null(gs, "GazeServer singleton must exist")
	assert_eq(gs.get_active_tracker_count(), 0, "Initial tracker count must be zero")

	var parent_node = Node.new()
	var guard = GazeTracker.track_node(parent_node)
	assert_not_null(guard, "track_node returns GazeTracker instance")
	assert_true(guard is GazeTracker, "guard is a GazeTracker instance")

	# Idempotency check: duplicate calls return same guard
	var guard2 = GazeTracker.track_node(parent_node)
	assert_eq(guard2, guard, "track_node is idempotent on same parent node")

	# Parent enters tree -> internal child enters tree -> starts tracking
	add_child(parent_node)
	assert_true(guard.is_tracking_active(), "Internal guard active when parent enters tree")
	assert_eq(gs.get_active_tracker_count(), 1, "Server tracker count is 1")

	# Parent leaves tree -> internal child leaves tree -> stops tracking
	remove_child(parent_node)
	assert_false(guard.is_tracking_active(), "Internal guard inactive when parent leaves tree")
	assert_eq(gs.get_active_tracker_count(), 0, "Server tracker count is 0")

	parent_node.free()
