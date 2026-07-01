# Godot calibration challenge tests
extends SceneTree

func _init():
	print("=================== CALIBRATION CHALLENGE TESTS ===================")
	
	# -------------------------------------------------------------
	# 1. Verify serialization compatibility of StoredDeviceCalibration and StoredBioCalibration
	# -------------------------------------------------------------
	print("--- 1. Testing StoredDeviceCalibration & StoredBioCalibration Serialization ---")
	
	var dev_test_cases = [
		{
			"pixel_size_mm": Vector2(0.28, 0.28),
			"camera_offset": Vector3(12.5, -5.0, 20.0),
			"camera_tilt": -3.5
		},
		{
			"pixel_size_mm": Vector2(0.001, 0.001), # very small
			"camera_offset": Vector3(0.0, 0.0, 0.0),   # zero
			"camera_tilt": 0.0
		},
		{
			"pixel_size_mm": Vector2(10.0, 10.0), # large
			"camera_offset": Vector3(-500.0, 500.0, -1000.0), # negative / large
			"camera_tilt": 90.0
		}
	]
	
	for i in range(dev_test_cases.size()):
		var dev_path = "user://challenge_stored_device_" + str(i) + ".tres"
		var case = dev_test_cases[i]
		var dc = StoredDeviceCalibration.new()
		dc.pixel_size_mm = case["pixel_size_mm"]
		dc.camera_offset = case["camera_offset"]
		dc.camera_tilt = case["camera_tilt"]
		
		var err = ResourceSaver.save(dc, dev_path)
		if err != OK:
			printerr("FAIL: ResourceSaver.save failed for StoredDeviceCalibration case ", i, " with code: ", err)
			quit(1)
			return
			
		var loaded = ResourceLoader.load(dev_path, "", ResourceLoader.CACHE_MODE_REPLACE)
		if not loaded is StoredDeviceCalibration:
			printerr("FAIL: ResourceLoader.load returned wrong class for StoredDeviceCalibration case ", i)
			quit(1)
			return
			
		print("Loaded Device Case ", i, " values: pixel_size_mm=", loaded.pixel_size_mm, " camera_offset=", loaded.camera_offset, " camera_tilt=", loaded.camera_tilt)
		
		if loaded.pixel_size_mm != case["pixel_size_mm"]:
			printerr("FAIL: StoredDeviceCalibration case ", i, " pixel_size_mm mismatch: ", loaded.pixel_size_mm, " vs ", case["pixel_size_mm"])
			quit(1)
			return
			
		if loaded.camera_offset != case["camera_offset"]:
			printerr("FAIL: StoredDeviceCalibration case ", i, " camera_offset mismatch: ", loaded.camera_offset, " vs ", case["camera_offset"])
			quit(1)
			return
			
		if loaded.camera_tilt != case["camera_tilt"]:
			printerr("FAIL: StoredDeviceCalibration case ", i, " camera_tilt mismatch: ", loaded.camera_tilt, " vs ", case["camera_tilt"])
			quit(1)
			return
			
	print("PASS: StoredDeviceCalibration serialization verified.")
	
	var bio_test_cases = [
		{
			"bias_pitch": -0.15,
			"bias_yaw": 0.25,
			"scale_pitch": 0.95,
			"scale_yaw": 1.05
		},
		{
			"bias_pitch": 0.0,
			"bias_yaw": 0.0,
			"scale_pitch": 0.0001,
			"scale_yaw": 0.0001
		},
		{
			"bias_pitch": -1.5,
			"bias_yaw": 1.5,
			"scale_pitch": 2.5,
			"scale_yaw": 2.5
		}
	]
	
	for i in range(bio_test_cases.size()):
		var bio_path = "user://challenge_stored_bio_" + str(i) + ".tres"
		var case = bio_test_cases[i]
		var bc = StoredBioCalibration.new()
		bc.bias_pitch = case["bias_pitch"]
		bc.bias_yaw = case["bias_yaw"]
		bc.scale_pitch = case["scale_pitch"]
		bc.scale_yaw = case["scale_yaw"]
		
		var err = ResourceSaver.save(bc, bio_path)
		if err != OK:
			printerr("FAIL: ResourceSaver.save failed for StoredBioCalibration case ", i, " with code: ", err)
			quit(1)
			return
			
		var loaded = ResourceLoader.load(bio_path, "", ResourceLoader.CACHE_MODE_REPLACE)
		if not loaded is StoredBioCalibration:
			printerr("FAIL: ResourceLoader.load returned wrong class for StoredBioCalibration case ", i)
			quit(1)
			return
			
		print("Loaded Bio Case ", i, " values: bias_pitch=", loaded.bias_pitch, " bias_yaw=", loaded.bias_yaw, " scale_pitch=", loaded.scale_pitch, " scale_yaw=", loaded.scale_yaw)
		
		if loaded.bias_pitch != case["bias_pitch"] or loaded.bias_yaw != case["bias_yaw"] or loaded.scale_pitch != case["scale_pitch"] or loaded.scale_yaw != case["scale_yaw"]:
			printerr("FAIL: StoredBioCalibration case ", i, " values mismatch")
			quit(1)
			return
			
	print("PASS: StoredBioCalibration serialization verified.")
	
	# -------------------------------------------------------------
	# 2. Verify lazy-loading and fallback behavior of DefaultDeviceCalibration & DefaultBioCalibration
	# -------------------------------------------------------------
	print("--- 2. Testing Lazy-Loading & Fallback Behavior ---")
	
	var nonexistent_dev_path = "user://nonexistent_dev_calib.tres"
	var nonexistent_bio_path = "user://nonexistent_bio_calib.tres"
	
	# Clean up any potential files
	var dir = DirAccess.open("user://")
	if dir:
		if dir.file_exists("nonexistent_dev_calib.tres"):
			dir.remove("nonexistent_dev_calib.tres")
		if dir.file_exists("nonexistent_bio_calib.tres"):
			dir.remove("nonexistent_bio_calib.tres")
			
	# Set settings to nonexistent paths
	ProjectSettings.set_setting("gaze/calibration/device_calibration_path", nonexistent_dev_path)
	ProjectSettings.set_setting("gaze/calibration/bio_calibration_path", nonexistent_bio_path)
	
	# Instantiate default calibrations
	var default_dc = DefaultDeviceCalibration.new()
	var default_bc = DefaultBioCalibration.new()
	
	# Verify fallback behavior (files do not exist, so they should return guess/default values)
	# For DefaultDeviceCalibration, fallback GuessDeviceCalibration should return camera_offset (0, 148, 0)
	var fallback_offset = default_dc.camera_offset
	if fallback_offset != Vector3(0.0, 148.0, 0.0):
		printerr("FAIL: DefaultDeviceCalibration did not fall back to GuessDeviceCalibration when file was missing. Offset: ", fallback_offset)
		quit(1)
		return
		
	# For DefaultBioCalibration, fallback GuessBioCalibration should return bias_pitch 0.0
	var fallback_pitch = default_bc.bias_pitch
	if fallback_pitch != 0.0:
		printerr("FAIL: DefaultBioCalibration did not fall back to GuessBioCalibration when file was missing. Pitch: ", fallback_pitch)
		quit(1)
		return
		
	print("PASS: Fallback to Guess calibration verified.")
	
	# Create valid stored files at different paths and point ProjectSettings to them
	var lazy_dev_path = "user://lazy_device_calib.tres"
	var lazy_bio_path = "user://lazy_bio_calib.tres"
	
	var stored_dc = StoredDeviceCalibration.new()
	stored_dc.camera_offset = Vector3(99.0, 99.0, 99.0)
	ResourceSaver.save(stored_dc, lazy_dev_path)
	
	var stored_bc = StoredBioCalibration.new()
	stored_bc.bias_pitch = 9.9
	ResourceSaver.save(stored_bc, lazy_bio_path)
	
	# Update settings
	ProjectSettings.set_setting("gaze/calibration/device_calibration_path", lazy_dev_path)
	ProjectSettings.set_setting("gaze/calibration/bio_calibration_path", lazy_bio_path)
	
	# Instantiate new default calibrations to test lazy loading (so they aren't using previously cached delegates)
	var new_default_dc = DefaultDeviceCalibration.new()
	var new_default_bc = DefaultBioCalibration.new()
	
	# Access properties to trigger lazy loading
	var loaded_offset = new_default_dc.camera_offset
	var loaded_pitch = new_default_bc.bias_pitch
	
	if loaded_offset != Vector3(99.0, 99.0, 99.0):
		printerr("FAIL: DefaultDeviceCalibration did not lazy-load from Project Settings path. Offset: ", loaded_offset)
		quit(1)
		return
		
	if loaded_pitch != 9.9:
		printerr("FAIL: DefaultBioCalibration did not lazy-load from Project Settings path. Pitch: ", loaded_pitch)
		quit(1)
		return
		
	print("PASS: Lazy-loading of stored files verified.")
	
	# -------------------------------------------------------------
	# 3. Verify delegate caching and state retention when setters are called
	# -------------------------------------------------------------
	print("--- 3. Testing Delegate Caching & State Retention ---")
	
	# Instantiate a new default device calibration
	var cached_dc = DefaultDeviceCalibration.new()
	# Set a value on it
	cached_dc.camera_tilt = 42.0
	
	# Create a file at the settings path with a different value
	var test_cache_dev_path = "user://test_cache_device_calib.tres"
	var file_dc = StoredDeviceCalibration.new()
	file_dc.camera_tilt = -100.0
	ResourceSaver.save(file_dc, test_cache_dev_path)
	
	# Change setting path to this new file
	ProjectSettings.set_setting("gaze/calibration/device_calibration_path", test_cache_dev_path)
	
	# Verify that cached_dc STILL returns 42.0 (i.e. did not reload from file and lose state)
	var cached_tilt = cached_dc.camera_tilt
	if cached_tilt != 42.0:
		printerr("FAIL: DefaultDeviceCalibration lost state or did not cache delegate. Tilt: ", cached_tilt)
		quit(1)
		return
		
	# Instantiate a new default bio calibration
	var cached_bc = DefaultBioCalibration.new()
	# Set a value on it
	cached_bc.bias_yaw = 0.88
	
	# Create a file at settings path with a different value
	var test_cache_bio_path = "user://test_cache_bio_calib.tres"
	var file_bc = StoredBioCalibration.new()
	file_bc.bias_yaw = -9.9
	ResourceSaver.save(file_bc, test_cache_bio_path)
	
	# Change setting path to this new file
	ProjectSettings.set_setting("gaze/calibration/bio_calibration_path", test_cache_bio_path)
	
	# Verify that cached_bc STILL returns 0.88
	var cached_yaw = cached_bc.bias_yaw
	if cached_yaw != 0.88:
		printerr("FAIL: DefaultBioCalibration lost state or did not cache delegate. Yaw: ", cached_yaw)
		quit(1)
		return
		
	print("PASS: Delegate caching and state retention verified.")
	
	# -------------------------------------------------------------
	# 4. Verify DefaultDeviceCalibration and DefaultBioCalibration signal bubbling
	# -------------------------------------------------------------
	print("--- 4. Testing Signal Bubbling on Direct Property Mutation ---")
	var bubble_dc = DefaultDeviceCalibration.new()
	var bubble_bc = DefaultBioCalibration.new()
	
	var dc_signals = {"count": 0}
	var bc_signals = {"count": 0}
	
	bubble_dc.connect("changed", func(): dc_signals["count"] += 1)
	bubble_bc.connect("changed", func(): bc_signals["count"] += 1)
	
	# Trigger direct mutations
	bubble_dc.camera_tilt = 12.34
	bubble_bc.bias_pitch = 0.55
	
	if dc_signals["count"] == 0:
		printerr("FAIL: DefaultDeviceCalibration did not emit 'changed' signal on direct property mutation")
		quit(1)
		return
		
	if bc_signals["count"] == 0:
		printerr("FAIL: DefaultBioCalibration did not emit 'changed' signal on direct property mutation")
		quit(1)
		return
		
	print("PASS: Signal bubbling on direct property mutation verified.")
	
	# -------------------------------------------------------------
	# 5. Testing Signal Bubbling from Underlying Delegate
	# -------------------------------------------------------------
	print("--- 5. Testing Signal Bubbling from Underlying Delegate ---")
	var delegate_dev_path = "user://delegate_dev_calib.tres"
	var delegate_bio_path = "user://delegate_bio_calib.tres"
	
	var stored_dev_res = StoredDeviceCalibration.new()
	stored_dev_res.camera_tilt = 1.0
	ResourceSaver.save(stored_dev_res, delegate_dev_path)
	
	var stored_bio_res = StoredBioCalibration.new()
	stored_bio_res.bias_pitch = 0.1
	ResourceSaver.save(stored_bio_res, delegate_bio_path)
	
	# Set settings paths
	ProjectSettings.set_setting("gaze/calibration/device_calibration_path", delegate_dev_path)
	ProjectSettings.set_setting("gaze/calibration/bio_calibration_path", delegate_bio_path)
	
	var bubbling_dc = DefaultDeviceCalibration.new()
	var bubbling_bc = DefaultBioCalibration.new()
	
	# Trigger lazy-load so delegates are cached
	var _t1 = bubbling_dc.camera_tilt
	var _p1 = bubbling_bc.bias_pitch
	
	var dc_delegate_signals = {"count": 0}
	var bc_delegate_signals = {"count": 0}
	
	bubbling_dc.connect("changed", func(): dc_delegate_signals["count"] += 1)
	bubbling_bc.connect("changed", func(): bc_delegate_signals["count"] += 1)
	
	# Load the same files using ResourceLoader (which returns cached instances because of Godot's resource cache)
	var loaded_dev_delegate = ResourceLoader.load(delegate_dev_path)
	var loaded_bio_delegate = ResourceLoader.load(delegate_bio_path)
	
	# Mutate the delegates directly
	loaded_dev_delegate.camera_tilt = 5.0
	loaded_bio_delegate.bias_pitch = 0.5
	
	if dc_delegate_signals["count"] == 0:
		printerr("FAIL: DefaultDeviceCalibration did not bubble up 'changed' signal from underlying cached delegate")
		quit(1)
		return
		
	if bc_delegate_signals["count"] == 0:
		printerr("FAIL: DefaultBioCalibration did not bubble up 'changed' signal from underlying cached delegate")
		quit(1)
		return
		
	print("PASS: Signal bubbling from underlying cached delegate verified.")
	
	# -------------------------------------------------------------
	# 6. Verify clear_cache() and Signal Disconnection
	# -------------------------------------------------------------
	print("--- 6. Testing clear_cache() and Signal Disconnection ---")
	var path_file1 = "user://clear_cache_dc_file1.tres"
	var path_file2 = "user://clear_cache_dc_file2.tres"
	
	var dc_file1 = StoredDeviceCalibration.new()
	dc_file1.camera_tilt = 10.0
	ResourceSaver.save(dc_file1, path_file1)
	
	var dc_file2 = StoredDeviceCalibration.new()
	dc_file2.camera_tilt = 20.0
	ResourceSaver.save(dc_file2, path_file2)
	
	# Set setting path to file1
	ProjectSettings.set_setting("gaze/calibration/device_calibration_path", path_file1)
	
	var cc_dc = DefaultDeviceCalibration.new()
	# Access property to cache delegate
	var cc_tilt1 = cc_dc.camera_tilt
	if cc_tilt1 != 10.0:
		printerr("FAIL: Expected initial camera_tilt 10.0, got: ", cc_tilt1)
		quit(1)
		return
		
	# Setup signal monitoring
	var cc_signals = {"count": 0}
	cc_dc.connect("changed", func(): cc_signals["count"] += 1)
	
	# Mutate delegate 1, should trigger signal
	var active_delegate1 = ResourceLoader.load(path_file1)
	active_delegate1.camera_tilt = 15.0
	if cc_signals["count"] != 1:
		printerr("FAIL: Mutation of active delegate 1 did not trigger changed signal on cc_dc. Count: ", cc_signals["count"])
		quit(1)
		return
		
	# Clear cache
	cc_dc.clear_cache()
	# Verify that clear_cache itself emits changed (the C++ code does emit_changed() in clear_cache())
	if cc_signals["count"] != 2:
		printerr("FAIL: clear_cache() did not emit changed signal. Count: ", cc_signals["count"])
		quit(1)
		return
		
	# Change setting path to file2
	ProjectSettings.set_setting("gaze/calibration/device_calibration_path", path_file2)
	
	# Access property to reload/cache delegate 2
	var cc_tilt2 = cc_dc.camera_tilt
	if cc_tilt2 != 20.0:
		printerr("FAIL: Expected camera_tilt 20.0 after cache clear and path change, got: ", cc_tilt2)
		quit(1)
		return
		
	# Mutate delegate 1 again. It should be disconnected.
	active_delegate1.camera_tilt = 18.0
	if cc_signals["count"] != 2:
		printerr("FAIL: Mutation of disconnected delegate 1 triggered changed signal on cc_dc after clear_cache. Count: ", cc_signals["count"])
		quit(1)
		return
		
	# Mutate delegate 2. It should be connected and trigger signal.
	var active_delegate2 = ResourceLoader.load(path_file2)
	active_delegate2.camera_tilt = 25.0
	if cc_signals["count"] != 3:
		printerr("FAIL: Mutation of newly loaded delegate 2 did not trigger changed signal on cc_dc. Count: ", cc_signals["count"])
		quit(1)
		return
		
	print("PASS: clear_cache() cache clearing, reloading, and signal disconnection verified.")
	
	# -------------------------------------------------------------
	# 7. Verify clear_cache() and Signal Disconnection for DefaultBioCalibration
	# -------------------------------------------------------------
	print("--- 7. Testing clear_cache() and Signal Disconnection for DefaultBioCalibration ---")
	var bio_path_file1 = "user://clear_cache_bc_file1.tres"
	var bio_path_file2 = "user://clear_cache_bc_file2.tres"
	
	var bc_file1 = StoredBioCalibration.new()
	bc_file1.bias_pitch = 1.0
	ResourceSaver.save(bc_file1, bio_path_file1)
	
	var bc_file2 = StoredBioCalibration.new()
	bc_file2.bias_pitch = 2.0
	ResourceSaver.save(bc_file2, bio_path_file2)
	
	# Set setting path to file1
	ProjectSettings.set_setting("gaze/calibration/bio_calibration_path", bio_path_file1)
	
	var cc_bc = DefaultBioCalibration.new()
	# Access property to cache delegate
	var cc_pitch1 = cc_bc.bias_pitch
	if cc_pitch1 != 1.0:
		printerr("FAIL: Expected initial bias_pitch 1.0, got: ", cc_pitch1)
		quit(1)
		return
		
	# Setup signal monitoring
	var cc_bc_signals = {"count": 0}
	cc_bc.connect("changed", func(): cc_bc_signals["count"] += 1)
	
	# Mutate delegate 1, should trigger signal
	var active_bc_delegate1 = ResourceLoader.load(bio_path_file1)
	active_bc_delegate1.bias_pitch = 1.5
	if cc_bc_signals["count"] != 1:
		printerr("FAIL: Mutation of active bio delegate 1 did not trigger changed signal. Count: ", cc_bc_signals["count"])
		quit(1)
		return
		
	# Clear cache
	cc_bc.clear_cache()
	if cc_bc_signals["count"] != 2:
		printerr("FAIL: clear_cache() did not emit changed signal for DefaultBioCalibration. Count: ", cc_bc_signals["count"])
		quit(1)
		return
		
	# Change setting path to file2
	ProjectSettings.set_setting("gaze/calibration/bio_calibration_path", bio_path_file2)
	
	# Access property to reload/cache delegate 2
	var cc_pitch2 = cc_bc.bias_pitch
	if cc_pitch2 != 2.0:
		printerr("FAIL: Expected bias_pitch 2.0 after cache clear and path change, got: ", cc_pitch2)
		quit(1)
		return
		
	# Mutate delegate 1 again. It should be disconnected.
	active_bc_delegate1.bias_pitch = 1.8
	if cc_bc_signals["count"] != 2:
		printerr("FAIL: Mutation of disconnected bio delegate 1 triggered changed signal after clear_cache. Count: ", cc_bc_signals["count"])
		quit(1)
		return
		
	# Mutate delegate 2. It should be connected and trigger signal.
	var active_bc_delegate2 = ResourceLoader.load(bio_path_file2)
	active_bc_delegate2.bias_pitch = 2.5
	if cc_bc_signals["count"] != 3:
		printerr("FAIL: Mutation of newly loaded bio delegate 2 did not trigger changed signal. Count: ", cc_bc_signals["count"])
		quit(1)
		return
		
	print("PASS: DefaultBioCalibration clear_cache() cache clearing, reloading, and signal disconnection verified.")
	
	print("==================================================================")
	print("ALL CALIBRATION CHALLENGE TESTS PASSED SUCCESSFULLY!")
	print("==================================================================")
	quit(0)
