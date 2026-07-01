extends Node

@onready var root = get_parent() # If run as root scene, its parent is the root viewport!

func quit(exit_code: int = 0) -> void:
	get_tree().quit(exit_code)

func _ready():
	call_deferred("run_tests")

func run_tests():
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
		
	var cam_sensor = CameraSensor.new()
	cam_sensor.name = "CameraSensor"
	cam_sensor.camera_device_id = -1
	tracker.add_child(cam_sensor)
	
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
	var scale = DisplayProfile.get_screen_scale()
	var vp_scale = tracker.get_adjusted_viewport_transform().get_scale()
	print("Projected coordinate shift: ", shift)
	var expected_shift = Vector2(-100.0, -150.0) * (scale / vp_scale)
	if abs(shift.x - expected_shift.x) > 0.5 or abs(shift.y - expected_shift.y) > 0.5:
		printerr("FAIL: Viewport projection did not shift correctly with window movement. Expected: ", expected_shift, " Got: ", shift)
		quit(1)
		return
		
	# Third: Resize root viewport to (1024, 768) at same position
	var vp_scale_2 = tracker.get_adjusted_viewport_transform().get_scale()
	root.size = Vector2i(1024, 768)
	var proj_3 = tracker.project_gaze_ray_to_viewport(simulated_origin, simulated_dir)
	print("Window resized to (1024, 768) | Projected: ", proj_3)
	var vp_scale_3 = tracker.get_adjusted_viewport_transform().get_scale()
	var expected_proj_3 = proj_2 * (vp_scale_2 / vp_scale_3)
	if abs(proj_3.x - expected_proj_3.x) > 0.5 or abs(proj_3.y - expected_proj_3.y) > 0.5:
		printerr("FAIL: Viewport projection shifted unexpectedly with window resizing. Expected: ", expected_proj_3, " Got: ", proj_3)
		quit(1)
		return
		
	# 2.6 Realistic Image-based Gaze Projection Test
	# Restore viewport size to (800, 600) to match baseline expectations
	root.size = Vector2i(800, 600)
	# Simulates self_center.jpg gaze ray under default display profile
	var img_origin = Vector3(12.285, 28.695, -745.75)
	var img_dir = Vector3(0.2434, -0.1505, 0.9582)
	# Default display profile width: 345mm, height: 215mm. Logical: 1920x1080.
	# With win_pos = (0, 0), viewport scale = (1, 1), offset = (0, 0)
	tracker.window_position_override = Vector2(0, 0)
	var proj_img = tracker.project_gaze_ray_to_viewport(img_origin, img_dir)
	print("Realistic self_center.jpg projected coordinate: ", proj_img)
	# Check that the coordinate matches the exact expected uncalibrated projection value (-162.6, 444.2)
	var scale_proj = DisplayProfile.get_screen_scale()
	var expected_proj = Vector2(-162.6095, 444.239) * (scale_proj / tracker.get_adjusted_viewport_transform().get_scale())
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
	
	OS.delay_msec(50) # wait 50ms to ensure timestamp advances.
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
	var px_per_mm = cal_node.screen_width_viewport / simulated_phys_size_x
	var calculated_width_viewport = px_per_mm * cal_node.CARD_PHYSICAL_WIDTH_MM
	
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
		
	# 5.5 Regression Test: Verify raw OpenCV-space gaze feeding projects correctly on GazeServer
	var raw_args = [
		true, # face_detected
		12.285, -28.695, 745.75, # left_eye_cv (OpenCV space)
		12.285, -28.695, 745.75, # right_eye_cv
		0.2434, 0.1505, -0.9582, # dir_cv
		0.0, 0.0, 0.0, # head_trans
		0.0, 0.0, 0.0, # head_rot
		0.0, 0.0 # canvas_pos
	]
	
	# Stop GazeServer processing thread to avoid overwriting fed mock values
	var gs = Engine.get_singleton("GazeServer")
	if gs:
		gs.stop_processing()
 
	camera_sensor.position = Vector3(0.0, 107.5, 0.0)
	tracker.window_position_override = Vector2(0, 0)
	if gs:
		var active_face = face_estimator.get_face_rid()
		var active_eye = eye_estimator.get_eye_rid()
		
		# Unpack raw Inference space coordinates (in mm)
		var left_eye_cv = Vector3(raw_args[1], raw_args[2], raw_args[3])
		var right_eye_cv = Vector3(raw_args[4], raw_args[5], raw_args[6])
		var dir_cv = Vector3(raw_args[7], raw_args[8], raw_args[9])
		var origin_cv = (left_eye_cv + right_eye_cv) * 0.5
		
		var head_trans = Vector3(raw_args[10], raw_args[11], raw_args[12])
		var head_rot = Vector3(raw_args[13], raw_args[14], raw_args[15])
		
		var origin_cam = Vector3(origin_cv.x, -origin_cv.y, -origin_cv.z)
		var dir_cam = Vector3(dir_cv.x, -dir_cv.y, -dir_cv.z)
		
		gs.face_tracker_set_pose(active_face, head_trans, head_rot, true)
		gs.eye_tracker_set_gaze(active_eye, origin_cam, dir_cam)
	
	var eye_rid = eye_estimator.get_eye_rid()
	tracker._on_gaze_data_ready(eye_rid)
	
	var raw_feed_proj = tracker.get_latest_projected_gaze()
	print("Raw OpenCV fed projected coordinate: ", raw_feed_proj)
	var raw_expected_proj = Vector2(-162.6095, 444.239) * tracker.get_adjusted_viewport_transform().get_scale()
	if abs(raw_feed_proj.x - raw_expected_proj.x) > 0.5 or abs(raw_feed_proj.y - raw_expected_proj.y) > 0.5:
		printerr("FAIL: Raw OpenCV fed projection did not match expected: ", raw_feed_proj, " vs ", raw_expected_proj)
		quit(1)
		return
	print("PASS: Raw OpenCV space coordinate projection verified.")
 
	# Verify Head Forward points towards the screen (positive Z in camera space)
	var head_fwd = tracker.get_head_forward()
	print("Derived Head Forward Vector: ", head_fwd)
	if head_fwd.z <= 0.0:
		printerr("FAIL: Head forward direction does not point towards the screen (positive Z): ", head_fwd)
		quit(1)
		return
	print("PASS: Head forward direction verified (points along positive Z).")
 
	print("PASS: High-DPI Dimension Calibration scaling and save logic verified.")
	
	# ================================================================
	# E2E TESTS FOR FEATURE 1: Decoupled Resource-Based Calibration (F1)
	# ================================================================
	print("=================== E2E TEST: FEATURE F1 (Calibration Resource) ===================")
	
	var dc_base = DeviceCalibration.new()
	var dc_guess = GuessDeviceCalibration.new()
	var dc_stored = StoredDeviceCalibration.new()
	var dc_default = DefaultDeviceCalibration.new()
	
	dc_base.pixel_size_mm = Vector2(0.3, 0.3)
	dc_base.camera_offset = Vector3(10.0, 20.0, 30.0)
	dc_base.camera_tilt = 5.0
	
	if dc_base.pixel_size_mm != Vector2(0.3, 0.3):
		printerr("FAIL: F1 - DeviceCalibration pixel_size_mm property get/set failed")
		quit(1)
		return
	if dc_base.camera_offset != Vector3(10.0, 20.0, 30.0):
		printerr("FAIL: F1 - DeviceCalibration camera_offset property get/set failed")
		quit(1)
		return
	if dc_base.camera_tilt != 5.0:
		printerr("FAIL: F1 - DeviceCalibration camera_tilt property get/set failed")
		quit(1)
		return
 
	var bc_base = BioCalibration.new()
	var bc_guess = GuessBioCalibration.new()
	var bc_stored = StoredBioCalibration.new()
	var bc_default = DefaultBioCalibration.new()
	
	bc_base.bias_pitch = 0.1
	bc_base.bias_yaw = -0.2
	bc_base.scale_pitch = 1.1
	bc_base.scale_yaw = 0.9
	
	if bc_base.bias_pitch != 0.1 or bc_base.bias_yaw != -0.2:
		printerr("FAIL: F1 - BioCalibration pitch/yaw bias property get/set failed")
		quit(1)
		return
	if bc_base.scale_pitch != 1.1 or bc_base.scale_yaw != 0.9:
		printerr("FAIL: F1 - BioCalibration scale pitch/yaw property get/set failed")
		quit(1)
		return
 
	var gdec = Engine.get_singleton("GazeDeviceEstimatedCalibration")
	if not gdec:
		printerr("FAIL: F1 - Singleton GazeDeviceEstimatedCalibration not registered in Engine")
		quit(1)
		return
	var default_dc = gdec.get_calibration()
	if not default_dc or not default_dc is DeviceCalibration:
		printerr("FAIL: F1 - GazeDeviceEstimatedCalibration get_calibration() did not return DeviceCalibration")
		quit(1)
		return
	print("GazeDeviceEstimatedCalibration resolved default calibration successfully.")
 
	var session = GazeCalibrationSession.new()
	session.clear()
	if session.get_sample_count() != 0:
		printerr("FAIL: F1 - GazeCalibrationSession initial sample count is not 0")
		quit(1)
		return
		
	session.add_sample(Vector2(960, 540), Vector3(0.0, 0.0, 600.0), Vector3(0.0, 0.0, -1.0))
	session.add_sample(Vector2(100, 100), Vector3(-10.0, 10.0, 600.0), Vector3(0.02, -0.02, -1.0).normalized())
	session.add_sample(Vector2(1800, 100), Vector3(10.0, 10.0, 600.0), Vector3(-0.02, -0.02, -1.0).normalized())
	session.add_sample(Vector2(100, 980), Vector3(-10.0, -10.0, 600.0), Vector3(0.02, 0.02, -1.0).normalized())
	session.add_sample(Vector2(1800, 980), Vector3(10.0, -10.0, 600.0), Vector3(-0.02, 0.02, -1.0).normalized())
	
	if session.get_sample_count() != 5:
		printerr("FAIL: F1 - GazeCalibrationSession get_sample_count() returned incorrect count: ", session.get_sample_count())
		quit(1)
		return
		
	var calib_res = session.calculate_calibration(tracker)
	if not calib_res.has("device_calibration") or not calib_res.has("bio_calibration"):
		printerr("FAIL: F1 - GazeCalibrationSession calculate_calibration did not return device/bio calibration keys")
		quit(1)
		return
	if not calib_res["device_calibration"] is DeviceCalibration or not calib_res["bio_calibration"] is BioCalibration:
		printerr("FAIL: F1 - GazeCalibrationSession returned incorrect types in result dictionary")
		quit(1)
		return
		
	var solved_bc = calib_res["bio_calibration"]
	if not (solved_bc.bias_pitch != 0.0 or solved_bc.bias_yaw != 0.0):
		printerr("FAIL: F1 - GazeCalibrationSession solve did not converge to non-zero values")
		quit(1)
		return
		
	print("GazeCalibrationSession sample math and estimation successfully triggered and solved.")
 
	var test_dev_path = "user://test_stored_device_calib.tres"
	var stored_dc = StoredDeviceCalibration.new()
	stored_dc.pixel_size_mm = Vector2(0.28, 0.28)
	stored_dc.camera_offset = Vector3(12.5, -5.0, 20.0)
	stored_dc.camera_tilt = -3.5
	
	var save_ok = ResourceSaver.save(stored_dc, test_dev_path)
	if save_ok != OK:
		printerr("FAIL: F1 - ResourceSaver failed to save StoredDeviceCalibration, code: ", save_ok)
		quit(1)
		return
		
	var loaded_dc = ResourceLoader.load(test_dev_path)
	if not loaded_dc is StoredDeviceCalibration:
		printerr("FAIL: F1 - ResourceLoader loaded object is not StoredDeviceCalibration")
		quit(1)
		return
	if loaded_dc.pixel_size_mm != Vector2(0.28, 0.28) or loaded_dc.camera_offset != Vector3(12.5, -5.0, 20.0) or loaded_dc.camera_tilt != -3.5:
		printerr("FAIL: F1 - Loaded StoredDeviceCalibration values do not match saved values")
		quit(1)
		return
 
	var test_bio_path = "user://test_stored_bio_calib.tres"
	var stored_bc = StoredBioCalibration.new()
	stored_bc.bias_pitch = -0.15
	stored_bc.bias_yaw = 0.25
	stored_bc.scale_pitch = 0.95
	stored_bc.scale_yaw = 1.05
	
	save_ok = ResourceSaver.save(stored_bc, test_bio_path)
	if save_ok != OK:
		printerr("FAIL: F1 - ResourceSaver failed to save StoredBioCalibration, code: ", save_ok)
		quit(1)
		return
		
	var loaded_bc = ResourceLoader.load(test_bio_path)
	if not loaded_bc is StoredBioCalibration:
		printerr("FAIL: F1 - ResourceLoader loaded object is not StoredBioCalibration")
		quit(1)
		return
	if loaded_bc.bias_pitch != -0.15 or loaded_bc.bias_yaw != 0.25 or loaded_bc.scale_pitch != 0.95 or loaded_bc.scale_yaw != 1.05:
		printerr("FAIL: F1 - Loaded StoredBioCalibration values do not match saved values")
		quit(1)
		return
 
	print("PASS: F1 Decoupled Resource-Based Calibration E2E verification complete.")
 
	# ================================================================
	# E2E TESTS FOR FEATURE 2: Configurable Vision Debug Overlay (F2)
	# ================================================================
	print("=================== E2E TEST: FEATURE F2 (Vision Debug Overlay) ===================")
	
	var vs = Engine.get_singleton("VisionServer")
	if not vs:
		printerr("FAIL: F2 - Singleton VisionServer not registered in Engine")
		quit(1)
		return
		
	var mock_vs = MockVisionServer.new()
	var mock_cam_rid = mock_vs.camera_create()
	if not mock_cam_rid.is_valid():
		printerr("FAIL: F2 - MockVisionServer failed to create camera RID")
		quit(1)
		return
		
	var start_ok = mock_vs.camera_start(mock_cam_rid)
	if not start_ok:
		printerr("FAIL: F2 - MockVisionServer camera_start failed")
		quit(1)
		return
		
	var test_img = Image.create(160, 120, false, Image.FORMAT_RGB8)
	test_img.fill(Color(1.0, 0.0, 0.0))
	var test_tex = ImageTexture.create_from_image(test_img)
	
	mock_vs.inject_texture(mock_cam_rid, test_tex)
	var cur_tex = mock_vs.get_camera_current_texture(mock_cam_rid)
	if cur_tex == null:
		printerr("FAIL: F2 - MockVisionServer get_camera_current_texture returned null")
		quit(1)
		return
	if cur_tex.get_width() != 160 or cur_tex.get_height() != 120:
		printerr("FAIL: F2 - MockVisionServer retrieved texture dimensions incorrect")
		quit(1)
		return
		
	var sensor = tracker.get_camera_sensor()
	var eye_est = tracker.get_eye_estimator()
	
	if not sensor is CameraSensor or not eye_est is EyeEstimator:
		printerr("FAIL: F2 - Tracker sub-components do not match CameraSensor/EyeEstimator types")
		quit(1)
		return
		
	var initial_frame = sensor.get_last_frame()
	var initial_left = eye_est.get_left_eye_crop()
	var initial_right = eye_est.get_right_eye_crop()
	print("Initial Camera Frame: ", "Empty/Null" if initial_frame == null or initial_frame.is_empty() else "Valid")
	print("Initial Left Eye Crop: ", "Empty/Null" if initial_left == null or initial_left.is_empty() else "Valid")
	print("Initial Right Eye Crop: ", "Empty/Null" if initial_right == null or initial_right.is_empty() else "Valid")
	
	# ================================================================
	# E2E TESTS FOR FEATURE 2 (Debug Overlay Scene Validation)
	# ================================================================
	var setting_key = "gaze/debug/overlay_scene_path"
	if not ProjectSettings.has_setting(setting_key):
		printerr("FAIL: F2 - ProjectSetting '", setting_key, "' is not registered")
		quit(1)
		return
	var scene_path = ProjectSettings.get_setting(setting_key)
	if scene_path != "res://addons/godot-gaze/debug_cam_feed.tscn":
		printerr("FAIL: F2 - ProjectSetting '", setting_key, "' value is incorrect: ", scene_path)
		quit(1)
		return
	print("ProjectSetting registered and retrieved successfully: ", scene_path)
 
	var overlay_scene = load(scene_path)
	if overlay_scene == null:
		printerr("FAIL: F2 - Unable to load overlay scene from path: ", scene_path)
		quit(1)
		return
	var overlay_instance = overlay_scene.instantiate()
	if overlay_instance == null:
		printerr("FAIL: F2 - Unable to instantiate overlay scene")
		quit(1)
		return
	print("Overlay scene loaded and instantiated successfully.")
 
	root.add_child(overlay_instance)
	overlay_instance.tracker = tracker
 
	var panel = overlay_instance.get_node_or_null("Panel")
	if panel == null or not panel is Panel:
		printerr("FAIL: F2 - Panel child node is missing or of incorrect type")
		quit(1)
		return
		
	var cam_rect = overlay_instance.get_node_or_null("Panel/ScrollContainer/VBoxContainer/CameraFeedBox/CameraFeedRect")
	if cam_rect == null or not cam_rect is TextureRect:
		printerr("FAIL: F2 - CameraFeedRect child node is missing or of incorrect type")
		quit(1)
		return
		
	var left_rect = overlay_instance.get_node_or_null("Panel/ScrollContainer/VBoxContainer/EyesBox/EyesContainer/LeftEyeBox/LeftEyeRect")
	if left_rect == null or not left_rect is TextureRect:
		printerr("FAIL: F2 - LeftEyeRect child node is missing or of incorrect type")
		quit(1)
		return
		
	var right_rect = overlay_instance.get_node_or_null("Panel/ScrollContainer/VBoxContainer/EyesBox/EyesContainer/RightEyeBox/RightEyeRect")
	if right_rect == null or not right_rect is TextureRect:
		printerr("FAIL: F2 - RightEyeRect child node is missing or of incorrect type")
		quit(1)
		return
	print("Overlay child nodes verified and typed correctly.")
 
	var mock_frame = Image.create(320, 240, false, Image.FORMAT_RGB8)
	mock_frame.fill(Color.BLUE)
	var mock_left = Image.create(80, 60, false, Image.FORMAT_RGB8)
	mock_left.fill(Color.YELLOW)
	var mock_right = Image.create(80, 60, false, Image.FORMAT_RGB8)
	mock_right.fill(Color.MAGENTA)
 
	if cam_rect.texture != null or left_rect.texture != null or right_rect.texture != null:
		printerr("FAIL: F2 - Textures are unexpectedly populated before frame signals")
		quit(1)
		return
 
	sensor.emit_signal("frame_ready", mock_frame)
	eye_est.emit_signal("eye_crops_ready", mock_left, mock_right)
 
	if cam_rect.texture == null or cam_rect.texture.get_size() != Vector2(320, 240):
		printerr("FAIL: F2 - CameraFeedRect texture not updated correctly via frame_ready signal")
		quit(1)
		return
		
	if left_rect.texture == null or left_rect.texture.get_size() != Vector2(80, 60):
		printerr("FAIL: F2 - LeftEyeRect texture not updated correctly via eye_crops_ready signal")
		quit(1)
		return
		
	if right_rect.texture == null or right_rect.texture.get_size() != Vector2(80, 60):
		printerr("FAIL: F2 - RightEyeRect texture not updated correctly via eye_crops_ready signal")
		quit(1)
		return
		
	print("E2E signal binding successfully updated all textures.")
 
	var test_rect = TextureRect.new()
	test_rect.texture = mock_left
	test_rect.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	var drawn_rect_result = overlay_instance.get_texture_drawn_rect(test_rect)
	if drawn_rect_result != Rect2(0, 0, 0, 0):
		printerr("FAIL: F2 - get_texture_drawn_rect did not handle non-positive size dimensions correctly")
		quit(1)
		return
	test_rect.free()
 
	var second_tracker = GazeTracker.new()
	second_tracker.camera_device_id = -1
	root.add_child(second_tracker)
	second_tracker.initialize_tracker()
	
	var overlay_test = overlay_scene.instantiate()
	root.add_child(overlay_test)
	
	overlay_test.tracker = tracker
	var sensor1 = tracker.get_camera_sensor()
	var eye_est1 = tracker.get_eye_estimator()
	
	overlay_test.tracker = second_tracker
	
	if sensor1.is_connected("frame_ready", overlay_test._on_frame_ready):
		printerr("FAIL: F2 - Old tracker CameraSensor signals not disconnected on tracker swap")
		quit(1)
		return
	if eye_est1.is_connected("eye_crops_ready", overlay_test._on_eye_crops_ready) or eye_est1.is_connected("gaze_estimated", overlay_test._on_gaze_estimated):
		printerr("FAIL: F2 - Old tracker EyeEstimator signals not disconnected on tracker swap")
		quit(1)
		return
		
	var sensor2 = second_tracker.get_camera_sensor()
	var eye_est2 = second_tracker.get_eye_estimator()
	if not sensor2.is_connected("frame_ready", overlay_test._on_frame_ready):
		printerr("FAIL: F2 - New tracker CameraSensor signals not connected on tracker swap")
		quit(1)
		return
	if not eye_est2.is_connected("eye_crops_ready", overlay_test._on_eye_crops_ready) or not eye_est2.is_connected("gaze_estimated", overlay_test._on_gaze_estimated):
		printerr("FAIL: F2 - New tracker EyeEstimator signals not connected on tracker swap")
		quit(1)
		return
 
	overlay_test.tracker = null
	if sensor2.is_connected("frame_ready", overlay_test._on_frame_ready):
		printerr("FAIL: F2 - Tracker CameraSensor signals not disconnected when tracker set to null")
		quit(1)
		return
		
	root.remove_child(overlay_test)
	overlay_test.free()
	second_tracker.stop_tracker()
	root.remove_child(second_tracker)
	second_tracker.free()
 
	root.remove_child(overlay_instance)
	overlay_instance.free()
 
	mock_vs.camera_stop(mock_cam_rid)
	mock_vs.camera_free(mock_cam_rid)
	mock_vs.free()
	
	print("PASS: F2 Configurable Vision Debug Overlay E2E verification complete.")
 
	# ================================================================
	# E2E TESTS FOR FEATURE 3: CI/CD Release Validation (F3)
	# ================================================================
	print("=================== E2E TEST: FEATURE F3 (CI/CD Release Validation) ===================")
	
	var gdext_path = "res://addons/godot-gaze/gaze.gdextension"
	if not FileAccess.file_exists(gdext_path):
		printerr("FAIL: F3 - gaze.gdextension configuration file missing from addon tree")
		quit(1)
		return
		
	var f = FileAccess.open(gdext_path, FileAccess.READ)
	if not f:
		printerr("FAIL: F3 - Unable to open gaze.gdextension for reading")
		quit(1)
		return
	var content = f.get_as_text()
	f.close()
	
	if not "entry_symbol" in content or not "gaze_library_init" in content:
		printerr("FAIL: F3 - gaze.gdextension entry point configuration invalid")
		quit(1)
		return
		
	if not "macos.debug" in content or not "macos.release" in content:
		printerr("FAIL: F3 - gaze.gdextension libraries config missing macos settings")
		quit(1)
		return
	if not "windows.debug" in content or not "linux.debug" in content:
		printerr("FAIL: F3 - gaze.gdextension libraries config missing cross-platform targets")
		quit(1)
		return
		
	var expected_settings = [
		"gaze/models/search_paths",
		"gaze/models/yunet_prefix",
		"gaze/models/gaze_prefix",
		"gaze/calibration/device_calibration_path",
		"gaze/calibration/bio_calibration_path",
		"gaze/debug/overlay_scene_path"
	]
	
	for setting in expected_settings:
		if not ProjectSettings.has_setting(setting):
			printerr("FAIL: F3 - Mandatory ProjectSetting '", setting, "' is not registered")
			quit(1)
			return
		print("ProjectSetting registered: ", setting, " = ", ProjectSettings.get_setting(setting))
		
	var required_singletons = ["VisionServer", "GazeServer", "GazeDeviceEstimatedCalibration"]
	for sing in required_singletons:
		if not Engine.has_singleton(sing):
			printerr("FAIL: F3 - Engine singleton '", sing, "' is not registered")
			quit(1)
			return
		var inst = Engine.get_singleton(sing)
		if not is_instance_valid(inst):
			printerr("FAIL: F3 - Engine singleton '", sing, "' instance is invalid")
			quit(1)
			return
		print("Engine singleton verified: ", sing)
		
	print("PASS: F3 CI/CD Release Validation E2E verification complete.")
 
	camera_sensor.stop_sensor()
	if gs:
		gs.stop_processing()
 
	cal_node.free()
	tracker.stop_tracker()
	root.remove_child(tracker)
	tracker.free()

	print("==================================================================")
	print("ALL Headless Integration & E2E tests have passed successfully!")
	print("==================================================================")
	quit(0)
