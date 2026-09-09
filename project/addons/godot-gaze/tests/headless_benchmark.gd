extends SceneTree

# ==============================================================================
# headless_benchmark.gd
#
# Automated Visual Gaze Benchmark Harness
# - Evaluates all 20 canonical test fixtures against test_assets/gaze_benchmark_golden.json
# - Generates side-by-side diagnostic overlays with oriented face ROI boxes
# - Exports standalone 60x60 eye crops
# - Outputs candidate JSON: build/tests/artifacts/gaze_benchmark_current.json
# - Generates self-contained HTML report: build/tests/artifacts/benchmark_report.html
# ==============================================================================

const SCREEN_WIDTH_MM: float = 301.5
const SCREEN_HEIGHT_MM: float = 188.5
const SCREEN_WIDTH_PT: float = 1512.0
const SCREEN_HEIGHT_PT: float = 982.0

const WIN_WIDTH_PT: float = 756.0
const WIN_HEIGHT_PT: float = 491.0
const WIN_POS_X_PT: float = 378.0
const WIN_POS_Y_PT: float = 245.5

const CAM_GEOM_1440 := {
	"res": Vector2i(1440, 960),
	"hfov_deg": 65.0
}
const CAM_GEOM_1024 := {
	"res": Vector2i(1024, 682),
	"hfov_deg": 65.0
}

const TARGET_CENTER := {
	"relative_to": Vector2(0.0, -50.0),
	"nose": Vector2(0.0, 0.0),
	"eye": Vector2(0.0, 0.0),
}
const TARGET_LEFT_LEFT := {
	"relative_to": Vector2(0.0, -50.0),
	"nose": Vector2(-150.75, 0.0),
	"eye": Vector2(-150.75, 0.0),
}
const TARGET_RIGHT_RIGHT := {
	"relative_to": Vector2(0.0, -50.0),
	"nose": Vector2(150.75, 0.0),
	"eye": Vector2(150.75, 0.0),
}
const TARGET_TOP_TOP := {
	"relative_to": Vector2(0.0, -50.0),
	"nose": Vector2(0.0, -94.25),
	"eye": Vector2(0.0, -94.25),
}
const TARGET_DOWN_DOWN := {
	"relative_to": Vector2(0.0, -50.0),
	"nose": Vector2(0.0, 94.25),
	"eye": Vector2(0.0, 94.25),
}
const TARGET_NOSEDOWN_EYESUP := {
	"relative_to": Vector2(0.0, -50.0),
	"nose": Vector2(0.0, 94.25),
	"eye": Vector2(0.0, -94.25),
}
const TARGET_NOSELEFT_EYESRIGHT := {
	"relative_to": Vector2(0.0, -50.0),
	"nose": Vector2(-150.75, 0.0),
	"eye": Vector2(150.75, 0.0),
}
const TARGET_NOSERIGHT_EYESLEFT := {
	"relative_to": Vector2(0.0, -50.0),
	"nose": Vector2(150.75, 0.0),
	"eye": Vector2(-150.75, 0.0),
}
const TARGET_NOSETOP_EYESDOWN := {
	"relative_to": Vector2(0.0, -50.0),
	"nose": Vector2(0.0, -94.25),
	"eye": Vector2(0.0, 94.25),
}

const FIXTURE_METADATA: Dictionary = {
	"self_center.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_CENTER, "is_eye_test": false},
	"self_center2.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_CENTER, "is_eye_test": false},
	"self_left_left.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_LEFT_LEFT, "is_eye_test": false},
	"self_right_right.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_RIGHT_RIGHT, "is_eye_test": false},
	"self_top_top.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_TOP_TOP, "is_eye_test": false},
	"self_down_down.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_DOWN_DOWN, "is_eye_test": false},
	"self_nosedown_eyesup.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_NOSEDOWN_EYESUP, "is_eye_test": false},
	"self_noseleft_eyesright.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_NOSELEFT_EYESRIGHT, "is_eye_test": false},
	"self_noseright_eyesleft.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_NOSERIGHT_EYESLEFT, "is_eye_test": false},
	"self_nosetop_eyesdown.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_NOSETOP_EYESDOWN, "is_eye_test": false},
	"self_roll_left.jpg": {"cam_geom": CAM_GEOM_1024, "roll_hint_deg": -25.0, "target": TARGET_CENTER, "is_eye_test": false},
	"self_roll_right.jpg": {"cam_geom": CAM_GEOM_1024, "roll_hint_deg": 25.0, "target": TARGET_CENTER, "is_eye_test": false},
	"self_yaw_left_roll_left.jpg": {"cam_geom": CAM_GEOM_1024, "roll_hint_deg": 25.0, "target": TARGET_LEFT_LEFT, "is_eye_test": false},
	"self_yaw_right_roll_left.jpg": {"cam_geom": CAM_GEOM_1024, "roll_hint_deg": 25.0, "target": TARGET_RIGHT_RIGHT, "is_eye_test": false},
	"eyes_both_open.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_CENTER, "is_eye_test": true},
	"eyes_both_wink.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_CENTER, "is_eye_test": true},
	"eyes_anatomical_left_wink.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_CENTER, "is_eye_test": true},
	"eyes_anatomical_right_wink.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_CENTER, "is_eye_test": true},
	"eyes_tilted_anatomical_right_wink.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_CENTER, "is_eye_test": true},
	"eyes_anatomical_left_obscured.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_CENTER, "is_eye_test": true},
}

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

func format_vec2(v: Vector2) -> String:
	return "(%.1f,%.1f)" % [v.x, v.y]

func format_vec3(v: Vector3) -> String:
	return "(%.1f,%.1f,%.1f)" % [v.x, v.y, v.z]

func px_to_screen_mm(px: Vector2) -> Vector2:
	if px.x == INF or px.y == INF or px.x <= -9000.0 or px.y <= -9000.0:
		return Vector2(-9999.0, -9999.0)
	var x_mm = (px.x - SCREEN_WIDTH_PT * 0.5) * (SCREEN_WIDTH_MM / SCREEN_WIDTH_PT)
	var y_mm = (px.y - SCREEN_HEIGHT_PT * 0.5) * (SCREEN_HEIGHT_MM / SCREEN_HEIGHT_PT)
	return Vector2(x_mm, y_mm)

func mm_to_screen_pt(pos_mm_from_center: Vector2) -> Vector2:
	var x_pt = (pos_mm_from_center.x * (SCREEN_WIDTH_PT / SCREEN_WIDTH_MM)) + (SCREEN_WIDTH_PT * 0.5)
	var y_pt = (pos_mm_from_center.y * (SCREEN_HEIGHT_PT / SCREEN_HEIGHT_MM)) + (SCREEN_HEIGHT_PT * 0.5)
	return Vector2(x_pt, y_pt)

