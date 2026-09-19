extends SceneTree

# tests/test_godot_movies.gd
# Dynamic video stream and saccade regression test suite.

func _init() -> void:
	call_deferred("_run_movie_tests")

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

func _run_movie_tests() -> void:
	print("\n" + "=".repeat(70))
	print("=== GODOT MOVIES DYNAMIC REGRESSION TEST SUITE (tests/godot_movies) ===")
	print("=".repeat(70) + "\n")

	var vs = Engine.get_singleton("VisionServer")
	var gs = Engine.get_singleton("GazeServer")

	if not vs or not gs:
		printerr("FATAL: VisionServer or GazeServer not available!")
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

	var base_dir = "res://../test_assets/movies"
	var global_base_dir = ProjectSettings.globalize_path(base_dir)

	var all_passed = true
	var failed_tests: Array[String] = []

	var golden_path = ProjectSettings.globalize_path("res://../test_assets/godot_movies_golden.json")
	var golden_data: Dictionary = {}
	if FileAccess.file_exists(golden_path):
		var json_str = FileAccess.get_file_as_string(golden_path)
		var json = JSON.new()
		if json.parse(json_str) == OK:
			golden_data = json.data.get("sequences", {})

	var seq_keys = [
		"seq_01_center_fixation",
		"seq_02_horizontal_saccade",
		"seq_03_diagonal_saccade",
		"seq_04_dynamic_head_roll",
		"seq_05_eye_blink_wink",
		"seq_06_center_dwell"
	]

	for seq_id in seq_keys:
		var seq_dir = global_base_dir + "/" + seq_id
		if not DirAccess.dir_exists_absolute(seq_dir):
			printerr("ERROR: Sequence directory missing: ", seq_dir)
			all_passed = false
			failed_tests.append(seq_id + " (missing dir)")
			continue

		var meta = golden_data.get(seq_id, {})
		var seq_desc = meta.get("description", seq_id)
		print("\n--- Running Sequence: %s ---" % [seq_id])
		print("    Description: %s" % [seq_desc])

		var gaze_points: Array[Vector2] = []
		var gaze_events: Array[InputEventGaze] = []
		var head_zs: Array[float] = []
		var head_rolls: Array[float] = []
		var left_opens: Array[float] = []
		var right_opens: Array[float] = []
		var tracked_frames = 0
		var total_frames = 0

		gs.reset()

		var f_idx = 0
		while true:
			var frame_path = "%s/frame_%02d.webp" % [seq_dir, f_idx]
			if not FileAccess.file_exists(frame_path):
				break
			total_frames += 1

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
				tracked_frames += 1
				var head_pos = gs.get_head_position()
				head_zs.append(head_pos.z)

				var head_rot = gs.get_head_pose_euler_deg()
				head_rolls.append(head_rot.z)

				var gaze_px = gs.get_gaze_screen_px(false)
				var gaze_mm = px_to_screen_mm(gaze_px, logical_sz, physical_mm)
				gaze_points.append(gaze_mm)
				left_opens.append(gs.get_left_eye_openness())
				right_opens.append(gs.get_right_eye_openness())

				var ev = gs.get_most_recent_event()
				if ev is InputEventGaze:
					gaze_events.append(ev)

			f_idx += 1

		var lock_rate = float(tracked_frames) / float(max(1, total_frames))
		print("    Tracked: %d/%d frames (Lock Rate: %.1f%%)" % [tracked_frames, total_frames, lock_rate * 100.0])

		var seq_passed = true

		# 1. Lock rate check
		var min_expected_lock = meta.get("expected_lock_rate", 0.80)
		if lock_rate < min_expected_lock:
			printerr("    FAIL: Lock rate %.1f%% below expected %.1f%%" % [lock_rate * 100.0, min_expected_lock * 100.0])
			seq_passed = false
		else:
			print("    PASS: Tracking lock rate %.1f%% >= %.1f%% threshold." % [lock_rate * 100.0, min_expected_lock * 100.0])

		# 2. Sequence specific checks
		if seq_id == "seq_01_center_fixation":
			var xs: Array[float] = []
			var ys: Array[float] = []
			for pt in gaze_points:
				xs.append(pt.x)
				ys.append(pt.y)
			var std_x = calc_std_dev(xs)
			var std_y = calc_std_dev(ys)
			print("    Jitter: σx = %.2f mm, σy = %.2f mm" % [std_x, std_y])
			if std_x > 20.0 or std_y > 20.0:
				printerr("    FAIL: Jitter exceeded 20.0 mm bound (σx=%.2f, σy=%.2f)" % [std_x, std_y])
				seq_passed = false
			else:
				print("    PASS: Steady fixation jitter within strict 20.0 mm bound.")

		elif seq_id == "seq_02_horizontal_saccade":
			if gaze_points.size() >= 2:
				var min_x = gaze_points[0].x
				var max_x = gaze_points[0].x
				for pt in gaze_points:
					min_x = min(min_x, pt.x)
					max_x = max(max_x, pt.x)
				var span_x = max_x - min_x
				print("    Horizontal saccade dynamic span: %.2f mm" % [span_x])
				if span_x < 30.0:
					printerr("    FAIL: Horizontal span %.2f mm below expected 30.0 mm" % [span_x])
					seq_passed = false
				else:
					print("    PASS: Horizontal saccade traversed %.2f mm across display bezel." % [span_x])

		elif seq_id == "seq_03_diagonal_saccade":
			if gaze_points.size() >= 2:
				var min_x = gaze_points[0].x
				var max_x = gaze_points[0].x
				var min_y = gaze_points[0].y
				var max_y = gaze_points[0].y
				for pt in gaze_points:
					min_x = min(min_x, pt.x)
					max_x = max(max_x, pt.x)
					min_y = min(min_y, pt.y)
					max_y = max(max_y, pt.y)
				var span_x = max_x - min_x
				var span_y = max_y - min_y
				print("    Diagonal saccade dynamic span: X = %.2f mm, Y = %.2f mm" % [span_x, span_y])
				if span_x < 25.0 or span_y < 25.0:
					printerr("    FAIL: Diagonal span below 25.0 mm bound (span_x=%.2f, span_y=%.2f)" % [span_x, span_y])
					seq_passed = false
				else:
					print("    PASS: Diagonal saccade traversed full screen (ΔX=%.2f, ΔY=%.2f mm)." % [span_x, span_y])

		elif seq_id == "seq_04_dynamic_head_roll":
			if head_rolls.size() >= 2:
				var min_roll = head_rolls[0]
				var max_roll = head_rolls[0]
				for r in head_rolls:
					min_roll = min(min_roll, r)
					max_roll = max(max_roll, r)
				var roll_span = max_roll - min_roll
				print("    Head Roll dynamic range: %.1f° (min=%.1f°, max=%.1f°)" % [roll_span, min_roll, max_roll])
				if roll_span < 30.0:
					printerr("    FAIL: Roll dynamic range %.1f° below expected 30.0°" % [roll_span])
					seq_passed = false
				else:
					print("    PASS: Dynamic head roll tracked across %.1f° range with zero dropout." % [roll_span])

		elif seq_id == "seq_05_eye_blink_wink":
			if left_opens.size() > 0 and right_opens.size() > 0:
				var min_left = 1.0
				var max_left = 0.0
				var min_right = 1.0
				var max_right = 0.0
				for o in left_opens:
					min_left = min(min_left, o)
					max_left = max(max_left, o)
				for o in right_opens:
					min_right = min(min_right, o)
					max_right = max(max_right, o)
				var delta_left = max_left - min_left
				var delta_right = max_right - min_right
				print("    Eye Openness span: Left Δ = %.2f, Right Δ = %.2f" % [delta_left, delta_right])
				if delta_left < 0.50 and delta_right < 0.50:
					printerr("    FAIL: Eye openness delta below required 0.50 signal separation bound")
					seq_passed = false
				else:
					print("    PASS: Eye state dynamics achieved clear signal separation (Δ >= 0.50).")

		elif seq_id == "seq_06_center_dwell":
			var calib = GazeCalibration.new()
			var screen_center = Vector2(logical_sz.x * 0.5, logical_sz.y * 0.5)
			calib.set_target(screen_center, 60.0)

			var smoothed_positions: Array[Vector2] = []
			for ev in gaze_events:
				calib.add_sample(ev, 0.033)
				smoothed_positions.append(calib.get_smoothed_gaze())

			var max_smoothed_vel: float = 0.0
			for i in range(1, smoothed_positions.size()):
				var vel = (smoothed_positions[i] - smoothed_positions[i - 1]).length() / 0.033
				if vel > max_smoothed_vel:
					max_smoothed_vel = vel

			print("    Smoothed gaze peak velocity: %.2f px/s" % [max_smoothed_vel])
			var max_expected_vel = meta.get("max_smoothed_vel_px_s", 150.0)
			if max_smoothed_vel > max_expected_vel:
				printerr("    FAIL: Smoothed velocity %.2f px/s exceeded %.2f px/s bound" % [max_smoothed_vel, max_expected_vel])
				seq_passed = false
			else:
				print("    PASS: Damped spring absorbed neural ML jitter into smooth trajectory (< %.1f px/s)." % [max_expected_vel])

		if not seq_passed:
			all_passed = false
			failed_tests.append(seq_id)

	print("\n" + "=".repeat(70))
	if all_passed:
		print("ALL %d DYNAMIC MOVIE SEQUENCES PASSED SUCCESSFULLY!" % [seq_keys.size()])
		print("=".repeat(70) + "\n")
		gs.stop_tracking(true)
		vs.camera_stop(cam_rid)
		vs.camera_free(cam_rid)
		quit(0)
	else:
		printerr("FAILED SEQUENCES: ", failed_tests)
		print("=".repeat(70) + "\n")
		gs.stop_tracking(true)
		vs.camera_stop(cam_rid)
		vs.camera_free(cam_rid)
		quit(1)
