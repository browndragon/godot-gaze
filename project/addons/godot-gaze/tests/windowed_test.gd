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
	print("=================== E2E TEST: GPU COMPUTE SHADER INTEGRITY AND EYE CROPS ===================")
	# Unregister the global VisionServer singleton, free it to reset the C++ static pointer, and register a MockVisionServer instead
	var old_vs = Engine.get_singleton("VisionServer")
	if old_vs:
		Engine.unregister_singleton("VisionServer")
		old_vs.free()

	var gpu_mock_vs = MockVisionServer.new()
	Engine.register_singleton("VisionServer", gpu_mock_vs)

	# Instantiate a new GazeTracker to run with the MockVisionServer
	var gpu_tracker = GazeTracker.new()
	gpu_tracker.display_profile = dp
	root.add_child(gpu_tracker)

	var gpu_sensor = CameraSensor.new()
	gpu_sensor.name = "CameraSensor"
	gpu_sensor.camera_device_id = -1
	gpu_tracker.add_child(gpu_sensor)

	var init_success = gpu_tracker.initialize_tracker()
	if not init_success:
		printerr("FAIL: Shaders/Crops - Failed to initialize GazeTracker with MockVisionServer")
		quit(1)
		return

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

	# Connect to crops ready signal to verify the output crops
	var test_state = {
		"got_crops": false,
		"crops_not_black": false,
		"left_crop_img": null,
		"right_crop_img": null
	}

	var on_crops_ready = func(left, right):
		test_state.got_crops = true
		test_state.left_crop_img = left
		test_state.right_crop_img = right
		# Check if crops contain non-black pixels
		var left_non_black = false
		var right_non_black = false
		for x in range(left.get_width()):
			for y in range(left.get_height()):
				if left.get_pixel(x, y).r > 0.01 or left.get_pixel(x, y).g > 0.01 or left.get_pixel(x, y).b > 0.01:
					left_non_black = true
					break
			if left_non_black:
				break
		for x in range(right.get_width()):
			for y in range(right.get_height()):
				if right.get_pixel(x, y).r > 0.01 or right.get_pixel(x, y).g > 0.01 or right.get_pixel(x, y).b > 0.01:
					right_non_black = true
					break
			if right_non_black:
				break
		test_state.crops_not_black = left_non_black and right_non_black

	var gpu_eye_est = gpu_tracker.get_eye_estimator()
	gpu_eye_est.connect("eye_crops_ready", on_crops_ready)

	# Wait a few frames for the asynchronous pipeline to execute, injecting the texture each frame
	for frame_step in range(30):
		gpu_mock_vs.inject_texture(gpu_sensor.get_camera_rid(), face_tex)
		await create_timer(0.05).timeout
		if test_state.got_crops:
			break

	# Asserts
	if not test_state.got_crops:
		printerr("FAIL: Shaders/Crops - Timeout waiting for eye_crops_ready signal")
		quit(1)
		return

	print("Received eye crops! Dimensions: ", test_state.left_crop_img.get_width(), "x", test_state.left_crop_img.get_height())
	if not test_state.crops_not_black:
		printerr("FAIL: Shaders/Crops - Eye crops are flat black, compute shader/preallocated textures failed!")
		quit(1)
		return

	# Assert spatial head pose outputs are correct (convex nose, left-right X coordinate alignment, head forward vector)
	var xform = gpu_tracker.get_head_transform()
	print("Tracker Head Transform: ", xform)
	
	# TODO: Pull from a well-known constant exposed by our library.
	var nose_pos = xform * Vector3(0.0, 0.5, 52.0)
	var eye_l_pos = xform * Vector3(30.0, 28.676, 0.0)
	var eye_r_pos = xform * Vector3(-30.0, 28.676, 0.0)
	
	print("Nose position: ", nose_pos, " | Left eye position: ", eye_l_pos, " | Right eye position: ", eye_r_pos)
	
	# Convex nose assertion: nose tip must be closer to camera (less negative Z) than the eyes
	if nose_pos.z <= eye_l_pos.z or nose_pos.z <= eye_r_pos.z:
		printerr("FAIL: Shaders/Crops - Head transform has concave nose! nose.z = ", nose_pos.z, " eye_l.z = ", eye_l_pos.z)
		quit(1)
		return
		
	# X-axis left-right coordinate alignment assertion: Left eye X must be greater than Right eye X
	if eye_l_pos.x <= eye_r_pos.x:
		printerr("FAIL: Shaders/Crops - Coordinate system X-axis is inverted! eye_l.x = ", eye_l_pos.x, " eye_r.x = ", eye_r_pos.x)
		quit(1)
		return
		
	# Head forward vector direction assertion: must point generally towards the screen (+Z direction)
	var head_forward = xform.basis.z.normalized()
	if head_forward.z <= 0.5:
		printerr("FAIL: Shaders/Crops - Head forward vector points away from the screen! head_forward = ", head_forward)
		quit(1)
		return

	print("PASS: Compute shaders executed successfully on the still frame (eye crops are NOT flat black).")

	# Clean up
	gpu_tracker.stop_tracker()
	gpu_tracker.free()
	gpu_mock_vs.free()

	print("==================================================================")
	print("ALL Windowed GPU integration tests have passed successfully!")
	print("==================================================================")
	quit(0)
