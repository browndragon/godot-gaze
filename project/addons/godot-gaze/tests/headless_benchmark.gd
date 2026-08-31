extends SceneTree

func _init():
	call_deferred("run_benchmark")

func parse_vector(str_val: String) -> Vector3:
	var cleaned = str_val.replace("(", "").replace(")", "").replace("mm", "").replace("deg", "").strip_edges()
	var parts = cleaned.split(",")
	if parts.size() >= 3:
		return Vector3(parts[0].to_float(), parts[1].to_float(), parts[2].to_float())
	elif parts.size() == 2:
		return Vector3(parts[0].to_float(), parts[1].to_float(), 0.0)
	return Vector3.ZERO

func parse_goldenfile(filepath: String) -> Dictionary:
	var data = {}
	if not FileAccess.file_exists(filepath):
		return data
		
	var file = FileAccess.open(filepath, FileAccess.READ)
	var text = file.get_as_text()
	file.close()

	var lines = text.split("\n")
	for line in lines:
		line = line.strip_edges()
		if line.begins_with("|") and not line.begins_with("| Image") and not line.begins_with("| ---"):
			var parts = line.split("|")
			if parts.size() >= 6:
				var img = parts[1].strip_edges()
				var prop = parts[2].strip_edges()
				var val = parts[3].strip_edges()
				var err = parts[4].strip_edges()
				data[img + ":" + prop] = {"val": val, "err": err}
	return data

func format_vec2(v: Vector2) -> String:
	return "(%.1f,%.1f)" % [v.x, v.y]

func format_vec3(v: Vector3) -> String:
	return "(%.1f,%.1f,%.1f)" % [v.x, v.y, v.z]

func px_to_mm(px: Vector2) -> Vector2:
	if px.x <= -9000.0 or px.y <= -9000.0:
		return Vector2(-9999.0, -9999.0)
	var x_mm = (px.x - 3024.0 / 2.0) * (301.5 / 3024.0)
	var y_mm = (px.y - 1964.0 / 2.0) * (188.5 / 1964.0)
	return Vector2(x_mm, y_mm)

func px_to_screen_mm(px: Vector2) -> Vector2:
	if px.x == INF or px.y == INF:
		return Vector2(-9999.0, -9999.0)
	var x_mm = (px.x - 3024.0 / 2.0) * (301.5 / 3024.0)
	var y_mm = (px.y - 1964.0 / 2.0) * (188.5 / 1964.0)
	return Vector2(x_mm, y_mm)

