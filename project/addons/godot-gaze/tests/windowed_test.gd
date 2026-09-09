# Godot windowed GPU unit tests for Godot Gaze.
extends SceneTree

func _init():
	print("=================== WINDOWED GPU INTEGRATION TESTS ===================")
	
	# 1. Setup GazeDeviceProfile
	var profile = GazeDeviceProfile.create_system_guess()

	# 2. Test GPU Native Handle Resolution Verification Test
	print("=================== E2E TEST: GPU NATIVE HANDLE RESOLUTION (WINDOWED) ===================")
	var mock_tex_img = Image.create(64, 64, false, Image.FORMAT_RGB8)
	mock_tex_img.fill(Color.GREEN)
	var mock_tex = ImageTexture.create_from_image(mock_tex_img)
	
	var tex_rid = mock_tex.get_rid()
	if not tex_rid.is_valid():
		printerr("FAIL: Mock texture RID is invalid")
		quit(1)
		return
	
	var rd_tex = RenderingServer.texture_get_rd_texture(tex_rid)
	if not rd_tex.is_valid():
		printerr("FAIL: RenderingServer.texture_get_rd_texture returned invalid RID")
		quit(1)
		return
		
	var rd = RenderingServer.get_rendering_device()
	if not rd:
		printerr("FAIL: RenderingDevice is null in windowed mode")
		quit(1)
		return
		
	var handle = rd.texture_get_native_handle(rd_tex)
	print("Resolved Native GPU Handle from RenderingDevice: ", handle)
	if handle == 0:
		printerr("FAIL: Resolved native GPU handle is zero in windowed mode")
		quit(1)
		return
	print("PASS: GPU Native Handle Resolution verified successfully.")

	# 3. Test GPU-based Preprocessing & Compute Shaders (needs active Window/Renderer)
	# 3. Test GPU-based Preprocessing & Compute Shaders (needs active Window/Renderer)
	print("=================== E2E TEST: GPU COMPUTE SHADER INTEGRITY AND EYE CROPS ===================")
	var vs = Engine.get_singleton("VisionServer")
	var gs = Engine.get_singleton("GazeServer")
	if not gs or not vs:
		printerr("FAIL: GazeServer or VisionServer singleton not found")
		quit(1)
		return

	gs.set_device_profile(profile)

	var cam_rid = vs.camera_create()
	vs.camera_set_device_id(cam_rid, -1)
	vs.camera_set_resolution(cam_rid, 1440, 960)
	vs.camera_set_focal_length(cam_rid, 1440.0 * 1.5625)
	vs.camera_start(cam_rid)

	gs.set_camera_vision_rid(cam_rid)
	gs.start_processing()

	# Load the real face image from tests/resources/self_left_left.jpg
	var face_img = Image.new()
	var err = face_img.load("../tests/resources/self_left_left.jpg")
	if err != OK:
		err = face_img.load("res://tests/resources/self_left_left.jpg")
	if err != OK:
		err = face_img.load("res://addons/godot-gaze/tests/resources/self_left_left.jpg")
	if err != OK:
		printerr("FAIL: Shaders/Crops - Failed to load face image, code: ", err)
		quit(1)
		return
	
	# Convert image texture
	var face_tex = ImageTexture.create_from_image(face_img)

	# Wait a few frames for the asynchronous pipeline to execute, injecting the texture each frame
	var latest_event: InputEventGaze = null
	for frame_step in range(30):
		vs.inject_texture(cam_rid, face_tex)
		gs.trigger_process()
		await create_timer(0.05).timeout
		var ev = gs.get_most_recent_event()
		if ev is InputEventGaze and ev.head_transform.origin != Vector3(0, 0, -500) and ev.head_transform.origin != Vector3(0, 0, 500) and ev.head_transform.origin != Vector3.ZERO:
			latest_event = ev
			break

	# Asserts
	if not latest_event:
		printerr("FAIL: Shaders/Crops - Timeout waiting for face tracking event")
		quit(1)
		return

	# Assert spatial head pose outputs are correct (convex nose, left-right X coordinate alignment, head forward vector)
	var xform = latest_event.head_transform
	print("Tracker Head Transform: ", xform)
	
	var nose_pos = xform * Vector3(0.0, 0.5, -52.0)
	var eye_l_pos = xform * Vector3(-30.0, 28.676, 0.0)
	var eye_r_pos = xform * Vector3(30.0, 28.676, 0.0)
	
	print("Nose position: ", nose_pos, " | Left eye position: ", eye_l_pos, " | Right eye position: ", eye_r_pos)
	
	# Convex nose assertion: nose tip is closer to camera (larger / less negative Z in Godot Camera Space) than the eyes
	if nose_pos.z <= eye_l_pos.z or nose_pos.z <= eye_r_pos.z:
		printerr("FAIL: Shaders/Crops - Head transform has concave nose! nose.z = ", nose_pos.z, " eye_l.z = ", eye_l_pos.z)
		quit(1)
		return
		
	# Head forward vector direction assertion: must point generally towards the screen (+Z direction in Godot Camera Space)
	var head_forward = -xform.basis.z.normalized()
	if head_forward.z <= 0.5:
		printerr("FAIL: Shaders/Crops - Head forward vector points away from the screen! head_forward = ", head_forward)
		quit(1)
		return

	print("PASS: Face and gaze estimation executed successfully on still frame.")

	# 4. Test Dynamic Window Position/Size Synchronization & Corner Invariance
	print("=================== E2E TEST: DYNAMIC WINDOW POSITION SYNC & CORNER INVARIANCE ===================")
	var screen_id = DisplayServer.window_get_current_screen()
	var screen_size = DisplayServer.screen_get_size(screen_id)
	var window_size = DisplayServer.window_get_size()
	var test_scale = DisplayServer.screen_get_scale(screen_id)
	print("Screen size (lpix): ", screen_size, " | Window size (lpix): ", window_size, " | Scale: ", test_scale)

	# 4a. Centered Window Test
	var center_pos = (screen_size - window_size) / 2
	DisplayServer.window_set_position(center_pos)
	await create_timer(0.3).timeout

	var proj_center = gs.project_ray_to_viewport(Vector3(0, 0, -500.0), Vector3(0, 0, 1.0))
	print("Centered Window Position: ", DisplayServer.window_get_position(), " -> Proj Center: ", proj_center)
	var expected_center_x = window_size.x / 2.0
	print("Expected Center X (window center): ", expected_center_x, " | Actual: ", proj_center.x)
	if abs(proj_center.x - expected_center_x) > 2.0:
		printerr("FAIL: Camera optical center ray did not hit window horizontal center! Expected ", expected_center_x, ", got ", proj_center.x)
		quit(1)
		return
	print("PASS: Camera optical axis hits exact window horizontal center.")

	# 4b. 4-Corner Window Invariance Test
	var test_positions = {
		"Top-Left": Vector2i(0, 0),
		"Top-Right": Vector2i(screen_size.x - window_size.x, 0),
		"Bottom-Left": Vector2i(0, screen_size.y - window_size.y),
		"Bottom-Right": Vector2i(screen_size.x - window_size.x, screen_size.y - window_size.y)
	}

	for corner_name in test_positions:
		var target_pos = test_positions[corner_name]
		DisplayServer.window_set_position(target_pos)
		await create_timer(0.3).timeout
		var actual_pos = DisplayServer.window_get_position()
		var proj_corner = gs.project_ray_to_viewport(Vector3(0, 0, -500.0), Vector3(0, 0, 1.0))
		var expected_x = (screen_size.x / 2.0) - actual_pos.x
		print("Corner [", corner_name, "] at ", actual_pos, " -> Proj: ", proj_corner, " (Expected X: ", expected_x, ")")
		if abs(proj_corner.x - expected_x) > 2.0:
			printerr("FAIL: Corner [", corner_name, "] projection mismatch! Expected X ", expected_x, ", got ", proj_corner.x)
			quit(1)
			return
		print("PASS: Corner [", corner_name, "] verified.")

	# 4c. Fullscreen Window Mode Test
	print("=================== E2E TEST: FULLSCREEN WINDOW MODE PROJECTION ===================")
	DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
	await create_timer(0.5).timeout
	var fs_mode = DisplayServer.window_get_mode()
	var fs_win_pos = DisplayServer.window_get_position()
	var fs_win_size = DisplayServer.window_get_size()
	var fs_screen_size = DisplayServer.screen_get_size(screen_id)
	print("Fullscreen Mode: ", fs_mode, " | Win Pos: ", fs_win_pos, " | Win Size: ", fs_win_size, " | Screen Size: ", fs_screen_size)
	print("Root Viewport Size: ", root.size, " | Visible Rect: ", root.get_visible_rect(), " | Final Xform: ", root.get_final_transform())
	print("Profile Logical Size: ", profile.get_logical_size_px(), " | Physical Size: ", profile.get_physical_size_mm())

	var fs_proj = gs.project_ray_to_viewport(Vector3(0, 0, -500.0), Vector3(0, 0, 1.0))
	print("Fullscreen Center Ray Projection (Window Space): ", fs_proj)
	var expected_fs_x = fs_win_size.x / 2.0
	print("Expected Fullscreen Center X (Window Space): ", expected_fs_x, " | Actual X: ", fs_proj.x)
	if abs(fs_proj.x - expected_fs_x) > 2.0:
		printerr("FAIL: Fullscreen window space center ray mismatch! Expected ", expected_fs_x, ", got ", fs_proj.x)
		quit(1)
		return

	var canvas_proj = root.get_final_transform().affine_inverse() * fs_proj
	var expected_canvas_center = root.get_visible_rect().size / 2.0
	print("Fullscreen Center Ray Projection (Canvas Space): ", canvas_proj, " | Expected Canvas Center: ", expected_canvas_center)
	if abs(canvas_proj.x - expected_canvas_center.x) > 2.0:
		printerr("FAIL: Fullscreen canvas space center ray mismatch! Expected ", expected_canvas_center.x, ", got ", canvas_proj.x)
		quit(1)
		return
	print("PASS: Fullscreen window and canvas projection verified.")

	# Restore windowed mode
	DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
	DisplayServer.window_set_size(window_size)
	DisplayServer.window_set_position(center_pos)
	await create_timer(0.5).timeout

	# Clean up synthetic camera
	gs.stop_tracking(true)
	vs.camera_stop(cam_rid)
	vs.camera_free(cam_rid)

	# 5. Test Physical Camera Device 0 and Telemetry Probe
	print("=================== E2E TEST: PHYSICAL CAMERA SERVER (DEVICE 0) & TELEMETRY ===================")
	var phys_cam_rid = vs.camera_create()
	vs.camera_set_device_id(phys_cam_rid, 0)
	gs.set_camera_vision_rid(phys_cam_rid)
	var started = gs.start_tracking()
	print("GazeServer start_tracking (device 0) returned: ", started)
	if vs.camera_is_active(phys_cam_rid):
		print("PASS: Physical camera device 0 active.")
		var cs = Engine.get_singleton("CameraServer")
		if cs:
			var feeds = cs.feeds()
			print("CameraServer Feed Count: ", feeds.size())
			for f in feeds:
				if f:
					print(" -> Feed ID: ", f.get_id(), " Name: ", f.get_name(), " Active: ", f.is_active(), " Datatype: ", f.get_datatype(), " Position: ", f.get_position())
		gs.camera_set_preview_requested(true)
		
		# Poll frames for 1 second
		for i in range(20):
			gs.trigger_process()
			await create_timer(0.05).timeout

		var cur_tex = gs.get_camera_texture()
		if cur_tex:
			print(" -> Current Texture Size: ", cur_tex.get_size())
			var img = cur_tex.get_image()
			if img:
				print(" -> Captured Image Format: ", img.get_format(), " (FORMAT_RGB8=4, FORMAT_RGBA8=5, FORMAT_L8=0, FORMAT_R8=1) Width: ", img.get_width(), " Height: ", img.get_height(), " Data size: ", img.get_data().size())
		else:
			print(" -> Current Texture is null (camera feed may be headless/mock).")
		gs.stop_tracking(true)
	else:
		print("INFO: Physical camera device 0 not active or feeds unavailable in this test session (cleanly handled).")
	vs.camera_free(phys_cam_rid)

	print("==================================================================")
	print("ALL Windowed GPU integration tests have passed successfully!")
	print("==================================================================")
	quit(0)
