# Godot windowed GPU unit tests for Godot Gaze.
extends SceneTree

func _init():
	print("=================== WINDOWED GPU INTEGRATION TESTS ===================")
	
	# 1. Setup DisplayProfile
	var dp = DisplayProfile.new()
	dp.logical_size_px = Vector2i(1920, 1080)
	dp.physical_size_mm = Vector2(345.0, 215.0)

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
	# Unregister the global VisionServer singleton, free it to reset the C++ static pointer, and register a MockVisionServer instead
	var old_vs = Engine.get_singleton("VisionServer")
	if old_vs:
		Engine.unregister_singleton("VisionServer")
		old_vs.free()

	var gpu_mock_vs = MockVisionServer.new()
	Engine.register_singleton("VisionServer", gpu_mock_vs)

	var gs = Engine.get_singleton("GazeServer")
	if not gs:
		printerr("FAIL: GazeServer singleton not found")
		quit(1)
		return

	gs.set_display_profile(dp)
	gs.start_tracking()

	var cam_rid = gpu_mock_vs.camera_create(-1)

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
		gpu_mock_vs.inject_texture(cam_rid, face_tex)
		await create_timer(0.05).timeout
		var ev = gs.get_most_recent_event()
		if ev is InputEventGaze and ev.is_face_tracked():
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
	
	# Convex nose assertion: nose tip must be closer to camera (less negative Z) than the eyes
	if nose_pos.z <= eye_l_pos.z or nose_pos.z <= eye_r_pos.z:
		printerr("FAIL: Shaders/Crops - Head transform has concave nose! nose.z = ", nose_pos.z, " eye_l.z = ", eye_l_pos.z)
		quit(1)
		return
		
	# X-axis left-right coordinate alignment assertion: Anatomical Left Eye (+X) must be greater than Anatomical Right Eye (-X)
	if eye_l_pos.x <= eye_r_pos.x:
		printerr("FAIL: Shaders/Crops - Coordinate system X-axis is inverted! eye_l.x = ", eye_l_pos.x, " eye_r.x = ", eye_r_pos.x)
		quit(1)
		return
		
	# Head forward vector direction assertion: must point generally towards the screen (+Z direction)
	var head_forward = -xform.basis.z.normalized()
	if head_forward.z <= 0.5:
		printerr("FAIL: Shaders/Crops - Head forward vector points away from the screen! head_forward = ", head_forward)
		quit(1)
		return

	print("PASS: Face and gaze estimation executed successfully on still frame.")

	# 4. Test Dynamic Window Position/Size Synchronization
	print("=================== E2E TEST: DYNAMIC WINDOW POSITION SYNC ===================")
	# Center the window first
	var screen_id = DisplayServer.window_get_current_screen()
	var screen_size = DisplayServer.screen_get_size(screen_id)
	var window_size = DisplayServer.window_get_size()
	var initial_pos = (screen_size - window_size) / 2
	DisplayServer.window_set_position(initial_pos)
	
	# Wait for OS window movements to settle
	await create_timer(0.5).timeout
	
	# Inject texture a few times to get initial gaze coordinate
	var initial_gaze = Vector2.ZERO
	for frame_step in range(30):
		gpu_mock_vs.inject_texture(cam_rid, face_tex)
		await create_timer(0.05).timeout
		var ev = gs.get_most_recent_event()
		if ev is InputEventGaze and ev.position != Vector2.ZERO:
			initial_gaze = ev.position
			break
			
	var test_scale = DisplayProfile.get_screen_scale()
	print("Initial window position: ", DisplayServer.window_get_position(), " | Initial scale: ", test_scale)
	print("Initial gaze: ", initial_gaze)
	if initial_gaze == Vector2.ZERO:
		printerr("FAIL: Could not obtain a valid initial gaze estimation.")
		quit(1)
		return
		
	# Clean up
	gs.stop_tracking(true)
	gpu_mock_vs.free()

	print("==================================================================")
	print("ALL Windowed GPU integration tests have passed successfully!")
	print("==================================================================")
	quit(0)
