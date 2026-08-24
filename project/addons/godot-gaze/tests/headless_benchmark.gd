extends Node

@onready var root = get_parent()

func quit(exit_code: int = 0) -> void:
	get_tree().quit(exit_code)

func _ready():
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

func project_ray_to_screen_mm(origin_godot: Vector3, dir_godot: Vector3) -> Vector2:
	var origin_cv = Vector3(origin_godot.x, -origin_godot.y, -origin_godot.z)
	var dir_cv = Vector3(dir_godot.x, -dir_godot.y, -dir_godot.z)

	var cam_offset = Vector3(0.0, 94.25, 0.0)

	var o_z = origin_cv.z + cam_offset.z
	var v_z = dir_cv.z

	if abs(v_z) < 1e-6 or v_z >= 0.0:
		return Vector2(-9999.0, -9999.0)

	var t = -o_z / v_z
	if t < 0.0:
		return Vector2(-9999.0, -9999.0)

	var pos_x = origin_cv.x + cam_offset.x + t * dir_cv.x
	var pos_y = origin_cv.y + cam_offset.y + t * dir_cv.y

	var mm_x = pos_x
	var mm_y = pos_y - 94.25
	return Vector2(mm_x, mm_y)

func run_benchmark():
	print("=================== GODOT HEADLESS GAZE BENCHMARK ===================")

	var dp = DisplayProfile.new()
	dp.logical_size_px = Vector2i(3024, 1964)
	dp.physical_size_mm = Vector2(301.5, 188.5)

	var tracker = GazeTracker.new()
	tracker.name = "HeadlessBenchmarkTracker"

	var cam_sensor = CameraSensor.new()
	cam_sensor.name = "CameraSensor"
	cam_sensor.camera_device_id = -1
	tracker.add_child(cam_sensor)

	tracker.display_profile = dp
	tracker.window_position_override = Vector2(0, 0)
	root.size = Vector2i(3024, 1964)
	root.add_child(tracker)

	var ok = tracker.initialize_tracker()
	tracker.update_projection_parameters()
	if not ok:
		printerr("FAIL: GazeTracker initialization failed in GazeServer")
		quit(1)
		return
	print("GazeServer + GazeTracker initialized successfully via ProjectSettings.")

	var vs = Engine.get_singleton("VisionServer")
	var gs = Engine.get_singleton("GazeServer")

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
		"| --- | --- | --- | --- | --- | --- |"
	]

	var mismatches = []

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

		if vs and cam_sensor:
			var cam_rid = cam_sensor.get_camera_rid()
			vs.camera_start(cam_rid)
			var tex = ImageTexture.create_from_image(img)
			vs.inject_texture(cam_rid, tex)
			if gs:
				for k in range(8):
					gs.trigger_process()
					await get_tree().create_timer(0.05).timeout

		# Get tracking outputs from GazeTracker & GazeServer
		var face_rid = tracker.get_face_estimator().get_face_rid()
		var eye_rid = tracker.get_eye_estimator().get_eye_rid()

		var head_trans = gs.get_head_pose_origin_mm(face_rid)
		var head_rot = gs.get_head_pose_euler_deg(face_rid)

		var nose_proj_mm = px_to_mm(tracker.nose_gaze)
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
			{"name": "head_pos_mm", "val_str": format_vec3(head_trans), "err_str": "N/A", "err_mag": 0.0},
			{"name": "head_rot_deg", "val_str": format_vec3(head_rot), "err_str": rot_err_str, "err_mag": rot_err_mag},
			{"name": "nose_mm", "val_str": format_vec2(nose_proj_mm), "err_str": format_vec2(nose_diff), "err_mag": nose_err_mag},
			{"name": "gaze_mm", "val_str": format_vec2(gaze_proj_mm), "err_str": format_vec2(gaze_diff), "err_mag": gaze_err_mag}
		]

		for p in props:
			var key = img_file + ":" + p["name"]
			var prev_err = "N/A"
			var delta_str = "N/A"

			if golden_data.has(key):
				var g_info = golden_data[key]
				var cur_v = parse_vector(p["val_str"])
				var gold_v = parse_vector(g_info["val"])
				var diff_len = 0.0
				if p["name"] == "head_rot_deg":
					var dyaw = cur_v.y - gold_v.y
					while dyaw > 180.0: dyaw -= 360.0
					while dyaw < -180.0: dyaw += 360.0
					var dpitch = cur_v.x - gold_v.x
					var droll = cur_v.z - gold_v.z
					diff_len = sqrt(dpitch * dpitch + dyaw * dyaw + droll * droll)
				else:
					diff_len = (cur_v - gold_v).length()
				var tol = 35.0 if (p["name"] == "gaze_mm" or p["name"] == "nose_mm") else (20.0 if p["name"] == "head_pos_mm" else 10.0)
				if diff_len > tol:
					mismatches.append("Goldenfile value mismatch for %s on %s: current %s vs golden %s (delta: %.2f > %.2f)" % [p["name"], img_file, p["val_str"], g_info["val"], diff_len, tol])

			report_lines.append("| %s | %s | %s | %s | %s | %s |" % [img_file, p["name"], p["val_str"], p["err_str"], prev_err, delta_str])

	# Write report artifact
	var report_content = "\n".join(report_lines) + "\n"
	var paths = [
		"build/tests/artifacts/gaze_benchmark_report.md",
		"../build/tests/artifacts/gaze_benchmark_report.md"
	]
	for artifact_path in paths:
		var dir_path = artifact_path.get_base_dir()
		DirAccess.make_dir_recursive_absolute(dir_path)
		var out_file = FileAccess.open(artifact_path, FileAccess.WRITE)
		if out_file:
			out_file.store_string(report_content)
			out_file.close()
			print("Benchmark report written to: ", artifact_path)

	if mismatches.size() > 0:
		printerr("========================================================================")
		printerr("BENCHMARK GOLDENFILE MISMATCH DETECTED:")
		for m in mismatches:
			printerr("  " + m)
		printerr("========================================================================")
		quit(1)
		return

	print("==========================================================")
	print("GAZE BENCHMARK SUCCEEDED: ALL METRICS MATCH GOLDENFILE!")
	print("==========================================================")
	quit(0)
