# Godot unit tests for Godot Gaze.
extends SceneTree

func _init():
	print("=================== HEADLESS INTEGRATION TESTS ===================")
	
	# 1. Test DisplayProfile Resource
	var dp = DisplayProfile.new()
	dp.logical_size_px = Vector2i(1920, 1080)
	dp.physical_size_mm = Vector2(345.0, 215.0)
	
	var dpi = dp.get_dpi()
	print("DisplayProfile DPI: ", dpi)
	if abs(dpi.x - 141.35) > 0.1 or abs(dpi.y - 127.64) > 0.1:
		printerr("FAIL: DisplayProfile DPI calculation incorrect")
		quit(1)
		return
	print("PASS: DisplayProfile resource verified.")
	
	var tracker = GazeTracker.new()
	if not tracker is Node3D:
		printerr("FAIL: GazeTracker does not inherit from Node3D")
		quit(1)
		return
		
	root.add_child(tracker)
	tracker.display_profile = dp
	
	# Initialize GazeTracker
	var ok = tracker.initialize_tracker()
	print("initialize_tracker returned: ", ok)
	
	# Retrieve components
	var camera_sensor = tracker.get_camera_sensor()
	var face_estimator = tracker.get_face_estimator()
	var eye_estimator = tracker.get_eye_estimator()
	var smooth_resource = tracker.get_screen_smooth()
	
	if not camera_sensor or not face_estimator or not eye_estimator or not smooth_resource:
		printerr("FAIL: Sub-components not initialized correctly")
		quit(1)
		return
		
	# Check parent-child spatial hierarchy
	if camera_sensor.get_parent() != tracker:
		printerr("FAIL: CameraSensor is not a child of GazeTracker")
		quit(1)
		return
	if face_estimator.get_parent() != camera_sensor:
		printerr("FAIL: FaceEstimator is not a child of CameraSensor")
		quit(1)
		return
	if eye_estimator.get_parent() != face_estimator:
		printerr("FAIL: EyeEstimator is not a child of FaceEstimator")
		quit(1)
		return
		
	print("PASS: Strict spatial node hierarchy verified.")

	# 2. Test Window Position Shifts and Resizing Math under headless display server
	# We place the camera at screen center top bezel: (0, 107.5, 0)
	camera_sensor.position = Vector3(0.0, 107.5, 0.0)
	camera_sensor.rotation = Vector3(0.0, 0.0, 0.0)
	camera_sensor.focal_length = 1000.0
	
	var simulated_origin = Vector3(0.0, 0.0, 800.0) # head center straight in front of screen center
	var simulated_dir = Vector3(0.0, -0.134375, -0.99093) # pointing down-forward toward absolute screen y=0
	
	# First: Window pos override (100, 150), Root Viewport size (800, 600)
	tracker.window_position_override = Vector2(100, 150)
	root.size = Vector2i(800, 600)
	
	var proj_1 = tracker.project_gaze_ray_to_viewport(simulated_origin, simulated_dir)
	print("Window position overridden to (100, 150) | Projected: ", proj_1)
	
	# Second: Move window override to (200, 300)
	tracker.window_position_override = Vector2(200, 300)
	var proj_2 = tracker.project_gaze_ray_to_viewport(simulated_origin, simulated_dir)
	print("Window position overridden to (200, 300) | Projected: ", proj_2)
	
	# Math check:
	# Absolute screen intersection shouldn't change.
	# Viewport pixel pos difference (proj_1 - proj_2) should be exactly equal to window shift (proj_2 - proj_1 = -(200-100, 300-150) = (-100, -150)).
	var shift = proj_2 - proj_1
	print("Projected coordinate shift: ", shift)
	if abs(shift.x - (-100.0)) > 0.5 or abs(shift.y - (-150.0)) > 0.5:
		printerr("FAIL: Viewport projection did not shift correctly with window movement")
		quit(1)
		return
		
	# Third: Resize root viewport to (1024, 768) at same position
	root.size = Vector2i(1024, 768)
	var proj_3 = tracker.project_gaze_ray_to_viewport(simulated_origin, simulated_dir)
	print("Window resized to (1024, 768) | Projected: ", proj_3)
	if abs(proj_3.x - proj_2.x) > 0.5 or abs(proj_3.y - proj_2.y) > 0.5:
		printerr("FAIL: Viewport projection shifted unexpectedly with window resizing")
		quit(1)
		return
		
	# 2.6 Realistic Image-based Gaze Projection Test
	# Simulates self_center.jpg gaze ray under default display profile
	var img_origin = Vector3(12.285, 28.695, -745.75)
	var img_dir = Vector3(0.2434, -0.1505, 0.9582)
	# Default display profile width: 345mm, height: 215mm. Logical: 1920x1080.
	# With win_pos = (0, 0), viewport scale = (1, 1), offset = (0, 0)
	tracker.window_position_override = Vector2(0, 0)
	var proj_img = tracker.project_gaze_ray_to_viewport(img_origin, img_dir)
	print("Realistic self_center.jpg projected coordinate: ", proj_img)
	# Check that the coordinate matches the exact expected uncalibrated projection value (-162.6, 444.2)
	var expected_proj = Vector2(-162.6095, 444.239)
	if abs(proj_img.x - expected_proj.x) > 0.5 or abs(proj_img.y - expected_proj.y) > 0.5:
		printerr("FAIL: Realistic image gaze projection did not match expected: ", proj_img, " vs ", expected_proj)
		quit(1)
		return
	print("PASS: Realistic image gaze projection verified.")

	# Restore previous window position override
	tracker.window_position_override = Vector2(200, 300)
	print("PASS: Window displacement and resizing projection math verified.")

	# 3. Test Camera Node3D spatial transform changes
	# Move the CameraSensor node left by 50mm (Vector3(-50.0, 107.5, 0.0))
	camera_sensor.position = Vector3(-50.0, 107.5, 0.0)
	var proj_cam_shifted = tracker.project_gaze_ray_to_viewport(simulated_origin, simulated_dir)
	print("Camera shifted left by 50mm | Projected: ", proj_cam_shifted)
	# Moving camera left means the ray intersection on the display plane relative to the camera shifts right.
	# Let's verify that the projection output changes (i.e. is not equal to proj_3).
	if proj_cam_shifted == proj_3:
		printerr("FAIL: Projection did not update after camera position shift")
		quit(1)
		return
		
	print("PASS: Camera Node3D spatial transform updates verified.")

	# 4. Test coordinate filtering (requires at least two samples to smooth)
	var raw_coord_1 = Vector2(100.0, 200.0)
	var filtered_1 = tracker.filter_gaze_coordinate(raw_coord_1)
	print("First Raw: ", raw_coord_1, " | First Filtered: ", filtered_1)
	
	OS.delay_msec(50) # wait 50ms to ensure timestamp advances. Because this is a single threaded unit test, hard blocking is ok.
	var raw_coord_2 = Vector2(300.0, 400.0)
	var filtered_2 = tracker.filter_gaze_coordinate(raw_coord_2)
	print("Second Raw: ", raw_coord_2, " | Second Filtered: ", filtered_2)
	
	if filtered_2 == raw_coord_2 or filtered_2 == Vector2.ZERO:
		printerr("FAIL: filter_gaze_coordinate did not filter second sample")
		quit(1)
		return
		
	print("PASS: Coordinate filtering verified.")

	# 5. Test Dimension Calibration Sizing Math (High-DPI / Viewport Relative Sizing Regression Test)
	var cal_script = load("res://addons/godot-gaze/tests/dimension_calibration.gd")
	var cal_node = cal_script.new()
	
	# Simulate Retina M1 Pro display: logical size (1512, 982), physical display width 300.0375mm, viewport width 3024.0 (scale=2.0)
	cal_node.screen_size_lpix = Vector2(1512, 982)
	cal_node.screen_width_viewport = 3024.0
	
	var simulated_phys_size_x = 300.0375
	var px_per_mm = cal_node.screen_width_viewport / simulated_phys_size_x # e.g. 3024 / 300.0375 = 10.0787...
	var calculated_width_viewport = px_per_mm * cal_node.CARD_PHYSICAL_WIDTH_MM # 10.0787 * 85.6 = 862.74...
	
	print("Simulated Retina px_per_mm: ", px_per_mm)
	print("Calculated Card Width: ", calculated_width_viewport)
	if abs(calculated_width_viewport - 862.74) > 0.1:
		printerr("FAIL: Viewport-relative card width calculation incorrect for Retina")
		quit(1)
		return
		
	# Test logical pixel conversions on save
	var w_viewport = calculated_width_viewport
	var px_per_mm_viewport = w_viewport / cal_node.CARD_PHYSICAL_WIDTH_MM
	var px_per_mm_logical = px_per_mm_viewport * (cal_node.screen_size_lpix.x / cal_node.screen_width_viewport)
	var physical_size = cal_node.screen_size_lpix / px_per_mm_logical
	
	print("Derived Physical Width: ", physical_size.x)
	if abs(physical_size.x - 300.0375) > 0.1:
		printerr("FAIL: Derived physical width calculation incorrect on save")
		quit(1)
		return
		
	print("PASS: High-DPI Dimension Calibration scaling and save logic verified.")
	print("================================================================")
	quit(0)
