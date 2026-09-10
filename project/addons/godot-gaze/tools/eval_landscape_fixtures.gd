extends SceneTree

# Evaluates captured landscape fixtures under:
# 1. Mode A: Center-Screen Hack (camera offset (0, 0))
# 2. Mode B: Physical Bezel Offset (camera offset (+W/2, 0) = (+150.75, 0) mm)

func _init() -> void:
	call_deferred("run_evaluation")

func run_evaluation() -> void:
	print("\n================================================================================")
	print("       LANDSCAPE FIXTURE EMPIRICAL EVALUATION & DRIFT BENCHMARK")
	print("================================================================================\n")

	var vs = Engine.get_singleton("VisionServer")
	var gs = Engine.get_singleton("GazeServer")

	if not vs or not gs:
		printerr("FATAL: VisionServer or GazeServer not available!")
		quit(1)
		return

	var res_dir = ProjectSettings.globalize_path("res://../tests/resources")
	var fixtures = [
		"landscape_center.jpg",
		"landscape_left_left.jpg",
		"landscape_right_right.jpg",
		"landscape_top_top.jpg",
		"landscape_down_down.jpg",
		"landscape_noseleft_eyesright.jpg",
		"landscape_noseright_eyesleft.jpg",
		"landscape_nosetop_eyesdown.jpg",
		"landscape_nosedown_eyesup.jpg"
	]

	var cam_rid = vs.camera_create()
	vs.camera_set_device_id(cam_rid, -1)
	vs.camera_start(cam_rid)
	gs.set_camera_vision_rid(cam_rid)
	gs.start_processing()

	var screen_w_mm = 188.5
	var screen_h_mm = 301.5
	var screen_w_pt = 982.0
	var screen_h_pt = 1512.0

	# Profile for Mode A: Screen Center (0, 0)
	var profile_center = GazeDeviceProfile.new()
	profile_center.set_physical_size_mm(Vector2(screen_w_mm, screen_h_mm))
	profile_center.set_logical_size_px(Vector2i(int(screen_w_pt), int(screen_h_pt)))
	profile_center.set_camera_offset_mm(Vector3(0.0, 0.0, 0.0))
	profile_center.set_camera_roll_deg(0.0)

	# Profile for Mode B: Physical Right Bezel (+W/2, 0)
	var profile_bezel = GazeDeviceProfile.new()
	profile_bezel.set_physical_size_mm(Vector2(screen_w_mm, screen_h_mm))
	profile_bezel.set_logical_size_px(Vector2i(int(screen_w_pt), int(screen_h_pt)))
	profile_bezel.set_camera_offset_mm(Vector3(screen_w_mm * 0.5, 0.0, 0.0))
	profile_bezel.set_camera_roll_deg(0.0)

	print("Evaluating %d landscape fixtures across Mode A (Center Hack) vs Mode B (Physical Side-Bezel Offset):\n" % fixtures.size())
	print("| %-30s | %-16s | %-20s | %-20s | %-18s |" % [
		"Fixture Name", "Head Pose (Y,P,R)", "Mode A Proj (pt)", "Mode B Proj (pt)", "Delta X Shift (mm)"
	])
	print("|" + "-".repeat(32) + "|" + "-".repeat(18) + "|" + "-".repeat(22) + "|" + "-".repeat(22) + "|" + "-".repeat(20) + "|")

	for fn in fixtures:
		var path = res_dir + "/" + fn
		if not FileAccess.file_exists(path):
			print("File missing: ", path)
			continue

		var img = Image.load_from_file(path)
		# Sensor orientation compensation: rotate sideways camera image 90° CCW
		img.rotate_90(COUNTERCLOCKWISE)
		var w = img.get_width()
		var h = img.get_height()
		var focal = float(w) / (2.0 * tan(deg_to_rad(65.0) * 0.5))

		vs.camera_set_resolution(cam_rid, w, h)
		vs.camera_set_focal_length(cam_rid, focal)

		# 1. Evaluate with Mode A (Center)
		gs.set_device_profile(profile_center)
		gs.reset()
		var tex = ImageTexture.create_from_image(img)
		vs.inject_texture(cam_rid, tex)
		gs.trigger_process()
		await gs.gaze_frame_ready

		var det_a = gs.is_face_detected()
		var head_rot = gs.get_head_pose_euler_deg()
		var gaze_a = gs.get_gaze_screen_px(false)

		# 2. Evaluate with Mode B (Bezel)
		gs.set_device_profile(profile_bezel)
		gs.reset()
		vs.inject_texture(cam_rid, tex)
		gs.trigger_process()
		await gs.gaze_frame_ready

		var gaze_b = gs.get_gaze_screen_px(false)

		var delta_x_mm = (gaze_b.x - gaze_a.x) * (screen_w_mm / screen_w_pt)

		var rot_str = "(%.1f, %.1f, %.1f)" % [head_rot.x, head_rot.y, head_rot.z] if det_a else "NO_FACE"
		var gaze_a_str = "(%.1f, %.1f)" % [gaze_a.x, gaze_a.y] if det_a else "NO_FACE"
		var gaze_b_str = "(%.1f, %.1f)" % [gaze_b.x, gaze_b.y] if det_a else "NO_FACE"
		var delta_str = "%+.1f mm" % delta_x_mm if det_a else "N/A"

		print("| %-30s | %-16s | %-20s | %-20s | %-18s |" % [
			fn, rot_str, gaze_a_str, gaze_b_str, delta_str
		])

	print("\n================================================================================")
	print("EVALUATION COMPLETE")
	print("================================================================================\n")
	gs.stop_processing()
	vs.camera_stop(cam_rid)
	vs.camera_free(cam_rid)
	quit(0)
