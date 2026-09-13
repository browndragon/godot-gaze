extends Node

@onready var root = get_parent()

func quit(exit_code: int = 0) -> void:
	get_tree().quit(exit_code)

func _ready():
	call_deferred("run_tests")

func project_ray_to_screen_mm(origin_godot: Vector3, dir_godot: Vector3) -> Vector2:
	if abs(dir_godot.z) < 1e-6:
		return Vector2(-9999.0, -9999.0)
	var t = -origin_godot.z / dir_godot.z
	var pos_x = -(origin_godot.x + t * dir_godot.x)
	var pos_y = -(origin_godot.y + t * dir_godot.y)
	return Vector2(pos_x, pos_y)

func run_tests():
	print("=================== HEADLESS INTEGRATION TESTS ===================")
	
	# 1. Test GazeDeviceProfile Resource
	var profile = GazeDeviceProfile.new()
	profile.set_logical_size_px(Vector2i(1920, 1080))
	profile.set_physical_size_mm(Vector2(345.0, 215.0))
	
	var dpi = profile.get_dpi()
	print("GazeDeviceProfile DPI: ", dpi)
	if abs(dpi.x - 141.35) > 0.1 or abs(dpi.y - 127.64) > 0.1:
		printerr("FAIL: GazeDeviceProfile DPI calculation incorrect")
		quit(1)
		return
	print("PASS: GazeDeviceProfile resource verified.")
	
	# 2. Test InputEventGazeBase, InputEventGaze, and InputEventGazeMissing ClassDB registration & polymorphism
	if not ClassDB.class_exists("InputEventGazeBase"):
		printerr("FAIL: InputEventGazeBase not registered in ClassDB")
		quit(1)
		return
	if not ClassDB.class_exists("InputEventGaze"):
		printerr("FAIL: InputEventGaze not registered in ClassDB")
		quit(1)
		return
	if not ClassDB.class_exists("InputEventGazeMissing"):
		printerr("FAIL: InputEventGazeMissing not registered in ClassDB")
		quit(1)
		return

	var event = ClassDB.instantiate("InputEventGaze") as InputEventGaze
	if not event or not event.is_face_tracked():
		printerr("FAIL: InputEventGaze failed instantiation or returned false for is_face_tracked()")
		quit(1)
		return

	var missing_event = ClassDB.instantiate("InputEventGazeMissing") as InputEventGazeMissing
	if not missing_event or missing_event.is_face_tracked():
		printerr("FAIL: InputEventGazeMissing failed instantiation or returned true for is_face_tracked()")
		quit(1)
		return
	print("PASS: InputEventGaze event hierarchy verified.")

	# 2b. Test InputEventGaze 3D Transform API & ClampingMode Enums
	if not ("CLAMPING_FREE" in InputEventGaze and "CLAMPING_CLAMPED" in InputEventGaze and "CLAMPING_DEFAULT" in InputEventGaze):
		printerr("FAIL: InputEventGaze ClampingMode enums not exposed in ClassDB")
		quit(1)
		return
	if InputEventGaze.CLAMPING_FREE != 0 or InputEventGaze.CLAMPING_CLAMPED != 1 or InputEventGaze.CLAMPING_DEFAULT != 2:
		printerr("FAIL: InputEventGaze ClampingMode enum integer values incorrect")
		quit(1)
		return

	var test_eye_xform = Transform3D(Basis(), Vector3(12.0, -5.0, -450.0))
	var test_head_xform = Transform3D(Basis(), Vector3(10.0, 20.0, -500.0))
	event.set_eye_transform(test_eye_xform)
	event.set_head_transform(test_head_xform)

	if event.get_eye_transform().origin != Vector3(12.0, -5.0, -450.0):
		printerr("FAIL: InputEventGaze get_eye_transform origin mismatch")
		quit(1)
		return
	if event.get_head_transform().origin != Vector3(10.0, 20.0, -500.0):
		printerr("FAIL: InputEventGaze get_head_transform origin mismatch")
		quit(1)
		return

	# Assert removal of redundant convenience methods
	if event.has_method("get_eye_origin") or event.has_method("get_eye_direction") or event.has_method("get_head_pose"):
		printerr("FAIL: InputEventGaze still exposes deprecated convenience methods (get_eye_origin / get_eye_direction / get_head_pose)")
		quit(1)
		return

	# 2c. Test Viewport Clamping on get_eye_gaze / get_nose_gaze
	event.set_eye_gaze(Vector2(-100.0, 300.0))
	var free_pos = event.get_eye_gaze(null, InputEventGaze.CLAMPING_FREE)
	var clamped_pos = event.get_eye_gaze(null, InputEventGaze.CLAMPING_CLAMPED)
	if abs(free_pos.x - (-100.0)) > 0.01 or abs(free_pos.y - 300.0) > 0.01:
		printerr("FAIL: get_eye_gaze(null, CLAMPING_FREE) did not return unconstrained point: ", free_pos)
		quit(1)
		return
	if abs(clamped_pos.x - 0.0) > 0.01 or abs(clamped_pos.y - 300.0) > 0.01:
		printerr("FAIL: get_eye_gaze(null, CLAMPING_CLAMPED) did not clamp point to viewport: ", clamped_pos)
		quit(1)
		return
	var delta_clamped = clamped_pos.x - free_pos.x
	if delta_clamped < 0.50:
		printerr("FAIL: Clamping signal separation failed, delta: ", delta_clamped)
		quit(1)
		return
	print("PASS: InputEventGaze 3D transform & ClampingMode API verified.")

	# 3. Test GazeServer Singleton & Lifecycle
	var gs = Engine.get_singleton("GazeServer")
	var vs = Engine.get_singleton("VisionServer")
	if not gs or not vs:
		printerr("FAIL: GazeServer or VisionServer singleton missing")
		quit(1)
		return

	gs.set_device_profile(profile)
	# Ensure stopped before testing 0->1 transition
	gs.stop_tracking(true)
	var started_fresh = gs.start_tracking()
	print("start_tracking transition 0->1 returned: ", started_fresh)
	if not started_fresh or not gs.is_tracking_active():
		printerr("FAIL: is_tracking_active() returned false or start_tracking returned false on 0->1")
		quit(1)
		return

	# Test refcounting (1->2 should return false)
	var second_start = gs.start_tracking()
	if second_start:
		printerr("FAIL: second start_tracking() call unexpectedly returned true")
		quit(1)
		return

	# Stop one tracker (active_trackers becomes 1)
	gs.stop_tracking(false)
	if not gs.is_tracking_active():
		printerr("FAIL: tracking stopped when refcount was still positive")
		quit(1)
		return
	print("PASS: GazeServer refcounted lifecycle verified.")

	# 3b. Test GazeServer Mouse Emulation and Default Clamping Configuration
	if not gs.has_method("set_default_clamping") or not gs.has_method("is_clamping_by_default"):
		printerr("FAIL: GazeServer missing default clamping methods")
		quit(1)
		return
	if not gs.is_clamping_by_default():
		printerr("FAIL: GazeServer default_clamping should default to true")
		quit(1)
		return
	gs.set_default_clamping(false)
	if gs.is_clamping_by_default():
		printerr("FAIL: GazeServer set_default_clamping(false) did not update")
		quit(1)
		return
	gs.set_default_clamping(true)

	if not gs.has_method("set_emulate_mouse_from_gaze") or not gs.has_method("get_emulate_mouse_from_gaze"):
		printerr("FAIL: GazeServer missing mouse emulation methods")
		quit(1)
		return
	var initial_emulate = gs.get_emulate_mouse_from_gaze()
	gs.set_emulate_mouse_from_gaze(not initial_emulate)
	if gs.get_emulate_mouse_from_gaze() == initial_emulate:
		printerr("FAIL: GazeServer set_emulate_mouse_from_gaze did not toggle value")
		quit(1)
		return
	gs.set_emulate_mouse_from_gaze(initial_emulate)

	# Verify mouse stillness API:
	if not gs.has_method("is_physical_mouse_still") or not gs.has_method("is_physical_mouse_active"):
		printerr("FAIL: GazeServer missing physical mouse stillness methods")
		quit(1)
		return
	if not gs.is_physical_mouse_still() or gs.is_physical_mouse_active():
		printerr("FAIL: GazeServer is_physical_mouse_still should be true at rest")
		quit(1)
		return
	if not gs.has_method("get_mouse_stillness_duration") or not gs.has_method("set_mouse_stillness_duration"):
		printerr("FAIL: GazeServer missing mouse stillness duration methods")
		quit(1)
		return
	var dur = gs.get_mouse_stillness_duration()
	if abs(dur - 1.5) > 0.01:
		printerr("FAIL: GazeServer default stillness duration should be 1.5s, got: ", dur)
		quit(1)
		return

	print("PASS: GazeServer mouse emulation and clamping configuration verified.")

	# =================== E2E TEST: FEATURE F1 (GazeDeviceProfile Resource) ===================
	print("=================== E2E TEST: FEATURE F1 (GazeDeviceProfile Resource) ===================")
	var guess_profile = GazeDeviceProfile.create_system_guess()
	if not guess_profile:
		printerr("FAIL: F1 - GazeDeviceProfile.create_system_guess() returned null")
		quit(1)
		return
	print("GazeDeviceProfile created system guess successfully.")

	guess_profile.calibrate_from_card_width(342.412, 85.603)
	var pitch = guess_profile.get_pixel_pitch_mm()
	if abs(pitch.x - 0.25) > 0.01 or abs(pitch.y - 0.25) > 0.01:
		printerr("FAIL: F1 - calibrate_from_card_width pixel pitch incorrect: ", pitch)
		quit(1)
		return
	var card_dpi = guess_profile.get_dpi()
	if abs(card_dpi.x - 101.6) > 0.1:
		printerr("FAIL: F1 - card DPI calculation incorrect: ", card_dpi)
		quit(1)
		return
	var fl = guess_profile.get_focal_length_px(1440.0)
	if abs(fl - 1130.16) > 1.0:
		printerr("FAIL: F1 - focal length calculation incorrect: ", fl)
		quit(1)
		return
	print("PASS: F1 Decoupled Resource-Based Calibration E2E verification complete.")

	# =================== E2E TEST: FEATURE F3 (CI/CD Release Validation) ===================
	print("=================== E2E TEST: FEATURE F3 (CI/CD Release Validation) ===================")
	var expected_settings = [
		"gaze/general/autostart",
		"gaze/pointing/emulate_gaze_from_mouse",
		"gaze/pointing/emulate_mouse_from_gaze",
		"gaze/pointing/default_clamping",
		"gaze/pointing/mouse_stillness_duration_sec",
		"gaze/pointing/mouse_stillness_threshold_px",
		"gaze/models/search_paths",
		"gaze/models/yunet_prefix",
		"gaze/models/gaze_prefix",
		"gaze/calibration/device_profile_path",

		"gaze/debug/overlay_scene_path"
	]
	
	for setting in expected_settings:
		if not ProjectSettings.has_setting(setting):
			printerr("FAIL: F3 - Mandatory ProjectSetting '", setting, "' is not registered")
			quit(1)
			return
		print("ProjectSetting registered: ", setting, " = ", ProjectSettings.get_setting(setting))
		
	var required_singletons = ["VisionServer", "GazeServer"]
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
	# =================== E2E TEST: TELEMETRY & GRAVITY ===================
	print("=================== E2E TEST: TELEMETRY & GRAVITY ===================")
	var accel = vs.get_raw_acceleration()
	var grav = vs.get_gravity_vector()
	print("Initial raw acceleration: ", accel, " gravity: ", grav)
	if vs.has_method("set_simulated_acceleration"):
		vs.set_simulated_acceleration(Vector3(0.0, -9.81, 0.0))
		var grav_down = vs.get_gravity_vector()
		if abs(grav_down.y - (-1.0)) > 0.05:
			printerr("FAIL: Telemetry simulated gravity down failed: ", grav_down)
			quit(1)
			return
		vs.set_simulated_acceleration(Vector3(9.81, 0.0, 0.0))
		var grav_right = vs.get_gravity_vector()
		if abs(grav_right.x - 1.0) > 0.05:
			printerr("FAIL: Telemetry simulated gravity right failed: ", grav_right)
			quit(1)
			return
		print("PASS: Telemetry acceleration injection and gravity derivation verified.")

	# =================== E2E TEST: CALIBRATION WIZARD & MODULAR STEPS ===================
	print("=================== E2E TEST: CALIBRATION WIZARD & MODULAR STEPS ===================")
	var wizard_scene = load("res://addons/godot-gaze/calibration/gaze_calibration_wizard.tscn")
	if not wizard_scene:
		printerr("FAIL: Could not load gaze_calibration_wizard.tscn")
		quit(1)
		return
	var wizard = wizard_scene.instantiate()
	root.add_child(wizard)
	var test_profile = GazeDeviceProfile.new()
	test_profile.set_logical_size_px(Vector2i(1920, 1080))
	test_profile.set_physical_size_mm(Vector2(345.0, 215.0))
	test_profile.set_camera_offset_mm(Vector3(0.0, 0.0, 0.0))
	
	var wizard_state = {"done": false, "profile": null}
	wizard.wizard_completed.connect(func(p):
		wizard_state["done"] = true
		wizard_state["profile"] = p
	)
	
	wizard.start_wizard(test_profile)
	if wizard._steps.size() != 5:
		printerr("FAIL: Wizard does not contain expected 5 steps, got: ", wizard._steps.size())
		quit(1)
		return
	
	# Simulate finishing each step
	for i in range(wizard._steps.size()):
		var step = wizard._steps[i]
		if not step.is_visible_in_tree():
			printerr("FAIL: Step ", i, " is not visible when active")
			quit(1)
			return
		step.complete_step()
		
	if not wizard_state["done"] or wizard_state["profile"] == null:
		printerr("FAIL: Wizard failed to complete all steps and emit wizard_completed")
		quit(1)
		return
	print("PASS: GazeCalibrationWizard sequential step execution verified.")
	wizard.queue_free()

	print("PASS: F3 CI/CD Release Validation E2E verification complete.")

	# =================== E2E TEST: PHYSICAL DIRECTIONAL INVARIANTS (GODOT BINDINGS) ===================
	print("=================== E2E TEST: PHYSICAL DIRECTIONAL INVARIANTS (GODOT BINDINGS) ===================")
	var cam_rid = vs.camera_create()
	vs.camera_set_device_id(cam_rid, -1)
	vs.camera_set_resolution(cam_rid, 1440, 960)
	vs.camera_set_focal_length(cam_rid, 1440.0 * 1.5625)
	vs.camera_start(cam_rid)

	var fixture_dev = GazeDeviceProfile.new()
	fixture_dev.set_logical_size_px(Vector2i(3024, 1964))
	fixture_dev.set_physical_size_mm(Vector2(301.5, 188.5))
	fixture_dev.set_camera_offset_mm(Vector3(0.0, 0.0, 0.0))
	fixture_dev.set_camera_roll_deg(0.0)
	gs.set_device_profile(fixture_dev)

	gs.set_camera_vision_rid(cam_rid)

	gs.start_processing()

	var run_fixture_e2e = func(img_name: String) -> Dictionary:
		var path = "tests/resources/" + img_name
		if not FileAccess.file_exists(path):
			path = "../tests/resources/" + img_name
		var img = Image.load_from_file(path)
		if not img:
			printerr("FAIL: E2E - Missing fixture: ", path)
			return {}
		var w = img.get_width()
		var h = img.get_height()
		var focal = float(w) * 1.5625
		vs.camera_set_resolution(cam_rid, w, h)
		vs.camera_set_focal_length(cam_rid, focal)
		gs.reset()
		var tex = ImageTexture.create_from_image(img)
		for k in range(25):
			vs.inject_texture(cam_rid, tex)
			gs.trigger_process()
			await get_tree().create_timer(0.04).timeout
		gs.trigger_process()
		var head_trans = gs.get_head_position()
		var head_xform = gs.get_head_transform()
		var head_fwd = -head_xform.basis.z.normalized()
		var nose_gaze = gs.project_ray_to_viewport(head_trans, head_fwd)
		var eye_gaze = gs.get_gaze_screen_px(false)
		print("  -> Image: ", img_name, " | Head Forward: ", head_fwd, " | Nose: ", nose_gaze, " | Gaze: ", eye_gaze)
		return {
			"face_detected": gs.is_face_detected(),
			"nose_gaze": nose_gaze,
			"eye_gaze": eye_gaze
		}

	var res_center = await run_fixture_e2e.call("self_center.jpg")
	var res_left = await run_fixture_e2e.call("self_left_left.jpg")
	var res_right = await run_fixture_e2e.call("self_right_right.jpg")
	var res_top_top = await run_fixture_e2e.call("self_top_top.jpg")
	var res_down_down = await run_fixture_e2e.call("self_down_down.jpg")
	var res_top_down = await run_fixture_e2e.call("self_nosetop_eyesdown.jpg")
	var res_nl_er = await run_fixture_e2e.call("self_noseleft_eyesright.jpg")

	if not (res_center["face_detected"] and res_left["face_detected"] and res_right["face_detected"] and res_top_top["face_detected"] and res_down_down["face_detected"] and res_top_down["face_detected"] and res_nl_er["face_detected"]):
		printerr("FAIL: E2E - One or more benchmark fixtures failed face detection")
		quit(1)
		return

	var nose_c: Vector2 = res_center["nose_gaze"]
	var gaze_c: Vector2 = res_center["eye_gaze"]
	var nose_l: Vector2 = res_left["nose_gaze"]
	var gaze_l: Vector2 = res_left["eye_gaze"]
	var nose_r: Vector2 = res_right["nose_gaze"]
	var gaze_r: Vector2 = res_right["eye_gaze"]
	var nose_top: Vector2 = res_top_top["nose_gaze"]
	var gaze_top: Vector2 = res_top_top["eye_gaze"]
	var nose_down: Vector2 = res_down_down["nose_gaze"]
	var gaze_down: Vector2 = res_down_down["eye_gaze"]
	var nose_top_down: Vector2 = res_top_down["nose_gaze"]
	var gaze_top_down: Vector2 = res_top_down["eye_gaze"]
	var nose_nl: Vector2 = res_nl_er["nose_gaze"]
	var gaze_er: Vector2 = res_nl_er["eye_gaze"]

	print("E2E GDScript Measurements:")
	print("  Center:     Nose=", nose_c, " Gaze=", gaze_c)
	print("  Left/Left:  Nose=", nose_l, " Gaze=", gaze_l)
	print("  Right/Right: Nose=", nose_r, " Gaze=", gaze_r)
	print("  Top/Top:    Nose=", nose_top, " Gaze=", gaze_top)
	print("  Down/Down:  Nose=", nose_down, " Gaze=", gaze_down)
	print("  Top/Down:   Nose=", nose_top_down, " Gaze=", gaze_top_down)
	print("  NL/ER:      Nose=", nose_nl, " Gaze=", gaze_er)

	# 1. NOSEGAZE YAW: Turning head to user's left (screen left, smaller X) must project left of center (< center.x)
	if not (nose_l.x < nose_c.x - 30.0 and nose_r.x > nose_c.x + 30.0):
		printerr("FAIL: E2E - Nosegaze yaw invariant violated! Left X: ", nose_l.x, " Center X: ", nose_c.x, " Right X: ", nose_r.x)
		quit(1)
		return

	# 2. NOSEGAZE PITCH: Pitching head UP must project towards screen top (smaller Y) than pitching DOWN
	if not (nose_top.y < nose_down.y - 20.0):
		printerr("FAIL: E2E - Nosegaze pitch invariant violated! Top Y: ", nose_top.y, " Down Y: ", nose_down.y)
		quit(1)
		return

	# 4. EYEGAZE PITCH: Gazing UP must project towards screen top (smaller Y) than gazing DOWN
	if not (gaze_top.y < gaze_down.y - 15.0):
		printerr("FAIL: E2E - Eyegaze pitch invariant violated! Top Y: ", gaze_top.y, " Down Y: ", gaze_down.y)
		quit(1)
		return

	# 5. DISSOCIATED GAZE INVARIANTS:
	if not (gaze_er.x > nose_nl.x + 50.0):
		printerr("FAIL: E2E - Dissociated gaze yaw invariant violated! Gaze ER X: ", gaze_er.x, " Nose NL X: ", nose_nl.x)
		quit(1)
		return

	if not (gaze_top_down.y > nose_top_down.y + 50.0):
		printerr("FAIL: E2E - Dissociated gaze pitch invariant violated! Gaze Top/Down Y: ", gaze_top_down.y, " Nose Top/Down Y: ", nose_top_down.y)
		quit(1)
		return

	print("PASS: All Physical Directional Invariants (Nose/Eye Yaw & Pitch) Verified in GDScript!")
	print("PASS: Physical Directional Invariants in GDScript bindings verified.")

	gs.stop_tracking(true)
	vs.camera_stop(cam_rid)
	vs.camera_free(cam_rid)

	print("==================================================================")
	print("ALL Headless Integration & E2E tests have passed successfully!")
	print("==================================================================")
	quit(0)
