# Camera exit test simulating toggles and exit cleanups.
extends SceneTree

func _init():
	print("=================== CAMERA EXIT TEST ===================")
	
	# 1. Start GazeServer tracking
	var gs = Engine.get_singleton("GazeServer")
	if not gs:
		printerr("FAIL: GazeServer not available")
		quit(1)
		return

	var started = gs.start_tracking()
	print("GazeServer tracking started (step: ", started, "), running toggles...")
	
	var vs = Engine.get_singleton("VisionServer")
	var cam_rid = vs.camera_create(0) if vs else RID()
	
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