func project_cam_3d_to_img_2d(p3d: Vector3, img_w: float, img_h: float, focal: float) -> Vector2:
	var z_depth = -p3d.z
	if z_depth <= 1.0:
		return Vector2(-9999.0, -9999.0)
	var u = (img_w * 0.5) + (focal * p3d.x) / z_depth
	var v = (img_h * 0.5) - (focal * p3d.y) / z_depth
	return Vector2(u, v)

func is_valid_pt(p: Vector2) -> bool:
	return p.x > -9000.0 and p.y > -9000.0 and not is_nan(p.x) and not is_nan(p.y) and not is_inf(p.x) and not is_inf(p.y)

func run_benchmark():
	print("=================== GODOT HEADLESS GAZE BENCHMARK ===================")

	var vs = Engine.get_singleton("VisionServer")
	var gs = Engine.get_singleton("GazeServer")

	if not vs or not gs:
		printerr("FATAL: VisionServer or GazeServer not available!")
		quit(1)
		return

	var profile = GazeDeviceProfile.new()
	profile.set_logical_size_px(Vector2i(1512, 982))
	profile.set_physical_size_mm(Vector2(301.5, 188.5))
	profile.set_camera_offset_mm(Vector3(0.0, 0.0, 0.0))
	profile.set_camera_roll_deg(0.0)
	gs.set_device_profile(profile)

	var cam_rid = vs.camera_create()
	vs.camera_set_device_id(cam_rid, -1)
	vs.camera_set_resolution(cam_rid, 1440, 960)
	var default_focal = 1440.0 / (2.0 * tan(deg_to_rad(65.0) * 0.5))
	vs.camera_set_focal_length(cam_rid, default_focal)
	vs.camera_start(cam_rid)

	gs.set_camera_offsets(Vector3(0.0, 0.0, 0.0), 0.0)
	gs.set_camera_vision_rid(cam_rid)
	gs.set_smoother(null)
	gs.set_crop_requested(true)
	gs.set_camera_preview_requested(true)

	gs.start_processing()
	print("GazeServer initialized successfully for headless benchmark.")

	# Load Tracked Goldenfile (Read-Only)
	var golden_file = "test_assets/gaze_benchmark_golden.json"
	if not FileAccess.file_exists(golden_file):
		golden_file = "../test_assets/gaze_benchmark_golden.json"
	if not FileAccess.file_exists(golden_file):
		printerr("FATAL: Benchmark goldenfile missing: ", golden_file)
		quit(1)
		return
		
	var golden_file_obj = FileAccess.open(golden_file, FileAccess.READ)
	var golden_json_text = golden_file_obj.get_as_text()
	golden_file_obj.close()
	var json_res = JSON.parse_string(golden_json_text)
	if json_res == null:
		printerr("FATAL: Failed to parse benchmark goldenfile JSON: ", golden_file)
		quit(1)
		return
	var golden_data: Dictionary = json_res

	# Prepare output directories
	var global_artifacts_dir = ProjectSettings.globalize_path("res://../build/tests/artifacts")
	var overlays_dir = global_artifacts_dir + "/benchmark_overlays"
	if DirAccess.dir_exists_absolute(overlays_dir):
		var dir = DirAccess.open(overlays_dir)
		if dir:
			dir.list_dir_begin()
			var fn = dir.get_next()
			while fn != "":
				if not dir.current_is_dir():
					dir.remove(fn)
				fn = dir.get_next()
			dir.list_dir_end()
	DirAccess.make_dir_recursive_absolute(overlays_dir)

	var report_lines = [
		"# Gaze Benchmark Report",
		"Generated on: " + Time.get_datetime_string_from_system(true) + "Z",
		"",
		"| Image File | Property | Current Value | Error | Previous Error | Delta | Status |",
		"| :--- | :--- | :--- | :--- | :--- | :--- | :--- |"
	]

	var any_metric_regressed = false
	var current_results_json: Dictionary = {}
	var html_table_rows: Array[Dictionary] = []
	var gallery_cards: Array[Dictionary] = []

	var total_fixtures_count = 0
	var passed_fixtures_count = 0
	var regressed_fixtures_count = 0

	for img_file in FIXTURE_METADATA.keys():
		total_fixtures_count += 1
		var meta = FIXTURE_METADATA[img_file]
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

		var cam_geom = meta.get("cam_geom", CAM_GEOM_1440)
		var expected_w = cam_geom.res.x
		var expected_h = cam_geom.res.y
		assert(img.get_width() == expected_w and img.get_height() == expected_h, "Fixture %s size mismatch" % img_file)
		var frame_focal = float(expected_w) / (2.0 * tan(deg_to_rad(cam_geom.hfov_deg) * 0.5))
		vs.camera_set_resolution(cam_rid, expected_w, expected_h)
		vs.camera_set_focal_length(cam_rid, frame_focal)

		gs.reset()
		if meta.has("roll_hint_deg"):
			gs.set_roll_hint(deg_to_rad(meta.roll_hint_deg))

		print("Processing benchmark frame: ", img_file, " (", img.get_width(), "x", img.get_height(), ")")

		var tex = ImageTexture.create_from_image(img)
		vs.inject_texture(cam_rid, tex)
		gs.trigger_process()
		await gs.gaze_frame_ready

		# Get tracking outputs from GazeServer
		var face_detected = gs.is_face_detected()
		var head_trans = gs.get_head_position()
		var head_rot = gs.get_head_pose_euler_deg()
		var head_xform = gs.get_head_transform()
		var head_fwd = -head_xform.basis.z.normalized()
		var lm_pts = gs.get_face_landmarks_2d()
		var left_open = gs.get_left_eye_openness()
		var right_open = gs.get_right_eye_openness()
		var eye_crops = gs.get_eye_crops()
		var left_crop: Image = eye_crops[0] if (face_detected and eye_crops.size() > 0) else null
		var right_crop: Image = eye_crops[1] if (face_detected and eye_crops.size() > 1) else null

		var eye_orig = gs.get_gaze_origin()
		if eye_orig == Vector3.ZERO:
			eye_orig = head_trans + Vector3(0.0, 30.0, 0.0)
		var gaze_dir = gs.get_gaze_direction()
		if gaze_dir == Vector3.ZERO:
			gaze_dir = head_fwd

		var nose_proj_px = gs.project_ray_to_viewport(head_trans, head_fwd)
		var gaze_proj_px = gs.get_projected_gaze(false)
		var nose_proj_mm = px_to_screen_mm(nose_proj_px)
		var gaze_proj_mm = gs.get_projected_gaze_mm(false)

		var target_info = meta.get("target", TARGET_CENTER)
		var rel_offset = target_info.get("relative_to", Vector2(0.0, -50.0))
		var nose_target = rel_offset + target_info.get("nose", Vector2(0.0, 0.0))
		var gaze_target = rel_offset + target_info.get("eye", Vector2(0.0, 0.0))

		# Compute rotation target error
		var P_cam_target = Vector3(nose_target.x, -(nose_target.y + 94.25), 0.0)
		var diff_vec = P_cam_target - head_trans
		var rot_err_str = "N/A"
		var rot_err_mag = 0.0
		if diff_vec.length() > 0.0 and face_detected:
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
		var nose_err_mag = sqrt(nose_diff.x * nose_diff.x + nose_diff.y * nose_diff.y) if is_valid_pt(nose_proj_mm) else 9999.0

		var gaze_diff = gaze_proj_mm - gaze_target
		var gaze_err_mag = sqrt(gaze_diff.x * gaze_diff.x + gaze_diff.y * gaze_diff.y) if is_valid_pt(gaze_proj_mm) else 9999.0

		var fixture_regressed = false
		var fixture_record: Dictionary = {}

		var props = [
			{"prop": "head_pos_mm", "val": format_vec3(head_trans) if face_detected else "NO_FACE", "err_str": "N/A", "err_mag": 0.0},
			{"prop": "head_rot_deg", "val": format_vec3(head_rot) if face_detected else "NO_FACE", "err_str": rot_err_str, "err_mag": rot_err_mag},
			{"prop": "nose_mm", "val": format_vec2(nose_proj_mm) if face_detected else "NO_FACE", "err_str": ("%.1f mm" % nose_err_mag) if is_valid_pt(nose_proj_mm) else "N/A", "err_mag": nose_err_mag if is_valid_pt(nose_proj_mm) else 0.0},
			{"prop": "gaze_mm", "val": format_vec2(gaze_proj_mm) if face_detected else "NO_FACE", "err_str": ("%.1f mm" % gaze_err_mag) if is_valid_pt(gaze_proj_mm) else "N/A", "err_mag": gaze_err_mag if is_valid_pt(gaze_proj_mm) else 0.0}
		]

		var golden_fixture = golden_data.get(img_file, {})

		for p in props:
			var prop_key = p["prop"]
			fixture_record[prop_key] = {"val": p["val"], "err": p["err_str"], "err_mag": p["err_mag"]}

			var prev_err_str = "N/A"
			var delta_str = "0.0 mm"
			var status_str = "OK"

			if golden_fixture.has(prop_key):
				var prev_entry = golden_fixture[prop_key]
				prev_err_str = prev_entry.get("err", "N/A")
				var prev_mag = prev_entry.get("err_mag", 0.0)

				if prev_mag > 0.0 and p["err_mag"] > 0.0 and not meta.get("is_eye_test", false):
					var delta = p["err_mag"] - prev_mag
					if delta > 15.0:
						delta_str = "+%.1f mm" % delta
						status_str = "REGRESSION"
						any_metric_regressed = true
						fixture_regressed = true
						printerr("REGRESSION: %s %s error regressed from %s to %s (delta: %s)" % [img_file, prop_key, prev_err_str, p["err_str"], delta_str])
					elif delta < -15.0:
						delta_str = "%.1f mm" % delta
						status_str = "IMPROVEMENT"
					else:
						delta_str = "0.0 mm"

			report_lines.append("| %s | %s | %s | %s | %s | %s | %s |" % [
				img_file, prop_key, p["val"], p["err_str"], prev_err_str, delta_str, status_str
			])

		current_results_json[img_file] = fixture_record

		if fixture_regressed:
			regressed_fixtures_count += 1
		else:
			passed_fixtures_count += 1

		# Export Standalone Eye Crops
		var base_fn = img_file.get_basename()
		var left_eye_rel_path = ""
		var right_eye_rel_path = ""
		if left_crop:
			var lc_png = overlays_dir + "/" + base_fn + "_left_eye.png"
			left_crop.save_png(lc_png)
			left_eye_rel_path = "benchmark_overlays/" + base_fn + "_left_eye.png"
		if right_crop:
			var rc_png = overlays_dir + "/" + base_fn + "_right_eye.png"
			right_crop.save_png(rc_png)
			right_eye_rel_path = "benchmark_overlays/" + base_fn + "_right_eye.png"

		# Render Diagnostic Overlays
		var diag_overlay_rel_path = "benchmark_overlays/" + base_fn + "_diagnostic.png"
		var left_panel = render_camera_panel(img, head_trans, head_xform, eye_orig, gaze_dir, lm_pts, face_detected, meta.get("roll_hint_deg", 0.0), frame_focal, left_crop, right_crop)
		var right_panel = render_display_panel(img, img_file, mm_to_screen_pt(nose_target), mm_to_screen_pt(gaze_target), nose_proj_px, gaze_proj_px)
		var composite = create_side_by_side_composite(left_panel, right_panel, img_file)
		composite.save_png(overlays_dir + "/" + base_fn + "_diagnostic.png")

		html_table_rows.append({
			"file": img_file,
			"face_detected": face_detected,
			"head_rot": format_vec3(head_rot) if face_detected else "N/A",
			"nose_err": ("%.1f mm" % nose_err_mag) if is_valid_pt(nose_proj_mm) else "N/A",
			"gaze_err": ("%.1f mm" % gaze_err_mag) if is_valid_pt(gaze_proj_mm) else "N/A",
			"left_open": "%.2f" % left_open,
			"right_open": "%.2f" % right_open,
			"status": "REGRESSED" if fixture_regressed else ("OK" if face_detected else "NO_FACE"),
			"overlay_rel": diag_overlay_rel_path
		})

		gallery_cards.append({
			"file": img_file,
			"status": "REGRESSED" if fixture_regressed else ("OK" if face_detected else "NO_FACE"),
			"roll_hint": meta.get("roll_hint_deg", 0.0),
			"left_open": left_open,
			"right_open": right_open,
			"nose_err": nose_err_mag if is_valid_pt(nose_proj_mm) else 0.0,
			"gaze_err": gaze_err_mag if is_valid_pt(gaze_proj_mm) else 0.0,
			"overlay_rel": diag_overlay_rel_path,
			"left_eye_rel": left_eye_rel_path,
			"right_eye_rel": right_eye_rel_path
		})

	# Save Candidate JSON Results
	var current_json_path = global_artifacts_dir + "/gaze_benchmark_current.json"
	var cj_file = FileAccess.open(current_json_path, FileAccess.WRITE)
	if cj_file:
		cj_file.store_string(JSON.stringify(current_results_json, "  "))
		cj_file.close()

	# Save Markdown Report
	var full_report = "\n".join(report_lines) + "\n"
	var parent_out_path = global_artifacts_dir + "/gaze_benchmark_report.md"
	var parent_file = FileAccess.open(parent_out_path, FileAccess.WRITE)
	if parent_file:
		parent_file.store_string(full_report)
		parent_file.close()

	# Generate HTML Dashboard Report
	var html_report_path = global_artifacts_dir + "/benchmark_report.html"
	generate_html_report(html_report_path, html_table_rows, gallery_cards, total_fixtures_count, passed_fixtures_count, regressed_fixtures_count, any_metric_regressed)

	print("\n" + full_report)
	print("Benchmark candidate JSON written to: ", current_json_path)
	print("Benchmark HTML report written to:     ", html_report_path)

	gs.stop_tracking(true)
	vs.camera_stop(cam_rid)
	vs.camera_free(cam_rid)

	# Assert that HTML report and overlays were successfully generated
	assert(FileAccess.file_exists(html_report_path), "HTML report was not generated!")

	if any_metric_regressed:
		printerr("\n==========================================================")
		printerr("GAZE BENCHMARK FAILED: METRICS REGRESSED FROM GOLDENFILE!")
		printerr("Visual Report:    ", html_report_path)
		printerr("Candidate Golden: ", current_json_path)
		printerr("")
		printerr("NOTE: Golden baselines must NOT be updated without human review.")
		printerr("If regressions are verified and approved by project leadership:")
		printerr("Run: python3 scripts/promote_benchmark_golden.py")
		printerr("==========================================================\n")
		quit(1)
	else:
		print("\n==========================================================")
		print("GAZE BENCHMARK SUCCEEDED: ALL METRICS MATCH GOLDENFILE!")
		print("Visual Report: ", html_report_path)
		print("==========================================================\n")
		quit(0)

