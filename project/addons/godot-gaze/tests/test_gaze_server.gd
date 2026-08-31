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

func test_gaze_server_device_calibration_injection():
	var gs = Engine.get_singleton("GazeServer")
	assert_not_null(gs, "GazeServer singleton must exist")

	var calib = MockDeviceCalibration.new()
	calib.set_logical_size_px(Vector2i(1920, 1080))
	calib.set_physical_size_mm(Vector2(345.0, 215.0))
	calib.set_window_position(Vector2(50.0, 25.0))

	gs.set_device_calibration(calib)
	var active_calib = gs.get_device_calibration()
	assert_not_null(active_calib, "Active device calibration should not be null")
	assert_eq(active_calib.get_logical_size_px(), Vector2i(1920, 1080), "Logical size matches")
	assert_eq(active_calib.get_physical_size_mm(), Vector2(345.0, 215.0), "Physical size matches")
	assert_eq(active_calib.get_window_position(), Vector2(50.0, 25.0), "Window position matches")

func test_gaze_server_tracking_lifecycle_and_camera_recovery():
	var gs = Engine.get_singleton("GazeServer")
	var vs = Engine.get_singleton("VisionServer")
	assert_not_null(gs, "GazeServer singleton must exist")
	assert_not_null(vs, "VisionServer singleton must exist")

	# Initial state: active trackers must be zero (no premature autostart at server constructor level)
	assert_eq(gs.get_active_tracker_count(), 0, "Initial tracker count must be zero")

	# Start tracking: transitions 0 -> 1
	var transitioned = gs.start_tracking()
	assert_true(transitioned, "First start_tracking must transition from 0 -> 1")
	assert_eq(gs.get_active_tracker_count(), 1, "Active trackers count must be 1")

	# Second start tracking: refcount becomes 2, does not transition from 0
	var transitioned_second = gs.start_tracking()
	assert_false(transitioned_second, "Second start_tracking must not transition from 0")
	assert_eq(gs.get_active_tracker_count(), 2, "Active trackers count must be 2")

	# Unref tracker: refcount becomes 1
	gs.stop_tracking(false)
	assert_eq(gs.get_active_tracker_count(), 1, "Active trackers count must be 1")

	# Full stop
	gs.stop_tracking(true)
	assert_eq(gs.get_active_tracker_count(), 0, "Active trackers count must return to 0 after immediate stop")

func test_gaze_server_domain_telemetry_apis():
	var gs = Engine.get_singleton("GazeServer")
	assert_not_null(gs, "GazeServer singleton must exist")

	# Landmarks should return a valid PackedVector2Array
	var landmarks = gs.get_face_landmarks()
	assert_not_null(landmarks, "get_face_landmarks should return a PackedVector2Array")

	# Eye crops should return an Array
	var crops = gs.get_eye_crops()
	assert_not_null(crops, "get_eye_crops should return an Array")

	# Face detected should return a boolean
	var detected = gs.is_face_detected()
	assert_true(detected is bool, "is_face_detected should return a boolean")

func test_mouse_emulation_settings_and_api():
	var gs = Engine.get_singleton("GazeServer")
	assert_not_null(gs, "GazeServer singleton must exist")
	
	# ProjectSetting checks
	assert_true(ProjectSettings.has_setting("gaze/pointing/mouse_emulation_dwell_sec"), "gaze/pointing/mouse_emulation_dwell_sec must be registered")
	assert_true(ProjectSettings.has_setting("gaze/pointing/mouse_emulation_transition_sec"), "gaze/pointing/mouse_emulation_transition_sec must be registered")
	
	var default_dwell = ProjectSettings.get_setting("gaze/pointing/mouse_emulation_dwell_sec")
	assert_almost_eq(default_dwell, 2.0, 0.01, "Default dwell setting should be 2.0 seconds")
	
	var default_trans = ProjectSettings.get_setting("gaze/pointing/mouse_emulation_transition_sec")
	assert_almost_eq(default_trans, 0.3, 0.01, "Default transition setting should be 0.3 seconds")
	
	# Default runtime getter checks
	assert_almost_eq(gs.get_mouse_emulation_dwell_sec(), 2.0, 0.01, "GazeServer default dwell should be 2.0 seconds")
	assert_almost_eq(gs.get_mouse_emulation_transition_sec(), 0.3, 0.01, "GazeServer default transition should be 0.3 seconds")
	
	# Runtime modifications
	gs.set_mouse_emulation_dwell_sec(3.5)
	assert_almost_eq(gs.get_mouse_emulation_dwell_sec(), 3.5, 0.01, "GazeServer should reflect updated dwell")
	
	gs.set_mouse_emulation_transition_sec(0.5)
	assert_almost_eq(gs.get_mouse_emulation_transition_sec(), 0.5, 0.01, "GazeServer should reflect updated transition")
	
	# Reset
	gs.set_mouse_emulation_dwell_sec(2.0)
	gs.set_mouse_emulation_transition_sec(0.3)


