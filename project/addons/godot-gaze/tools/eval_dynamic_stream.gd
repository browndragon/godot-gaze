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

func calc_std_dev(values: Array[float]) -> float:
	if values.size() < 2:
		return 0.0
	var sum = 0.0
	for v in values:
		sum += v
	var mean = sum / float(values.size())
	var var_sum = 0.0
	for v in values:
		var_sum += (v - mean) * (v - mean)
	return sqrt(var_sum / float(values.size() - 1))

func _run_eval() -> void:
	print("\n==================================================================")
	print("=== EVALUATING DYNAMIC CONTINUOUS VIDEO SEQUENCES (30 FPS) ===")
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

	var base_dir = "res://../build/tests/artifacts/video_sequences"
	var global_base_dir = ProjectSettings.globalize_path(base_dir)

	var cues = [
		"self_center",
		"self_top_left",
		"self_top_right",
		"self_bottom_left",
		"self_bottom_right",
		"self_yaw_left",
		"self_yaw_right",
		"self_pitch_down",
		"self_pitch_up",
		"self_roll_left",
		"self_roll_right",
		"eyes_both_open",
		"eyes_both_wink",
		"eyes_left_wink"
	]

	print("| %-20s | %-12s | %-18s | %-18s | %-16s | %-15s |" % [
		"Sequence (1s / 30f)", "Lock Rate", "Mean Gaze (X, Y mm)", "Gaze Jitter (std dev)", "Mean Head Depth", "Eye Openness"
	])
	print("|" + "-".repeat(22) + "|" + "-".repeat(14) + "|" + "-".repeat(20) + "|" + "-".repeat(20) + "|" + "-".repeat(18) + "|" + "-".repeat(17) + "|")

	for cue_id in cues:
		var cue_dir = global_base_dir + "/" + cue_id
		if not DirAccess.dir_exists_absolute(cue_dir):
			continue

		var gaze_xs: Array[float] = []
		var gaze_ys: Array[float] = []
		var head_zs: Array[float] = []
		var left_opens: Array[float] = []
		var right_opens: Array[float] = []
		var frames_tracked = 0
		var total_frames = 30

		gs.reset()

		for i in range(total_frames):
			var frame_path = "%s/frame_%02d.jpg" % [cue_dir, i]
			if not FileAccess.file_exists(frame_path):
				break

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
				frames_tracked += 1
				var head_pos = gs.get_head_position()
				head_zs.append(head_pos.z)

				var gaze_px = gs.get_gaze_screen_px(false)
				var gaze_mm = px_to_screen_mm(gaze_px, logical_sz, physical_mm)
				gaze_xs.append(gaze_mm.x)
				gaze_ys.append(gaze_mm.y)

				left_opens.append(gs.get_left_eye_openness())
				right_opens.append(gs.get_right_eye_openness())

		var lock_rate = "%.0f%% (%d/30)" % [float(frames_tracked) / float(total_frames) * 100.0, frames_tracked]
		
		var mean_gx = 0.0
		var mean_gy = 0.0
		for x in gaze_xs: mean_gx += x
		for y in gaze_ys: mean_gy += y
		if gaze_xs.size() > 0:
			mean_gx /= float(gaze_xs.size())
			mean_gy /= float(gaze_ys.size())

		var std_gx = calc_std_dev(gaze_xs)
		var std_gy = calc_std_dev(gaze_ys)

		var mean_z = 0.0
		for z in head_zs: mean_z += z
		if head_zs.size() > 0: mean_z /= float(head_zs.size())

		var mean_lo = 0.0
		var mean_ro = 0.0
		for lo in left_opens: mean_lo += lo
		for ro in right_opens: mean_ro += ro
		if left_opens.size() > 0:
			mean_lo /= float(left_opens.size())
			mean_ro /= float(right_opens.size())

		var gaze_mean_str = "(%.1f, %.1f) mm" % [mean_gx, mean_gy] if frames_tracked > 0 else "NO_FACE"
		var jitter_str = "σx=%.1f, σy=%.1f mm" % [std_gx, std_gy] if frames_tracked > 1 else "N/A"
		var depth_str = "Z=%.1f mm" % [mean_z] if frames_tracked > 0 else "NO_FACE"
		var open_str = "L=%.2f, R=%.2f" % [mean_lo, mean_ro] if frames_tracked > 0 else "N/A"

		print("| %-20s | %-12s | %-18s | %-18s | %-16s | %-15s |" % [
			cue_id, lock_rate, gaze_mean_str, jitter_str, depth_str, open_str
		])

	print("\n==================================================================\n")

	gs.stop_tracking(true)
	vs.camera_stop(cam_rid)
	vs.camera_free(cam_rid)
	quit(0)
