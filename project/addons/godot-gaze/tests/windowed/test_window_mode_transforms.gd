extends SceneTree

func _init():
	call_deferred("run_tests")

func run_tests():
	print("=================== WINDOW MODE CANVAS TRANSFORMS TEST ===================")
	var gs = Engine.get_singleton("GazeServer")
	if not gs:
		printerr("FAIL: GazeServer singleton not available")
		quit(1)
		return

	var gds = Engine.get_singleton("GazeDisplayServer")
	var screen_id = DisplayServer.window_get_current_screen()
	var screen_size_lpix = gds.get_screen_size_pixels(screen_id) if gds else Vector2(DisplayServer.screen_get_size(screen_id))
	var screen_size_mm = gds.get_screen_size_mm(screen_id) if gds else Vector2(301.214, 195.63)
	var screen_scale = gds.get_screen_scale(screen_id) if gds else DisplayServer.screen_get_scale(screen_id)
	print("Display Metrics - Size (lpix): ", screen_size_lpix, " | Size (mm): ", screen_size_mm, " | Scale: ", screen_scale)

	var profile = GazeDeviceProfile.new()
	profile.set_logical_size_px(Vector2i(int(screen_size_lpix.x), int(screen_size_lpix.y)))
	profile.set_physical_size_mm(screen_size_mm)
	profile.set_camera_offset_mm(Vector3(0.0, 0.0, 0.0))
	gs.set_device_profile(profile)

	# -------------------------------------------------------------
	# 1. Centered Window Test
	# -------------------------------------------------------------
	print("--- 1. Testing Centered Window Mode ---")
	DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
	var win_rect = gds.get_window_rect_pixels() if gds else Rect2i(DisplayServer.window_get_position(), DisplayServer.window_get_size())
	var window_size = win_rect.size
	var center_pos = Vector2i((int(screen_size_lpix.x) - window_size.x) / 2, (int(screen_size_lpix.y) - window_size.y) / 2)
	DisplayServer.window_set_position(center_pos * screen_scale)
	await create_timer(0.4).timeout

	var actual_win_pos = gds.get_window_rect_pixels().position if gds else DisplayServer.window_get_position()
	print("Actual Centered Window Pos (lpix): ", actual_win_pos, " | Window size: ", window_size)

	var root = get_root()
	var canvas_center_expected = root.get_visible_rect().size * 0.5
	print("Windowed - Root Visible Rect: ", root.get_visible_rect(), " | Final Xform: ", root.get_final_transform())

	# Ray aimed directly along optical axis (screen center)
	var cam_axis_win = gs.project_ray_to_canvas(Vector3(0, 0, -500.0), Vector3(0, 0, 1.0))
	print("Windowed - Screen Center Ray -> Canvas Pos: ", cam_axis_win, " (Expected: ", canvas_center_expected, ")")
	if abs(cam_axis_win.x - canvas_center_expected.x) > 3.0 or abs(cam_axis_win.y - canvas_center_expected.y) > 3.0:
		printerr("FAIL: Windowed screen center ray does not land on canvas center! Got: ", cam_axis_win, " Expected: ", canvas_center_expected)
		quit(1)
		return
	print("PASS: Windowed screen center ray lands on canvas center.")

	# Ray aimed at the right edge of the physical screen
	var right_edge_x_mm = screen_size_mm.x * 0.5
	var right_edge_dir = (Vector3(-right_edge_x_mm, 0.0, 500.0) - Vector3(0, 0, 0)).normalized()
	# Note: in Godot camera coordinates, +X is camera left, -X is camera right (looking towards +Z display)
	var right_edge_win = gs.project_ray_to_canvas(Vector3(0, 0, -500.0), right_edge_dir)
	var win_dx = right_edge_win.x - cam_axis_win.x
	print("Windowed - Right Edge Ray -> Canvas Pos: ", right_edge_win, " (Delta X from center: ", win_dx, ")")
	if win_dx <= 10.0:
		printerr("FAIL: Windowed right-edge gaze shifted left or did not move right! Delta X: ", win_dx)
		quit(1)
		return
	print("PASS: Windowed right-edge gaze produces positive X offset from center.")

	# -------------------------------------------------------------
	# 2. Fullscreen Mode Test
	# -------------------------------------------------------------
	print("--- 2. Testing Fullscreen Mode ---")
	DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
	await create_timer(0.6).timeout

	var fs_win_rect = gds.get_window_rect_pixels() if gds else Rect2i(DisplayServer.window_get_position(), DisplayServer.window_get_size())
	var fs_win_pos = fs_win_rect.position
	var fs_canvas_center_expected = root.get_visible_rect().size * 0.5
	print("Fullscreen - Win Pos: ", fs_win_pos, " | Root Visible Rect: ", root.get_visible_rect(), " | Final Xform: ", root.get_final_transform())

	# Ray aimed directly along optical axis (screen center)
	var cam_axis_fs = gs.project_ray_to_canvas(Vector3(0, 0, -500.0), Vector3(0, 0, 1.0))
	print("Fullscreen - Screen Center Ray -> Canvas Pos: ", cam_axis_fs, " (Expected: ", fs_canvas_center_expected, ")")
	# Allow for minor window position offset (e.g. macOS menu bar 37pt offset in non-exclusive fullscreen)
	var expected_fs_x = (screen_size_lpix.x / 2.0) - fs_win_pos.x
	var expected_fs_y = (screen_size_lpix.y / 2.0) - fs_win_pos.y
	var expected_fs_canvas = root.get_final_transform().affine_inverse() * (Vector2(expected_fs_x, expected_fs_y) * screen_scale)
	print("Fullscreen - Expected Canvas Pos taking menu bar into account: ", expected_fs_canvas)
	if abs(cam_axis_fs.x - expected_fs_canvas.x) > 3.0 or abs(cam_axis_fs.y - expected_fs_canvas.y) > 3.0:
		printerr("FAIL: Fullscreen screen center ray mismatch! Got: ", cam_axis_fs, " Expected: ", expected_fs_canvas)
		quit(1)
		return
	print("PASS: Fullscreen screen center ray lands on expected canvas position.")

	# Ray aimed at the right edge of the physical screen
	var right_edge_fs = gs.project_ray_to_canvas(Vector3(0, 0, -500.0), right_edge_dir)
	var fs_dx = right_edge_fs.x - cam_axis_fs.x
	print("Fullscreen - Right Edge Ray -> Canvas Pos: ", right_edge_fs, " (Delta X from center: ", fs_dx, ")")
	if fs_dx <= 10.0:
		printerr("FAIL: Fullscreen right-edge gaze shifted left or did not move right! Delta X: ", fs_dx)
		quit(1)
		return
	print("PASS: Fullscreen right-edge gaze produces positive X offset from center.")

	# -------------------------------------------------------------
	# 3. Geometric Invariance & Direction Parity
	# -------------------------------------------------------------
	print("--- 3. Testing Direction Parity & Sign Invariance ---")
	if (win_dx > 0) != (fs_dx > 0):
		printerr("FAIL: Vector sign flipped between windowed and fullscreen! win_dx: ", win_dx, " fs_dx: ", fs_dx)
		quit(1)
		return
	print("PASS: Right-edge gaze maintains positive direction (+X) in BOTH windowed and fullscreen modes.")

	# Restore windowed mode
	DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
	await create_timer(0.3).timeout

	print("==================================================================")
	print("ALL Window Mode Canvas Transform Tests PASSED Successfully!")
	print("==================================================================")
	quit(0)