# ==============================================================================
# Rendering Routines: Camera Panel & Display Mockup
# ==============================================================================

func render_camera_panel(
	img: Image, head_pos: Vector3, head_xform: Transform3D,
	eye_orig: Vector3, gaze_dir: Vector3, lm_pts: PackedVector2Array,
	face_detected: bool, roll_hint_deg: float, focal: float,
	left_crop: Image = null, right_crop: Image = null
) -> Image:
	var panel = img.duplicate()
	panel.convert(Image.FORMAT_RGBA8)
	var w = float(panel.get_width())
	var h = float(panel.get_height())

	if face_detected:
		# 1. Draw 2D Facial Landmarks
		for pt in lm_pts:
			draw_circle(panel, int(pt.x), int(pt.y), 3, Color(0.0, 0.9, 1.0, 0.9))

		# 2. Draw Oriented Face ROI Bounding Box
		if lm_pts.size() >= 5:
			draw_oriented_face_bbox(panel, lm_pts, roll_hint_deg)

		# 3. 10cm (100mm) Nose Transform Axis (+X Red, +Y Green, -Z Lime towards camera)
		var nose_origin_2d = project_cam_3d_to_img_2d(head_pos, w, h, focal)
		var axis_len = 100.0

		var nose_x_3d = head_pos + head_xform.basis.x.normalized() * axis_len
		var nose_y_3d = head_pos + head_xform.basis.y.normalized() * axis_len
		var nose_z_3d = head_pos - head_xform.basis.z.normalized() * axis_len

		var nose_x_2d = project_cam_3d_to_img_2d(nose_x_3d, w, h, focal)
		var nose_y_2d = project_cam_3d_to_img_2d(nose_y_3d, w, h, focal)
		var nose_z_2d = project_cam_3d_to_img_2d(nose_z_3d, w, h, focal)

		if is_valid_pt(nose_origin_2d):
			if is_valid_pt(nose_x_2d):
				draw_thick_line(panel, int(nose_origin_2d.x), int(nose_origin_2d.y), int(nose_x_2d.x), int(nose_x_2d.y), Color.RED, 4)
			if is_valid_pt(nose_y_2d):
				draw_thick_line(panel, int(nose_origin_2d.x), int(nose_origin_2d.y), int(nose_y_2d.x), int(nose_y_2d.y), Color.GREEN, 4)
			if is_valid_pt(nose_z_2d):
				draw_thick_line(panel, int(nose_origin_2d.x), int(nose_origin_2d.y), int(nose_z_2d.x), int(nose_z_2d.y), Color(0.1, 1.0, 0.2, 1.0), 5)
			draw_circle(panel, int(nose_origin_2d.x), int(nose_origin_2d.y), 6, Color.WHITE)

		# 4. 10cm Eye Gaze Axis (Teal)
		var eye_origin_2d = project_cam_3d_to_img_2d(eye_orig, w, h, focal)
		var eye_fwd_3d = eye_orig + gaze_dir.normalized() * axis_len
		var eye_fwd_2d = project_cam_3d_to_img_2d(eye_fwd_3d, w, h, focal)

		if is_valid_pt(eye_origin_2d):
			if is_valid_pt(eye_fwd_2d):
				draw_thick_line(panel, int(eye_origin_2d.x), int(eye_origin_2d.y), int(eye_fwd_2d.x), int(eye_fwd_2d.y), Color(0.0, 0.95, 0.95, 1.0), 5)
			draw_circle(panel, int(eye_origin_2d.x), int(eye_origin_2d.y), 6, Color.CYAN)

	# Panel Header Bar
	draw_filled_rect(panel, 0, 0, int(w), 36, Color(0.05, 0.05, 0.08, 0.90))
	draw_bitmap_text(panel, 14, 10, "CAMERA VIEW, ORIENTED ROI & 3D AXES", Color.WHITE, 2)
	draw_rect(panel, 0, 0, int(w), int(h), Color(0.3, 0.3, 0.35, 1.0), 2)

	# Draw Inset Eye Crop Cards at bottom-left
	if face_detected and (left_crop != null or right_crop != null):
		var card_y = int(h) - 170
		var card_w = 340
		var card_h = 154
		draw_filled_rect(panel, 16, card_y, card_w, card_h, Color(0.05, 0.05, 0.08, 0.92))
		draw_rect(panel, 16, card_y, card_w, card_h, Color(0.3, 0.6, 0.9, 1.0), 2)
		draw_bitmap_text(panel, 26, card_y + 10, "MODEL EYE CROP INPUTS (60x60)", Color(0.3, 0.8, 1.0, 1.0), 1)

		var crop_display_sz = 100

		# Left Card: Image Left Eye (p0..p1 / Anatomical Right)
		if right_crop:
			var rc = right_crop.duplicate()
			rc.convert(Image.FORMAT_RGBA8)
			rc.resize(crop_display_sz, crop_display_sz, Image.INTERPOLATE_BILINEAR)
			panel.blit_rect(rc, Rect2i(0, 0, crop_display_sz, crop_display_sz), Vector2i(28, card_y + 32))
			draw_rect(panel, 28, card_y + 32, crop_display_sz, crop_display_sz, Color.CYAN, 1)
			draw_bitmap_text(panel, 36, card_y + 136, "IMG LEFT (p0-1)", Color.WHITE, 1)

		# Right Card: Image Right Eye (p2..p3 / Anatomical Left)
		if left_crop:
			var lc = left_crop.duplicate()
			lc.convert(Image.FORMAT_RGBA8)
			lc.resize(crop_display_sz, crop_display_sz, Image.INTERPOLATE_BILINEAR)
			panel.blit_rect(lc, Rect2i(0, 0, crop_display_sz, crop_display_sz), Vector2i(180, card_y + 32))
			draw_rect(panel, 180, card_y + 32, crop_display_sz, crop_display_sz, Color.CYAN, 1)
			draw_bitmap_text(panel, 184, card_y + 136, "IMG RIGHT (p2-3)", Color.WHITE, 1)

	return panel

