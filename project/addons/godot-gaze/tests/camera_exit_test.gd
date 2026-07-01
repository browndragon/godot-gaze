# Camera exit test simulating toggles and exit cleanups.
extends SceneTree

func _init():
	print("=================== CAMERA EXIT TEST ===================")
	
	# 1. Setup DisplayProfile
	var dp = DisplayProfile.new()
	dp.logical_size_px = Vector2i(1920, 1080)
	dp.physical_size_mm = Vector2(345.0, 215.0)

	# 2. Instantiate GazeTracker
	var tracker = GazeTracker.new()
	tracker.display_profile = dp
	root.add_child(tracker)

	var sensor = CameraSensor.new()
	sensor.name = "CameraSensor"
	sensor.camera_device_id = 0
	tracker.add_child(sensor)

	var init_success = tracker.initialize_tracker()
	if not init_success:
		printerr("FAIL: Failed to initialize GazeTracker")
		quit(1)
		return

	print("GazeTracker initialized, running toggles several times...")
	
	var cam_rid = sensor.get_camera_rid()
	var vs = Engine.get_singleton("VisionServer")
	
	# Loop toggles 15 times
	for i in range(15):
		print("Iteration ", i)
		vs.camera_set_preview_requested(cam_rid, true)
		await create_timer(0.05).timeout
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
		await create_timer(0.05).timeout
		vs.camera_set_preview_requested(cam_rid, false)
		await create_timer(0.05).timeout
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
		await create_timer(0.05).timeout

	print("Calling quit(0) immediately after several toggles...")
	quit(0)
