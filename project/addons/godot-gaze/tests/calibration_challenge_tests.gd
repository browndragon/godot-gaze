# Godot GazeDeviceProfile unit and challenge tests
extends SceneTree

func _init():
	print("=================== GAZE DEVICE PROFILE CHALLENGE TESTS ===================")
	
	# -------------------------------------------------------------
	# 1. Verify serialization compatibility of GazeDeviceProfile
	# -------------------------------------------------------------
	print("--- 1. Testing GazeDeviceProfile Serialization & Deserialization ---")
	
	var dev_test_cases = [
		{
			"pixel_pitch_mm": Vector2(0.28, 0.28),
			"logical_size_px": Vector2i(1920, 1080),
			"camera_offset_mm": Vector3(12.5, -5.0, 20.0),
			"camera_roll_deg": -3.5,
			"camera_hfov_deg": 65.0
		},
		{
			"pixel_pitch_mm": Vector2(0.15, 0.15),
			"logical_size_px": Vector2i(2560, 1440),
			"camera_offset_mm": Vector3(0.0, 0.0, 0.0),
			"camera_roll_deg": 0.0,
			"camera_hfov_deg": 70.0
		},
		{
			"pixel_pitch_mm": Vector2(0.05, 0.05),
			"logical_size_px": Vector2i(3840, 2160),
			"camera_offset_mm": Vector3(-50.0, 150.0, 10.0),
			"camera_roll_deg": 90.0,
			"camera_hfov_deg": 80.0
		}
	]
	
	for i in range(dev_test_cases.size()):
		var dev_path = "user://challenge_gaze_profile_" + str(i) + ".tres"
		var case = dev_test_cases[i]
		var profile = GazeDeviceProfile.new()
		profile.set_pixel_pitch_mm(case["pixel_pitch_mm"])
		profile.set_logical_size_px(case["logical_size_px"])
		profile.set_camera_offset_mm(case["camera_offset_mm"])
		profile.set_camera_roll_deg(case["camera_roll_deg"])
		profile.set_camera_hfov_deg(case["camera_hfov_deg"])
		
		var err = ResourceSaver.save(profile, dev_path)
		if err != OK:
			printerr("FAIL: ResourceSaver.save failed for GazeDeviceProfile case ", i, " with code: ", err)
			quit(1)
			return
			
		var loaded = ResourceLoader.load(dev_path, "", ResourceLoader.CACHE_MODE_REPLACE)
		if not loaded is GazeDeviceProfile:
			printerr("FAIL: ResourceLoader.load returned wrong class for GazeDeviceProfile case ", i)
			quit(1)
			return
			
		print("Loaded Profile Case ", i, " values: pixel_pitch_mm=", loaded.pixel_pitch_mm, " logical_size_px=", loaded.logical_size_px, " camera_offset_mm=", loaded.camera_offset_mm, " camera_roll_deg=", loaded.camera_roll_deg, " camera_hfov_deg=", loaded.camera_hfov_deg)
		
		if (loaded.pixel_pitch_mm - case["pixel_pitch_mm"]).length() > 0.0001:
			printerr("FAIL: GazeDeviceProfile case ", i, " pixel_pitch_mm mismatch: ", loaded.pixel_pitch_mm, " vs ", case["pixel_pitch_mm"])
			quit(1)
			return
			
		if loaded.logical_size_px != case["logical_size_px"]:
			printerr("FAIL: GazeDeviceProfile case ", i, " logical_size_px mismatch: ", loaded.logical_size_px, " vs ", case["logical_size_px"])
			quit(1)
			return
			
		if (loaded.camera_offset_mm - case["camera_offset_mm"]).length() > 0.0001:
			printerr("FAIL: GazeDeviceProfile case ", i, " camera_offset_mm mismatch: ", loaded.camera_offset_mm, " vs ", case["camera_offset_mm"])
			quit(1)
			return
			
		if abs(loaded.camera_roll_deg - case["camera_roll_deg"]) > 0.0001:
			printerr("FAIL: GazeDeviceProfile case ", i, " camera_roll_deg mismatch: ", loaded.camera_roll_deg, " vs ", case["camera_roll_deg"])
			quit(1)
			return
			
		if abs(loaded.camera_hfov_deg - case["camera_hfov_deg"]) > 0.0001:
			printerr("FAIL: GazeDeviceProfile case ", i, " camera_hfov_deg mismatch: ", loaded.camera_hfov_deg, " vs ", case["camera_hfov_deg"])
			quit(1)
			return
			
	print("PASS: GazeDeviceProfile serialization verified.")
	
	# -------------------------------------------------------------
	# 2. Verify Derived Properties & Mutators
	# -------------------------------------------------------------
	print("--- 2. Testing Derived Properties & Mutators ---")
	var p2 = GazeDeviceProfile.new()
	p2.set_logical_size_px(Vector2i(1000, 500))
	p2.set_pixel_pitch_mm(Vector2(0.5, 0.5))
	
	var phys = p2.get_physical_size_mm()
	if abs(phys.x - 500.0) > 0.01 or abs(phys.y - 250.0) > 0.01:
		printerr("FAIL: Expected physical size (500, 250), got ", phys)
		quit(1)
		return
		
	var dpi = p2.get_dpi()
	if abs(dpi.x - 50.8) > 0.1: # 25.4 / 0.5 = 50.8
		printerr("FAIL: Expected DPI ~50.8, got ", dpi)
		quit(1)
		return
		
	# Test set_physical_size_mm modifies pitch
	p2.set_physical_size_mm(Vector2(200.0, 100.0))
	var updated_pitch = p2.get_pixel_pitch_mm()
	if abs(updated_pitch.x - 0.2) > 0.001 or abs(updated_pitch.y - 0.2) > 0.001:
		printerr("FAIL: Expected updated pixel pitch (0.2, 0.2), got ", updated_pitch)
		quit(1)
		return
		
	print("PASS: Derived properties and mutators verified.")
	
	# -------------------------------------------------------------
	# 3. Verify Card Calibration Helper
	# -------------------------------------------------------------
	print("--- 3. Testing Card Calibration Helper ---")
	var p3 = GazeDeviceProfile.new()
	# If card is 85.603 mm and spans 400 logical pixels
	p3.calibrate_from_card_width(400.0, 85.603)
	var expected_pitch = 85.603 / 400.0
	if abs(p3.get_pixel_pitch_mm().x - expected_pitch) > 0.0001:
		printerr("FAIL: Expected card calibrated pitch ", expected_pitch, ", got ", p3.get_pixel_pitch_mm())
		quit(1)
		return
	print("PASS: Card calibration verified.")
	
	# -------------------------------------------------------------
	# 4. Verify Signal Emission on Property Mutation
	# -------------------------------------------------------------
	print("--- 4. Testing Signal Emission on Property Mutation ---")
	var p4 = GazeDeviceProfile.new()
	var sig_info = {"count": 0}
	p4.connect("changed", func(): sig_info["count"] += 1)
	
	p4.set_pixel_pitch_mm(Vector2(0.3, 0.3))
	p4.set_logical_size_px(Vector2i(1920, 1080))
	p4.set_camera_offset_mm(Vector3(1, 2, 3))
	p4.set_camera_roll_deg(5.0)
	p4.set_camera_hfov_deg(70.0)
	p4.set_physical_size_mm(Vector2(400, 300))
	p4.calibrate_from_card_width(350.0)
	
	if sig_info["count"] != 7:
		printerr("FAIL: Expected 7 changed signals, got: ", sig_info["count"])
		quit(1)
		return
	print("PASS: Signal emission on mutations verified.")
	
	# -------------------------------------------------------------
	# 5. Verify Static Helpers
	# -------------------------------------------------------------
	print("--- 5. Testing Static Math Helpers ---")
	var f_scaled = GazeDeviceProfile.get_focal_length_under_scaling(1000.0, 100.0, 200.0)
	if abs(f_scaled - 2000.0) > 0.01:
		printerr("FAIL: get_focal_length_under_scaling returned ", f_scaled)
		quit(1)
		return
		
	var card_px = GazeDeviceProfile.get_card_width_px(65.0, 500.0, 1280.0, 85.603)
	if card_px <= 0.0:
		printerr("FAIL: get_card_width_px returned ", card_px)
		quit(1)
		return
		
	var hfov = GazeDeviceProfile.diagonal_to_horizontal_fov(78.0, 16.0, 9.0)
	if hfov <= 0.0 or hfov >= 78.0:
		printerr("FAIL: diagonal_to_horizontal_fov returned ", hfov)
		quit(1)
		return
		
	# -------------------------------------------------------------
	# 6. Verify System Guess Instantiation
	# -------------------------------------------------------------
	print("--- 6. Testing System Guess ---")
	var guess = GazeDeviceProfile.create_system_guess()
	if not guess or not guess is GazeDeviceProfile:
		printerr("FAIL: create_system_guess failed to return GazeDeviceProfile instance")
		quit(1)
		return
		
	if guess.get_logical_size_px().x <= 0 or guess.get_physical_size_mm().x <= 0.0:
		printerr("FAIL: create_system_guess returned invalid dimensions: logical=", guess.get_logical_size_px(), " physical=", guess.get_physical_size_mm())
		quit(1)
		return
		
	print("PASS: System guess verified: logical=", guess.get_logical_size_px(), " physical=", guess.get_physical_size_mm())
	
	print("==================================================================")
	print("ALL GAZE DEVICE PROFILE CHALLENGE TESTS PASSED SUCCESSFULLY!")
	print("==================================================================")
	quit(0)