func draw_oriented_face_bbox(panel: Image, lm_pts: PackedVector2Array, roll_deg: float):
	var center = Vector2.ZERO
	for pt in lm_pts:
		center += pt
	center /= float(lm_pts.size())

	var rad = deg_to_rad(-roll_deg)
	var cos_a = cos(rad)
	var sin_a = sin(rad)

	var min_u = 9999.0
	var max_u = -9999.0
	var min_v = 9999.0
	var max_v = -9999.0

	for pt in lm_pts:
		var rel = pt - center
		var u = rel.x * cos_a - rel.y * sin_a
		var v = rel.x * sin_a + rel.y * cos_a
		min_u = min(min_u, u)
		max_u = max(max_u, u)
		min_v = min(min_v, v)
		max_v = max(max_v, v)

	var pad_u = (max_u - min_u) * 0.25
	var pad_v = (max_v - min_v) * 0.30
	min_u -= pad_u
	max_u += pad_u
	min_v -= pad_v
	max_v += pad_v

	var corners_local = [
		Vector2(min_u, min_v),
		Vector2(max_u, min_v),
		Vector2(max_u, max_v),
		Vector2(min_u, max_v)
	]

	var unrad = deg_to_rad(roll_deg)
	var ucos = cos(unrad)
	var usin = sin(unrad)

	var corners_img = []
	for cl in corners_local:
		var rx = cl.x * ucos - cl.y * usin
		var ry = cl.x * usin + cl.y * ucos
		corners_img.append(center + Vector2(rx, ry))

	var col = Color(1.0, 0.85, 0.2, 0.9)
	for i in range(4):
		var p0 = corners_img[i]
		var p1 = corners_img[(i + 1) % 4]
		draw_thick_line(panel, int(p0.x), int(p0.y), int(p1.x), int(p1.y), col, 2)

