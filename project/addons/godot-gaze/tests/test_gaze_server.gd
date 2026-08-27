extends "res://addons/gut/test.gd"

func test_gaze_server_camera_preview_refcounting():
	var gs = Engine.get_singleton("GazeServer")
	assert_not_null(gs, "GazeServer singleton must exist")
	
	# Initial state: preview should be inactive (refcount 0)
	assert_false(gs.is_camera_preview_requested(), "Preview should be inactive initially")

	# First consumer requests preview
	gs.camera_set_preview_requested(true)
	assert_true(gs.is_camera_preview_requested(), "Preview should be active after 1st request")

	# Second consumer requests preview
	gs.camera_set_preview_requested(true)
	assert_true(gs.is_camera_preview_requested(), "Preview should stay active after 2nd request")

	# First consumer releases preview -> still active because 1 consumer remains
	gs.camera_set_preview_requested(false)
	assert_true(gs.is_camera_preview_requested(), "Preview should stay active after 1 release")

	# Second consumer releases preview -> inactive
	gs.camera_set_preview_requested(false)
	assert_false(gs.is_camera_preview_requested(), "Preview should be inactive after all consumers release")

func test_gaze_server_display_profile_injection():
	var gs = Engine.get_singleton("GazeServer")
	assert_not_null(gs, "GazeServer singleton must exist")

	var profile = DisplayProfile.new()
	profile.set_logical_size_px(Vector2i(1920, 1080))
	profile.set_physical_size_mm(Vector2(345.0, 215.0))

	gs.set_display_profile(profile)
	var active_profile = gs.get_display_profile()
	assert_not_null(active_profile, "Active display profile should not be null")
	assert_eq(active_profile.get_logical_size_px(), Vector2i(1920, 1080), "Logical size matches")
	assert_eq(active_profile.get_physical_size_mm(), Vector2(345.0, 215.0), "Physical size matches")
