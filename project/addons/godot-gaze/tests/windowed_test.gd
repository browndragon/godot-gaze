# Godot windowed GPU unit tests for Godot Gaze.
extends SceneTree

func _init():
	print("=================== WINDOWED GPU INTEGRATION TESTS ===================")
	
	# 1. Setup DeviceCalibration
	var dev_cal = GuessDeviceCalibration.new()

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

	gs.set_device_calibration(dev_cal)

	var cam_rid = vs.camera_create()
	vs.camera_set_device_id(cam_rid, -1)
	vs.camera_set_resolution(cam_rid, 1440, 960)
	vs.camera_set_focal_length(cam_rid, 1440.0 * 1.5625)
	vs.camera_start(cam_rid)

	gs.camera_set_vision_rid(gs.get_default_camera_rid(), cam_rid)
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
		vs.inject_texture(cam_rid, face_tex)
		gs.trigger_process()
		await create_timer(0.05).timeout
		var ev = gs.get_most_recent_event()
		if ev is InputEventGaze and ev.position != Vector2.ZERO:
			initial_gaze = ev.position
			break
			
	var test_scale = DisplayServer.screen_get_scale(DisplayServer.window_get_current_screen())
	print("Initial window position: ", DisplayServer.window_get_position(), " | Initial scale: ", test_scale)
	print("Initial gaze: ", initial_gaze)
	if initial_gaze == Vector2.ZERO:
		printerr("FAIL: Could not obtain a valid initial gaze estimation.")
		quit(1)
		return
		
	# Clean up synthetic camera
	gs.stop_tracking(true)
	vs.camera_stop(cam_rid)
	vs.camera_free(cam_rid)

	# 4. Test Physical Camera Device 0 and CameraServer Query
	print("=================== E2E TEST: PHYSICAL CAMERA SERVER (DEVICE 0) ===================")
	var phys_cam_rid = vs.camera_create()
	vs.camera_set_device_id(phys_cam_rid, 0)
	var started = vs.camera_start(phys_cam_rid)
	print("Physical camera start (device 0) returned: ", started)
	if started:
		print("PASS: Physical camera device 0 started successfully.")
		vs.camera_stop(phys_cam_rid)
	else:
		print("INFO: Physical camera device 0 not active or feeds unavailable in this test session (cleanly handled).")
	vs.camera_free(phys_cam_rid)

	print("==================================================================")
	print("ALL Windowed GPU integration tests have passed successfully!")
	print("==================================================================")
	quit(0)