func render_display_panel(
	img: Image, _img_name: String, target_nose_pt: Vector2, target_eye_pt: Vector2,
	nose_rep: Vector2, eye_rep: Vector2
) -> Image:
	var margin_x = 160
	var margin_y = 120
	var canvas_w = int(SCREEN_WIDTH_PT) + margin_x * 2
	var canvas_h = int(SCREEN_HEIGHT_PT) + margin_y * 2

	var canvas = Image.create(canvas_w, canvas_h, false, Image.FORMAT_RGBA8)
	canvas.fill(Color(0.08, 0.08, 0.10, 1.0))

	var screen_x = margin_x
	var screen_y = margin_y
	draw_filled_rect(canvas, screen_x, screen_y, int(SCREEN_WIDTH_PT), int(SCREEN_HEIGHT_PT), Color(0.14, 0.14, 0.17, 1.0))
	draw_rect(canvas, screen_x, screen_y, int(SCREEN_WIDTH_PT), int(SCREEN_HEIGHT_PT), Color(0.5, 0.5, 0.55, 1.0), 3)

	var cam_px_x = screen_x + int(SCREEN_WIDTH_PT * 0.5)
	var cam_px_y = screen_y + 4
	draw_filled_rect(canvas, cam_px_x - 30, screen_y, 60, 12, Color(0.05, 0.05, 0.07, 1.0))
	draw_circle(canvas, cam_px_x, cam_px_y, 4, Color(0.1, 0.8, 1.0, 0.8))

	var win_w = 756
	var win_h = 491
	var win_x = screen_x + int(WIN_POS_X_PT)
	var win_y = screen_y + int(WIN_POS_Y_PT)

	var titlebar_h = 28
	draw_filled_rect(canvas, win_x, win_y, win_w, win_h, Color(0.10, 0.10, 0.12, 1.0))
	draw_rect(canvas, win_x, win_y, win_w, win_h, Color(0.35, 0.55, 0.85, 1.0), 2)
	draw_filled_rect(canvas, win_x, win_y, win_w, titlebar_h, Color(0.18, 0.18, 0.22, 1.0))

	draw_circle(canvas, win_x + 16, win_y + 14, 5, Color(1.0, 0.38, 0.35, 1.0))
	draw_circle(canvas, win_x + 32, win_y + 14, 5, Color(1.0, 0.75, 0.25, 1.0))
	draw_circle(canvas, win_x + 48, win_y + 14, 5, Color(0.30, 0.85, 0.40, 1.0))

	var interior_x = win_x + 2
	var interior_y = win_y + titlebar_h + 1
	var interior_w = win_w - 4
	var interior_h = win_h - titlebar_h - 3
	draw_filled_rect(canvas, interior_x, interior_y, interior_w, interior_h, Color(0.06, 0.06, 0.08, 1.0))

	var aspect_src = float(img.get_width()) / float(img.get_height())
	var aspect_dst = float(interior_w) / float(interior_h)
	var fit_w = interior_w
	var fit_h = interior_h
	if aspect_src > aspect_dst:
		fit_h = int(float(interior_w) / aspect_src)
	else:
		fit_w = int(float(interior_h) * aspect_src)

	var fit_x = interior_x + (interior_w - fit_w) / 2
	var fit_y = interior_y + (interior_h - fit_h) / 2

	var mirrored_img = img.duplicate()
	mirrored_img.convert(Image.FORMAT_RGBA8)
	mirrored_img.flip_x()
	mirrored_img.resize(fit_w, fit_h, Image.INTERPOLATE_BILINEAR)
	canvas.blit_rect(mirrored_img, Rect2i(0, 0, fit_w, fit_h), Vector2i(fit_x, fit_y))
	draw_rect(canvas, fit_x, fit_y, fit_w, fit_h, Color(0.2, 0.5, 0.8, 0.4), 1)

	var to_canvas = func(p_screen: Vector2) -> Vector2i:
		return Vector2i(screen_x + int(p_screen.x), screen_y + int(p_screen.y))

	# 1. Target Points
	if is_valid_pt(target_nose_pt):
		var cp = to_canvas.call(target_nose_pt)
		draw_circle(canvas, cp.x, cp.y, 8, Color.RED)
		draw_crosshair(canvas, cp.x, cp.y, 16, Color.RED)

	var targets_differ = (target_nose_pt - target_eye_pt).length() > 5.0
	if is_valid_pt(target_eye_pt) and targets_differ:
		var cp = to_canvas.call(target_eye_pt)
		draw_circle(canvas, cp.x, cp.y, 8, Color(1.0, 0.65, 0.0, 1.0))
		draw_crosshair(canvas, cp.x, cp.y, 16, Color(1.0, 0.65, 0.0, 1.0))

	# 2. Reported Gaze Rays
	if is_valid_pt(nose_rep):
		var cp = to_canvas.call(nose_rep)
		draw_thick_line(canvas, cam_px_x, cam_px_y, cp.x, cp.y, Color(0.2, 1.0, 0.3, 0.6), 2)
		draw_circle(canvas, cp.x, cp.y, 8, Color(0.2, 1.0, 0.3, 1.0))
		draw_crosshair(canvas, cp.x, cp.y, 14, Color.WHITE)

	if is_valid_pt(eye_rep):
		var cp = to_canvas.call(eye_rep)
		draw_thick_line(canvas, cam_px_x, cam_px_y, cp.x, cp.y, Color(0.0, 0.95, 1.0, 0.6), 2)
		draw_circle(canvas, cp.x, cp.y, 8, Color(0.0, 0.95, 1.0, 1.0))
		draw_crosshair(canvas, cp.x, cp.y, 14, Color.WHITE)

	return canvas

