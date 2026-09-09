extends SceneTree

func _init() -> void:
	call_deferred("_run_eval")

func px_to_screen_mm(px: Vector2, logical_sz: Vector2i, physical_mm: Vector2) -> Vector2:
	var screen_cx = float(logical_sz.x) * 0.5
	var px_per_mm_x = float(logical_sz.x) / physical_mm.x
	var px_per_mm_y = float(logical_sz.y) / physical_mm.y
	var x_mm = -(px.x - screen_cx) / px_per_mm_x
	var y_mm = -px.y / px_per_mm_y
	return Vector2(x_mm, y_mm)

func _run_eval() -> void:
	print("\n==================================================================")
	print("=== EVALUATING DYNAMIC MOTION TRANSITION SEQUENCES ===")
	print("==================================================================\n")

	var vs = Engine.get_singleton("VisionServer")
	var gs = Engine.get_singleton("GazeServer")

	if not vs or not gs:
		printerr("Gaze singletons not available!")
		quit(1)
		return

	var logical_sz = Vector2i(3024, 1964)
	var physical_mm = Vector2(301.5, 188.5)

	var profile = GazeDeviceProfile.new()
	profile.set_logical_size_px(logical_sz)
	profile.set_physical_size_mm(physical_mm)
	profile.set_camera_offset_mm(Vector3(0.0, 0.0, 0.0))
	profile.set_camera_roll_deg(0.0)
	gs.set_device_profile(profile)

	var cam_rid = vs.camera_create()
	vs.camera_set_device_id(cam_rid, -1)
	vs.camera_start(cam_rid)

	gs.set_camera_offsets(Vector3(0.0, 0.0, 0.0), 0.0)
	gs.set_camera_vision_rid(cam_rid)
	gs.set_smoother(null)
	gs.start_processing()

	var base_dir = "res://../build/tests/artifacts/transitions"
	var global_base_dir = ProjectSettings.globalize_path(base_dir)

	var trans_list = [
		"trans_00_self_center_to_self_top_left",
		"trans_01_self_top_left_to_self_top_right",
		"trans_02_self_top_right_to_self_bottom_left",
		"trans_03_self_bottom_left_to_self_bottom_right",
		"trans_04_self_bottom_right_to_self_yaw_left",
		"trans_05_self_yaw_left_to_self_yaw_right",
		"trans_06_self_yaw_right_to_self_pitch_down",
		"trans_07_self_pitch_down_to_self_pitch_up",
		"trans_08_self_pitch_up_to_self_roll_left",
		"trans_09_self_roll_left_to_self_roll_right"
	]

	print("| %-38s | %-12s | %-16s | %-16s | %-16s |" % [
		"Transition Motion", "Lock Rate", "Start Gaze (mm)", "End Gaze (mm)", "Path Length (mm)"
	])
	print("|" + "-".repeat(40) + "|" + "-".repeat(14) + "|" + "-".repeat(18) + "|" + "-".repeat(18) + "|" + "-".repeat(18) + "|")

	for trans_id in trans_list:
		var trans_dir = global_base_dir + "/" + trans_id
		if not DirAccess.dir_exists_absolute(trans_dir):
			continue

		var gaze_points: Array[Vector2] = []
		var head_rots: Array[Vector3] = []
		var tracked_count = 0
		var total_count = 0

		gs.reset()

		# Iterate through frames
		var f_idx = 0
		while true:
			var frame_path = "%s/frame_%03d.jpg" % [trans_dir, f_idx]
			if not FileAccess.file_exists(frame_path):
				break
			total_count += 1

			var img = Image.load_from_file(frame_path)
			var w = img.get_width()
			var h = img.get_height()
			var focal = float(w) / (2.0 * tan(deg_to_rad(65.0) * 0.5))

			vs.camera_set_resolution(cam_rid, w, h)
			vs.camera_set_focal_length(cam_rid, focal)

			var tex = ImageTexture.create_from_image(img)
			vs.inject_texture(cam_rid, tex)
			gs.trigger_process()
			await gs.gaze_frame_ready

			if gs.is_face_detected():
				tracked_count += 1
				var gaze_px = gs.get_gaze_screen_px(false)
				var gaze_mm = px_to_screen_mm(gaze_px, logical_sz, physical_mm)
				gaze_points.append(gaze_mm)
				head_rots.append(gs.get_head_pose_euler_deg())

			f_idx += 1

		var lock_rate = "%.0f%% (%d/%d)" % [float(tracked_count) / float(max(1, total_count)) * 100.0, tracked_count, total_count]
		var start_gaze = gaze_points[0] if gaze_points.size() > 0 else Vector2.ZERO
		var end_gaze = gaze_points[gaze_points.size() - 1] if gaze_points.size() > 0 else Vector2.ZERO

		# Compute path length
		var path_len = 0.0
		for k in range(1, gaze_points.size()):
			path_len += (gaze_points[k] - gaze_points[k - 1]).length()

		var start_str = "(%.1f, %.1f)" % [start_gaze.x, start_gaze.y] if tracked_count > 0 else "NO_FACE"
		var end_str = "(%.1f, %.1f)" % [end_gaze.x, end_gaze.y] if tracked_count > 0 else "NO_FACE"
		var path_str = "%.1f mm" % [path_len]

		print("| %-38s | %-12s | %-16s | %-16s | %-16s |" % [
			trans_id.replace("trans_", ""), lock_rate, start_str, end_str, path_str
		])

	print("\n==================================================================\n")

	gs.stop_tracking(true)
	vs.camera_stop(cam_rid)
	vs.camera_free(cam_rid)
	quit(0)
