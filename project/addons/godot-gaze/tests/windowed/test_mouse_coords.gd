extends SceneTree

func _init():
	call_deferred("run_diag")

func run_diag():
	print("--- MOUSE COORD DIAGNOSTICS ---")
	var gds = Engine.get_singleton("GazeDisplayServer")
	var gs = Engine.get_singleton("GazeServer")
	var num_screens = DisplayServer.get_screen_count()
	print("Screen count: ", num_screens)
	for i in range(num_screens):
		print("Screen ", i, ": pos=", DisplayServer.screen_get_position(i), 
			  " size=", DisplayServer.screen_get_size(i), 
			  " scale=", DisplayServer.screen_get_scale(i))
	
	var cur_screen = DisplayServer.window_get_current_screen()
	print("Current screen: ", cur_screen)
	print("DisplayServer.window_get_position: ", DisplayServer.window_get_position())
	print("DisplayServer.window_get_size: ", DisplayServer.window_get_size())
	if gds:
		print("GazeDisplayServer.get_window_rect_pixels: ", gds.get_window_rect_pixels())
		print("GazeDisplayServer.get_screen_scale: ", gds.get_screen_scale())
		print("GazeDisplayServer.mouse_get_position: ", gds.mouse_get_position())
	
	print("DisplayServer.mouse_get_position: ", DisplayServer.mouse_get_position())
	var root = get_root()
	print("root.get_visible_rect: ", root.get_visible_rect())
	print("root.get_final_transform: ", root.get_final_transform())
	print("root.get_mouse_position: ", root.get_mouse_position())

	# Strict Red Phase Assertion: GazeDisplayServer must return logical display coordinates
	if gds and DisplayServer.screen_get_scale() > 1.0:
		var gds_m = gds.mouse_get_position()
		var ds_m = DisplayServer.mouse_get_position()
		var expected_gds_x = ds_m.x / DisplayServer.screen_get_scale()
		var err_x = abs(gds_m.x - expected_gds_x)
		print("GDS mouse logical alignment check: gds_x=", gds_m.x, " expected_x=", expected_gds_x, " err=", err_x)
		if err_x > 2.0:
			printerr("RED PHASE ASSERTION FAILED: GazeDisplayServer.mouse_get_position() returned physical pixels instead of logical pixels! err=", err_x)
			quit(1)
			return

	# Enable mouse emulation
	gs.set_emulate_gaze_from_mouse(true)
	gs.set_emulate_mouse_from_gaze(false)

	# Simulate physical mouse moving to (1000, 700) on display
	# Note: GazeDisplayServer.mouse_get_position is hardware position
	# Let's inspect what mouse_pos is calculated as in synthesize_event
	var m_canvas = root.get_mouse_position()
	print("Root mouse pos (canvas): ", m_canvas)
	
	# Verify inverse transform round-trip to camera space and back
	var profile = gs.get_device_profile()
	var scr_scale = gds.get_screen_scale() if gds else 1.0
	var win_offset = gds.get_window_rect_pixels().position if gds else Vector2i(0, 0)
	var win_phys = root.get_final_transform() * m_canvas
	var win_logical = win_phys / scr_scale
	var disp_logical = win_logical + Vector2(win_offset)
	print("disp_logical: ", disp_logical)

	var phys_sz = profile.get_physical_size_mm() if profile else Vector2(301.214, 195.63)
	var log_sz = profile.get_logical_size_px() if profile else Vector2i(1512, 982)
	var cam_off = profile.get_camera_offset_mm() if profile else Vector3(0, 0, 0)
	print("profile logical_size: ", log_sz)
	print("profile physical_size: ", phys_sz)
	print("profile pixel_pitch: ", profile.get_pixel_pitch_mm() if profile else "null")
	print("profile camera_offset: ", cam_off)



	var dx_mm = (disp_logical.x - log_sz.x * 0.5) * (phys_sz.x / float(log_sz.x))
	var dy_mm = (disp_logical.y - log_sz.y * 0.5) * (phys_sz.y / float(log_sz.y))
	var target_cam = Vector3(-dx_mm + cam_off.x, -dy_mm - cam_off.y, 0.0)
	var eye_origin = Vector3(0, 0, -500.0)
	var gaze_dir = (target_cam - eye_origin).normalized()

	var projected_back = gs.project_ray_to_canvas(eye_origin, gaze_dir)
	print("Projected back to canvas: ", projected_back)
	var round_trip_err = (projected_back - m_canvas).length()
	print("Round-trip error: ", round_trip_err)
	if round_trip_err > 1.0:
		printerr("ASSERTION FAILED: Round-trip projection error > 1.0! err=", round_trip_err)
		quit(1)
		return

	if gds:
		var win_mouse = gds.get_window_mouse_position()
		print("gds.get_window_mouse_position(): ", win_mouse)
		var expected_win_mouse = m_canvas / scr_scale
		var win_mouse_err = (win_mouse - expected_win_mouse).length()
		print("Window mouse error: ", win_mouse_err)
		if win_mouse_err > 1.5:
			printerr("ASSERTION FAILED: Window mouse position does not match canvas/scale! err=", win_mouse_err)
			quit(1)
			return

	print("ALL MOUSE COORD ASSERTIONS PASSED PERFECTLY!")
	quit(0)