func create_side_by_side_composite(left_panel: Image, right_panel: Image, title: String) -> Image:
	var target_h = 1080
	var scale_l = float(target_h) / float(left_panel.get_height())
	var scale_r = float(target_h) / float(right_panel.get_height())

	var scaled_l = left_panel.duplicate()
	scaled_l.convert(Image.FORMAT_RGBA8)
	scaled_l.resize(int(left_panel.get_width() * scale_l), target_h, Image.INTERPOLATE_BILINEAR)

	var scaled_r = right_panel.duplicate()
	scaled_r.convert(Image.FORMAT_RGBA8)
	scaled_r.resize(int(right_panel.get_width() * scale_r), target_h, Image.INTERPOLATE_BILINEAR)

	var total_w = scaled_l.get_width() + scaled_r.get_width() + 16
	var header_h = 44
	var comp = Image.create(total_w, target_h + header_h, false, Image.FORMAT_RGBA8)
	comp.fill(Color(0.04, 0.04, 0.06, 1.0))

	draw_filled_rect(comp, 0, 0, total_w, header_h, Color(0.08, 0.08, 0.12, 1.0))
	draw_rect(comp, 0, 0, total_w, header_h, Color(0.25, 0.25, 0.35, 1.0), 1)
	draw_bitmap_text(comp, 20, 14, "DIAGNOSTIC VISUALIZATION: " + title, Color(0.9, 0.9, 0.95, 1.0), 2)

	comp.blit_rect(scaled_l, Rect2i(0, 0, scaled_l.get_width(), target_h), Vector2i(0, header_h))
	comp.blit_rect(scaled_r, Rect2i(0, 0, scaled_r.get_width(), target_h), Vector2i(scaled_l.get_width() + 16, header_h))
	draw_thick_line(comp, scaled_l.get_width() + 8, header_h, scaled_l.get_width() + 8, target_h + header_h, Color(0.3, 0.3, 0.4, 1.0), 2)

	return comp

# ==============================================================================
# HTML Report Generator
# ==============================================================================

func generate_html_report(
	out_path: String, table_rows: Array[Dictionary], cards: Array[Dictionary],
	total_cnt: int, passed_cnt: int, regressed_cnt: int, has_regression: bool
):
	var status_class = "status-failed" if has_regression else "status-passed"
	var status_text = "FAILED (REGRESSIONS DETECTED)" if has_regression else "PASSED (ALL MATCH GOLDENFILE)"

	var html = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Godot-Gaze Visual Benchmark Report</title>