func run_benchmark():
	print("=================== GODOT HEADLESS GAZE BENCHMARK ===================")

	var vs = Engine.get_singleton("VisionServer")
	var gs = Engine.get_singleton("GazeServer")

	var dev_cal = MockDeviceCalibration.new()
	dev_cal.logical_size_px = Vector2i(3024, 1964)
	dev_cal.physical_size_mm = Vector2(301.5, 188.5)
	dev_cal.camera_offset = Vector3(0.0, 0.0, 0.0)
	dev_cal.camera_tilt = 0.0
	dev_cal.set_window_position(Vector2(0, 0))
	gs.set_device_calibration(dev_cal)

	var cam_rid = vs.camera_create()
	vs.camera_set_device_id(cam_rid, -1)
	vs.camera_set_resolution(cam_rid, 1440, 960)
	var focal = 1440.0 / (2.0 * tan(deg_to_rad(65.0) * 0.5))
	vs.camera_set_focal_length(cam_rid, focal)
	vs.camera_start(cam_rid)

	var disp_rid = gs.display_create()
	gs.display_set_device_calibration(disp_rid, dev_cal)
	var s_cam_rid = gs.camera_create(disp_rid)
	gs.camera_set_offsets(s_cam_rid, Vector3(0.0, 0.0, 0.0), 0.0)
	gs.camera_set_vision_rid(s_cam_rid, cam_rid)
	var face_rid = gs.face_tracker_create(s_cam_rid)
	var eye_rid = gs.eye_tracker_create(face_rid)

	gs.start_processing()
	print("GazeServer initialized successfully for headless benchmark.")

	var targets = [
		{"file": "self_center.jpg", "nose_target": Vector2(0.0, 0.0), "gaze_target": Vector2(0.0, 0.0)},
		{"file": "self_left_left.jpg", "nose_target": Vector2(-150.75, 0.0), "gaze_target": Vector2(-150.75, 0.0)},
		{"file": "self_right_right.jpg", "nose_target": Vector2(150.75, 0.0), "gaze_target": Vector2(150.75, 0.0)},
		{"file": "self_top_top.jpg", "nose_target": Vector2(0.0, -94.25), "gaze_target": Vector2(0.0, -94.25)},
		{"file": "self_down_down.jpg", "nose_target": Vector2(0.0, 94.25), "gaze_target": Vector2(0.0, 94.25)},
		{"file": "self_nosedown_eyesup.jpg", "nose_target": Vector2(0.0, 94.25), "gaze_target": Vector2(0.0, -94.25)},
		{"file": "self_noseleft_eyesright.jpg", "nose_target": Vector2(-150.75, 0.0), "gaze_target": Vector2(150.75, 0.0)},
		{"file": "self_noseright_eyesleft.jpg", "nose_target": Vector2(150.75, 0.0), "gaze_target": Vector2(-150.75, 0.0)},
		{"file": "self_nosetop_eyesdown.jpg", "nose_target": Vector2(0.0, -94.25), "gaze_target": Vector2(0.0, 94.25)}
	]

	var golden_file = "test_assets/gaze_benchmark_report.md"
	if not FileAccess.file_exists(golden_file):
		golden_file = "../test_assets/gaze_benchmark_report.md"
	var golden_data = parse_goldenfile(golden_file)

	var report_lines = [
		"# Gaze Benchmark Report",
		"Generated on: " + Time.get_datetime_string_from_system(true) + "Z",
		"",
		"| Image File | Property | Current Value | Error | Previous Error | Delta |",
		"| :--- | :--- | :--- | :--- | :--- | :--- |"
	]

	var any_metric_regressed = false

	for target in targets:
		var img_file = target["file"]
		var img_path = "tests/resources/" + img_file
		if not FileAccess.file_exists(img_path):
			img_path = "../tests/resources/" + img_file

		if not FileAccess.file_exists(img_path):
			printerr("Missing test image: ", img_path)
			continue

		var img = Image.load_from_file(img_path)
		if not img:
			printerr("Failed to load image: ", img_path)
			continue

		print("Processing benchmark frame: ", img_file, " (", img.get_width(), "x", img.get_height(), ")")

		if vs:
			var tex = ImageTexture.create_from_image(img)
			vs.inject_texture(cam_rid, tex)
			if gs:
				for k in range(10):
					gs.trigger_process()
					await create_timer(0.04).timeout

		# Get tracking outputs from GazeServer
		var head_trans = gs.get_head_pose_origin_mm(face_rid)
		var head_rot = gs.get_head_pose_euler_deg(face_rid)
		var head_xform = gs.get_relative_transform(face_rid)
		var head_fwd = -head_xform.basis.z
		var nose_proj_px = gs.project_ray_to_viewport(head_trans, head_fwd, false)
		var nose_proj_mm = px_to_screen_mm(nose_proj_px)
		var gaze_proj_mm = gs.get_projected_gaze_mm_from_eye_tracker(eye_rid, false)

		print("  -> Tracked Face: ", gs.is_face_detected(face_rid), " | Head Trans: ", head_trans, " | Head Rot: ", head_rot, " | Nose mm: ", nose_proj_mm, " | Gaze mm: ", gaze_proj_mm)

		var nose_target = target["nose_target"]
		var gaze_target = target["gaze_target"]

		# Compute rotation target error
		var P_cam_target = Vector3(nose_target.x, -(nose_target.y + 94.25), 0.0)
		var diff_vec = P_cam_target - head_trans
		var rot_err_str = "N/A"
		var rot_err_mag = 0.0
		if diff_vec.length() > 0.0:
			var pitch_exp = rad_to_deg(atan2(-diff_vec.y, sqrt(diff_vec.x * diff_vec.x + diff_vec.z * diff_vec.z)))
			var yaw_exp = rad_to_deg(atan2(diff_vec.x, -diff_vec.z))
			var yaw_diff = head_rot.y - yaw_exp
			while yaw_diff > 180.0: yaw_diff -= 360.0
			while yaw_diff < -180.0: yaw_diff += 360.0
			var pitch_diff = head_rot.x - pitch_exp
			var roll_diff = head_rot.z
			rot_err_str = format_vec3(Vector3(pitch_diff, yaw_diff, roll_diff))
			rot_err_mag = sqrt(pitch_diff * pitch_diff + yaw_diff * yaw_diff + roll_diff * roll_diff)

		var nose_diff = nose_proj_mm - nose_target
		var nose_err_mag = sqrt(nose_diff.x * nose_diff.x + nose_diff.y * nose_diff.y)

		var gaze_diff = gaze_proj_mm - gaze_target
		var gaze_err_mag = sqrt(gaze_diff.x * gaze_diff.x + gaze_diff.y * gaze_diff.y)

		var props = [
			{"prop": "head_pos_mm", "val": format_vec3(head_trans), "err_str": "N/A", "err_mag": 0.0},
			{"prop": "head_rot_deg", "val": format_vec3(head_rot), "err_str": rot_err_str, "err_mag": rot_err_mag},
			{"prop": "nose_mm", "val": format_vec2(nose_proj_mm), "err_str": "%.1f mm" % nose_err_mag, "err_mag": nose_err_mag},
			{"prop": "gaze_mm", "val": format_vec2(gaze_proj_mm), "err_str": "%.1f mm" % gaze_err_mag, "err_mag": gaze_err_mag}
		]

		for p in props:
			var key = img_file + ":" + p["prop"]
			var prev_err_str = "N/A"
			var delta_str = "0.0"
			if golden_data.has(key):
				prev_err_str = golden_data[key]["err"]
				var prev_mag = 0.0
				var has_prev_err = false
				if prev_err_str.ends_with(" mm"):
					prev_mag = prev_err_str.replace(" mm", "").to_float()
					has_prev_err = true
				elif prev_err_str.begins_with("(") and prev_err_str.ends_with(")"):
					var pv = parse_vector(prev_err_str)
					prev_mag = Vector2(pv.x, pv.y).length()
					has_prev_err = true
				
				if has_prev_err:
					var curr_val = p["err_mag"]
					var delta = curr_val - prev_mag
					if delta > 0.5:
						delta_str = "+%.1f mm (REGRESSION)" % delta
						any_metric_regressed = true
					elif delta < -0.5:
						delta_str = "%.1f mm (IMPROVEMENT)" % delta
					else:
						delta_str = "0.0 mm"

			report_lines.append("| %s | %s | %s | %s | %s | %s |" % [
				img_file, p["prop"], p["val"], p["err_str"], prev_err_str, delta_str
			])

	var full_report = "\n".join(report_lines) + "\n"
	var global_artifacts_dir = ProjectSettings.globalize_path("res://../build/tests/artifacts")
	if not DirAccess.dir_exists_absolute(global_artifacts_dir):
		DirAccess.make_dir_recursive_absolute(global_artifacts_dir)
	var parent_out_path = global_artifacts_dir + "/gaze_benchmark_report.md"
	var parent_file = FileAccess.open(parent_out_path, FileAccess.WRITE)
	if parent_file:
		parent_file.store_string(full_report)
		parent_file.close()
		print("Benchmark report written to: ", parent_out_path)
	else:
		printerr("Failed to open report output: ", FileAccess.get_open_error())

	print("\n" + full_report)

	gs.eye_tracker_free(eye_rid)
	gs.face_tracker_free(face_rid)
	gs.camera_free(s_cam_rid)
	gs.display_free(disp_rid)
	gs.stop_tracking(true)
	vs.camera_stop(cam_rid)
	vs.camera_free(cam_rid)

	if any_metric_regressed:
		printerr("==========================================================")
		printerr("GAZE BENCHMARK FAILED: METRICS REGRESSED FROM GOLDENFILE!")
		printerr("==========================================================")
		quit(1)
	else:
		print("==========================================================")
		print("GAZE BENCHMARK SUCCEEDED: ALL METRICS MATCH GOLDENFILE!")
		print("==========================================================")
		quit(0)