<style>
  :root {
    --bg: #0d1117;
    --card-bg: #161b22;
    --border: #30363d;
    --text: #c9d1d9;
    --text-bright: #f0f6fc;
    --green: #2ea043;
    --red: #f85149;
    --blue: #58a6ff;
    --cyan: #39c5bb;
    --yellow: #d29922;
  }
  body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
    background-color: var(--bg);
    color: var(--text);
    margin: 0;
    padding: 24px;
  }
  .container { max-width: 1400px; margin: 0 auto; }
  .header-card {
    background: var(--card-bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 24px;
    margin-bottom: 24px;
    display: flex;
    justify-content: space-between;
    align-items: center;
  }
  .header-title h1 { margin: 0 0 8px 0; color: var(--text-bright); font-size: 24px; }
  .header-title p { margin: 0; color: #8b949e; font-size: 14px; }
  .badge {
    padding: 6px 14px;
    border-radius: 20px;
    font-weight: 600;
    font-size: 14px;
    display: inline-block;
  }
  .status-passed { background: rgba(46, 160, 67, 0.2); color: var(--green); border: 1px solid var(--green); }
  .status-failed { background: rgba(248, 81, 73, 0.2); color: var(--red); border: 1px solid var(--red); }
  .badge-ok { background: rgba(46, 160, 67, 0.15); color: var(--green); }
  .badge-reg { background: rgba(248, 81, 73, 0.2); color: var(--red); }
  .badge-imp { background: rgba(88, 166, 255, 0.2); color: var(--blue); }
  .badge-noface { background: rgba(139, 148, 158, 0.2); color: #8b949e; }
  .kpi-row {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 16px;
    margin-bottom: 24px;
  }
  .kpi-card {
    background: var(--card-bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 16px;
    text-align: center;
  }
  .kpi-val { font-size: 28px; font-weight: 700; color: var(--text-bright); margin-top: 4px; }
  .kpi-lbl { font-size: 12px; text-transform: uppercase; color: #8b949e; letter-spacing: 0.5px; }
  table {
    width: 100%;
    border-collapse: collapse;
    background: var(--card-bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    overflow: hidden;
    margin-bottom: 32px;
  }
  th, td {
    padding: 12px 16px;
    text-align: left;
    border-bottom: 1px solid var(--border);
  }
  th { background: #21262d; color: var(--text-bright); font-size: 13px; font-weight: 600; }
  tr:last-child td { border-bottom: none; }
  tr:hover { background: rgba(255, 255, 255, 0.02); }
  .gallery-grid {
    display: grid;
    grid-template-columns: 1fr;
    gap: 24px;
  }
  .fixture-card {
    background: var(--card-bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 20px;
  }
  .fixture-card.regressed { border-color: var(--red); }
  .fixture-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 16px;
  }
  .fixture-title { font-size: 18px; font-weight: 600; color: var(--text-bright); }
  .fixture-body {
    display: grid;
    grid-template-columns: 180px 1fr;
    gap: 20px;
    align-items: start;
  }
  .eye-crops-col {
    display: flex;
    flex-direction: column;
    gap: 12px;
  }
  .eye-box {
    background: #0d1117;
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 8px;
    text-align: center;
  }
  .eye-box img { width: 100%; max-width: 120px; border-radius: 4px; display: block; margin: 0 auto 6px auto; }
  .eye-lbl { font-size: 11px; color: #8b949e; font-weight: 600; }
  .eye-val { font-size: 12px; color: var(--cyan); font-weight: bold; }
  .overlay-col img {
    width: 100%;
    border-radius: 6px;
    border: 1px solid var(--border);
    display: block;
  }
  a { color: var(--blue); text-decoration: none; }
  a:hover { text-decoration: underline; }
</style>
</head>
<body>
<div class="container">
  <div class="header-card">
    <div class="header-title">
      <h1>Godot-Gaze Visual Benchmark Report</h1>
      <p>Generated on """ + Time.get_datetime_string_from_system(true) + """Z &bull; 20 Fixture Suite</p>
    </div>
    <div class="badge """ + status_class + """">""" + status_text + """</div>
  </div>

  <div class="kpi-row">
    <div class="kpi-card"><div class="kpi-lbl">Total Fixtures</div><div class="kpi-val">""" + str(total_cnt) + """</div></div>
    <div class="kpi-card"><div class="kpi-lbl">Passed</div><div class="kpi-val" style="color: var(--green);">""" + str(passed_cnt) + """</div></div>
    <div class="kpi-card"><div class="kpi-lbl">Regressions</div><div class="kpi-val" style="color: """ + ("var(--red)" if regressed_cnt > 0 else "var(--text-bright)") + """;">""" + str(regressed_cnt) + """</div></div>
  </div>

  <h2>Benchmark Metrics Table</h2>
  <table>
    <thead>
      <tr>
        <th>Fixture Image</th>
        <th>Status</th>
        <th>Head Pose (deg)</th>
        <th>Nose Error</th>
        <th>Gaze Error</th>
        <th>Left Eye Open</th>
        <th>Right Eye Open</th>
        <th>Overlay Link</th>
      </tr>
    </thead>
    <tbody>
"""

	for r in table_rows:
		var badge_cls = "badge-ok"
		if r["status"] == "REGRESSED": badge_cls = "badge-reg"
		elif r["status"] == "NO_FACE": badge_cls = "badge-noface"

		html += """      <tr>
        <td><strong>""" + r["file"] + """</strong></td>
        <td><span class="badge """ + badge_cls + """">""" + r["status"] + """</span></td>
        <td><code>""" + r["head_rot"] + """</code></td>
        <td>""" + r["nose_err"] + """</td>
        <td>""" + r["gaze_err"] + """</td>
        <td>""" + r["left_open"] + """</td>
        <td>""" + r["right_open"] + """</td>
        <td><a href='""" + r["overlay_rel"] + """' target='_blank'>View Overlay</a></td>
      </tr>\n"""

	html += """    </tbody>
  </table>

  <h2>Diagnostic Visual Gallery</h2>
  <div class="gallery-grid">
"""

	for c in cards:
		var reg_card_cls = "regressed" if c["status"] == "REGRESSED" else ""
		var badge_cls = "badge-ok"
		if c["status"] == "REGRESSED": badge_cls = "badge-reg"
		elif c["status"] == "NO_FACE": badge_cls = "badge-noface"

		html += """    <div class="fixture-card """ + reg_card_cls + """">
      <div class="fixture-header">
        <div class="fixture-title">""" + c["file"] + """ <span style="font-size: 13px; color: #8b949e; margin-left: 8px;">(Roll Hint: """ + str(c["roll_hint"]) + """&deg;)</span></div>
        <div class="badge """ + badge_cls + """">""" + c["status"] + """</div>
      </div>
      <div class="fixture-body">
        <div class="eye-crops-col">
"""
		if c["left_eye_rel"] != "":
			html += """          <div class="eye-box">
            <img src='""" + c["left_eye_rel"] + """' alt='Left Eye' />
            <div class="eye-lbl">LEFT EYE (p0-1)</div>
            <div class="eye-val">""" + ("%.2f" % c["left_open"]) + """</div>
          </div>\n"""
		if c["right_eye_rel"] != "":
			html += """          <div class="eye-box">
            <img src='""" + c["right_eye_rel"] + """' alt='Right Eye' />
            <div class="eye-lbl">RIGHT EYE (p2-3)</div>
            <div class="eye-val">""" + ("%.2f" % c["right_open"]) + """</div>
          </div>\n"""
		if c["left_eye_rel"] == "" and c["right_eye_rel"] == "":
			html += """          <div class="eye-box" style="padding: 24px 8px; color: #8b949e; font-size: 11px;">
            NO FACE DETECTED
          </div>\n"""

		html += """        </div>
        <div class="overlay-col">
          <a href='""" + c["overlay_rel"] + """' target='_blank'>
            <img src='""" + c["overlay_rel"] + """' alt='Diagnostic Overlay' />
          </a>
        </div>
      </div>
    </div>\n"""

	html += """  </div>
</div>
</body>
</html>
"""

	var hf = FileAccess.open(out_path, FileAccess.WRITE)
	if hf:
		hf.store_string(html)
		hf.close()

# ==============================================================================
# Drawing Primitives & Crisp Bitmap Font Engine
# ==============================================================================

func draw_rect(img: Image, x: int, y: int, w: int, h: int, col: Color, thickness: int = 1):
	for t in range(thickness):
		for px in range(x + t, x + w - t):
			if px >= 0 and px < img.get_width():
				if y + t >= 0 and y + t < img.get_height():
					img.set_pixel(px, y + t, col)
				if y + h - 1 - t >= 0 and y + h - 1 - t < img.get_height():
					img.set_pixel(px, y + h - 1 - t, col)
		for py in range(y + t, y + h - t):
			if py >= 0 and py < img.get_height():
				if x + t >= 0 and x + t < img.get_width():
					img.set_pixel(x + t, py, col)
				if x + w - 1 - t >= 0 and x + w - 1 - t < img.get_width():
					img.set_pixel(x + w - 1 - t, py, col)

func draw_filled_rect(img: Image, x: int, y: int, w: int, h: int, col: Color):
	for py in range(y, y + h):
		if py >= 0 and py < img.get_height():
			for px in range(x, x + w):
				if px >= 0 and px < img.get_width():
					img.set_pixel(px, py, col)

func draw_crosshair(img: Image, cx: int, cy: int, size: int, col: Color):
	for d in range(-size, size + 1):
		var px = cx + d
		var py = cy + d
		if px >= 0 and px < img.get_width() and cy >= 0 and cy < img.get_height():
			img.set_pixel(px, cy, col)
		if cx >= 0 and cx < img.get_width() and py >= 0 and py < img.get_height():
			img.set_pixel(cx, py, col)

func draw_thick_line(img: Image, x0: int, y0: int, x1: int, y1: int, col: Color, thickness: int = 1):
	var dx = abs(x1 - x0)
	var dy = -abs(y1 - y0)
	var sx = 1 if x0 < x1 else -1
	var sy = 1 if y0 < y1 else -1
	var err = dx + dy
	var cx = x0
	var cy = y0
	var ht = thickness / 2
	while true:
		for tx in range(-ht, ht + 1):
			for ty in range(-ht, ht + 1):
				var px = cx + tx
				var py = cy + ty
				if px >= 0 and px < img.get_width() and py >= 0 and py < img.get_height():
					img.set_pixel(px, py, col)
		if cx == x1 and cy == y1:
			break
		var e2 = 2 * err
		if e2 >= dy:
			err += dy
			cx += sx
		if e2 <= dx:
			err += dx
			cy += sy

func draw_circle(img: Image, cx: int, cy: int, r: int, col: Color):
	for dy in range(-r, r + 1):
		for dx in range(-r, r + 1):
			if dx * dx + dy * dy <= r * r:
				var px = cx + dx
				var py = cy + dy
				if px >= 0 and px < img.get_width() and py >= 0 and py < img.get_height():
					img.set_pixel(px, py, col)

const BITMAP_FONT_5X7: Dictionary = {
	' ': [0x00, 0x00, 0x00, 0x00, 0x00],
	'!': [0x00, 0x00, 0x5F, 0x00, 0x00],
	'"': [0x00, 0x07, 0x00, 0x07, 0x00],
	'#': [0x14, 0x7F, 0x14, 0x7F, 0x14],
	'$': [0x24, 0x2A, 0x7F, 0x2A, 0x12],
	'%': [0x23, 0x13, 0x08, 0x64, 0x62],
	'&': [0x36, 0x49, 0x55, 0x22, 0x50],
	'\'': [0x00, 0x05, 0x03, 0x00, 0x00],
	'(': [0x00, 0x1C, 0x22, 0x41, 0x00],
	')': [0x00, 0x41, 0x22, 0x1C, 0x00],
	'*': [0x08, 0x2A, 0x1C, 0x2A, 0x08],
	'+': [0x08, 0x08, 0x3E, 0x08, 0x08],
	',': [0x00, 0x50, 0x30, 0x00, 0x00],
	'-': [0x08, 0x08, 0x08, 0x08, 0x08],
	'.': [0x00, 0x60, 0x60, 0x00, 0x00],
	'/': [0x20, 0x10, 0x08, 0x04, 0x02],
	'0': [0x3E, 0x51, 0x49, 0x45, 0x3E],
	'1': [0x00, 0x42, 0x7F, 0x40, 0x00],
	'2': [0x42, 0x61, 0x51, 0x49, 0x46],
	'3': [0x21, 0x41, 0x45, 0x4B, 0x31],
	'4': [0x18, 0x14, 0x12, 0x7F, 0x10],
	'5': [0x27, 0x45, 0x45, 0x45, 0x39],
	'6': [0x3C, 0x4A, 0x49, 0x49, 0x30],
	'7': [0x01, 0x71, 0x09, 0x05, 0x03],
	'8': [0x36, 0x49, 0x49, 0x49, 0x36],
	'9': [0x06, 0x49, 0x49, 0x29, 0x1E],
	':': [0x00, 0x36, 0x36, 0x00, 0x00],
	';': [0x00, 0x56, 0x36, 0x00, 0x00],
	'<': [0x08, 0x14, 0x22, 0x41, 0x00],
	'=': [0x14, 0x14, 0x14, 0x14, 0x14],
	'>': [0x00, 0x41, 0x22, 0x14, 0x08],
	'?': [0x02, 0x01, 0x51, 0x09, 0x06],
	'@': [0x32, 0x49, 0x79, 0x41, 0x3E],
	'A': [0x7E, 0x11, 0x11, 0x11, 0x7E],
	'B': [0x7F, 0x49, 0x49, 0x49, 0x36],
	'C': [0x3E, 0x41, 0x41, 0x41, 0x22],
	'D': [0x7F, 0x41, 0x41, 0x22, 0x1C],
	'E': [0x7F, 0x49, 0x49, 0x49, 0x41],
	'F': [0x7F, 0x09, 0x09, 0x09, 0x01],
	'G': [0x3E, 0x41, 0x49, 0x49, 0x7A],
	'H': [0x7F, 0x08, 0x08, 0x08, 0x7F],
	'I': [0x00, 0x41, 0x7F, 0x41, 0x00],
	'J': [0x20, 0x40, 0x41, 0x3F, 0x01],
	'K': [0x7F, 0x08, 0x14, 0x22, 0x41],
	'L': [0x7F, 0x40, 0x40, 0x40, 0x40],
	'M': [0x7F, 0x02, 0x0C, 0x02, 0x7F],
	'N': [0x7F, 0x04, 0x08, 0x10, 0x7F],
	'O': [0x3E, 0x41, 0x41, 0x41, 0x3E],
	'P': [0x7F, 0x09, 0x09, 0x09, 0x06],
	'Q': [0x3E, 0x41, 0x51, 0x21, 0x5E],
	'R': [0x7F, 0x09, 0x19, 0x29, 0x46],
	'S': [0x46, 0x49, 0x49, 0x49, 0x31],
	'T': [0x01, 0x01, 0x7F, 0x01, 0x01],
	'U': [0x3F, 0x40, 0x40, 0x40, 0x3F],
	'V': [0x1F, 0x20, 0x40, 0x20, 0x1F],
	'W': [0x7F, 0x20, 0x18, 0x20, 0x7F],
	'X': [0x63, 0x14, 0x08, 0x14, 0x63],
	'Y': [0x07, 0x08, 0x70, 0x08, 0x07],
	'Z': [0x61, 0x51, 0x49, 0x45, 0x43],
	'[': [0x00, 0x7F, 0x41, 0x41, 0x00],
	'\\': [0x02, 0x04, 0x08, 0x10, 0x20],
	']': [0x00, 0x41, 0x41, 0x7F, 0x00],
	'^': [0x04, 0x02, 0x01, 0x02, 0x04],
	'_': [0x40, 0x40, 0x40, 0x40, 0x40],
	'|': [0x00, 0x00, 0x7F, 0x00, 0x00],
	'~': [0x08, 0x04, 0x08, 0x10, 0x08]
}

func draw_bitmap_text(img: Image, x: int, y: int, text: String, col: Color, scale: int = 1):
	var cursor_x = x
	for i in range(text.length()):
		var ch = text[i].to_upper()
		var glyph = BITMAP_FONT_5X7.get(ch, BITMAP_FONT_5X7['?'])
		for col_idx in range(5):
			var col_byte = glyph[col_idx]
			for row_idx in range(7):
				if (col_byte & (1 << row_idx)) != 0:
					for sx in range(scale):
						for sy in range(scale):
							var px = cursor_x + col_idx * scale + sx
							var py = y + row_idx * scale + sy
							if px >= 0 and px < img.get_width() and py >= 0 and py < img.get_height():
								img.set_pixel(px, py, col)
		cursor_x += 6 * scale
