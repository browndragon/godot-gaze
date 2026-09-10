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
const CAM_GEOM_1920 := {
	"res": Vector2i(1920, 1080),
	"hfov_deg": 65.0
}

const TARGET_CENTER := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(0.0, 0.0),
	"eye": Vector2(0.0, 0.0),
}
const TARGET_LEFT_LEFT := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(-138.7, 0.0),
	"eye": Vector2(-138.7, 0.0),
}
const TARGET_RIGHT_RIGHT := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(138.7, 0.0),
	"eye": Vector2(138.7, 0.0),
}
const TARGET_TOP_TOP := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(0.0, -86.7),
	"eye": Vector2(0.0, -86.7),
}
const TARGET_DOWN_DOWN := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(0.0, 86.7),
	"eye": Vector2(0.0, 86.7),
}
const TARGET_NOSEDOWN_EYESUP := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(0.0, 86.7),
	"eye": Vector2(0.0, -86.7),
}
const TARGET_NOSELEFT_EYESRIGHT := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(-138.7, 0.0),
	"eye": Vector2(138.7, 0.0),
}
const TARGET_NOSERIGHT_EYESLEFT := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(138.7, 0.0),
	"eye": Vector2(-138.7, 0.0),
}
const TARGET_NOSETOP_EYESDOWN := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(0.0, -86.7),
	"eye": Vector2(0.0, 86.7),
}

const TARGET_LANDSCAPE_CENTER := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(0.0, 0.0),
	"eye": Vector2(0.0, 0.0),
}
const TARGET_LANDSCAPE_LEFT_LEFT := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(-86.7, 0.0),
	"eye": Vector2(-86.7, 0.0),
}
const TARGET_LANDSCAPE_RIGHT_RIGHT := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(86.7, 0.0),
	"eye": Vector2(86.7, 0.0),
}
const TARGET_LANDSCAPE_TOP_TOP := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(0.0, -138.7),
	"eye": Vector2(0.0, -138.7),
}
const TARGET_LANDSCAPE_DOWN_DOWN := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(0.0, 138.7),
	"eye": Vector2(0.0, 138.7),
}
const TARGET_LANDSCAPE_NOSEDOWN_EYESUP := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(0.0, 138.7),
	"eye": Vector2(0.0, -138.7),
}
const TARGET_LANDSCAPE_NOSELEFT_EYESRIGHT := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(-86.7, 0.0),
	"eye": Vector2(86.7, 0.0),
}
const TARGET_LANDSCAPE_NOSERIGHT_EYESLEFT := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(86.7, 0.0),
	"eye": Vector2(-86.7, 0.0),
}
const TARGET_LANDSCAPE_NOSETOP_EYESDOWN := {
	"relative_to": Vector2(0.0, 0.0),
	"nose": Vector2(0.0, -138.7),
	"eye": Vector2(0.0, 138.7),
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
	"self_roll_left.jpg": {"cam_geom": CAM_GEOM_1024, "roll_hint_deg": 25.0, "target": TARGET_CENTER, "is_eye_test": false},
	"self_roll_right.jpg": {"cam_geom": CAM_GEOM_1024, "roll_hint_deg": -25.0, "target": TARGET_CENTER, "is_eye_test": false},
	"self_yaw_left_roll_left.jpg": {"cam_geom": CAM_GEOM_1024, "roll_hint_deg": -25.0, "target": TARGET_LEFT_LEFT, "is_eye_test": false},
	"self_yaw_right_roll_left.jpg": {"cam_geom": CAM_GEOM_1024, "roll_hint_deg": -25.0, "target": TARGET_RIGHT_RIGHT, "is_eye_test": false},
	"eyes_both_open.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_CENTER, "is_eye_test": true},
	"eyes_both_wink.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_CENTER, "is_eye_test": true},
	"eyes_anatomical_left_wink.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_CENTER, "is_eye_test": true},
	"eyes_anatomical_right_wink.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_CENTER, "is_eye_test": true},
	"eyes_tilted_anatomical_right_wink.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_CENTER, "is_eye_test": true},
	"eyes_anatomical_left_obscured.jpg": {"cam_geom": CAM_GEOM_1440, "roll_hint_deg": 0.0, "target": TARGET_CENTER, "is_eye_test": true},
	"landscape_center.jpg": {"cam_geom": CAM_GEOM_1920, "sensor_orientation_deg": -90.0, "screen_pt": Vector2(982.0, 1512.0), "screen_mm": Vector2(188.5, 301.5), "camera_offset_mm": Vector3(94.25, 0.0, 0.0), "target": TARGET_LANDSCAPE_CENTER, "is_eye_test": false},
	"landscape_left_left.jpg": {"cam_geom": CAM_GEOM_1920, "sensor_orientation_deg": -90.0, "screen_pt": Vector2(982.0, 1512.0), "screen_mm": Vector2(188.5, 301.5), "camera_offset_mm": Vector3(94.25, 0.0, 0.0), "target": TARGET_LANDSCAPE_LEFT_LEFT, "is_eye_test": false},
	"landscape_right_right.jpg": {"cam_geom": CAM_GEOM_1920, "sensor_orientation_deg": -90.0, "screen_pt": Vector2(982.0, 1512.0), "screen_mm": Vector2(188.5, 301.5), "camera_offset_mm": Vector3(94.25, 0.0, 0.0), "target": TARGET_LANDSCAPE_RIGHT_RIGHT, "is_eye_test": false},
	"landscape_top_top.jpg": {"cam_geom": CAM_GEOM_1920, "sensor_orientation_deg": -90.0, "screen_pt": Vector2(982.0, 1512.0), "screen_mm": Vector2(188.5, 301.5), "camera_offset_mm": Vector3(94.25, 0.0, 0.0), "target": TARGET_LANDSCAPE_TOP_TOP, "is_eye_test": false},
	"landscape_down_down.jpg": {"cam_geom": CAM_GEOM_1920, "sensor_orientation_deg": -90.0, "screen_pt": Vector2(982.0, 1512.0), "screen_mm": Vector2(188.5, 301.5), "camera_offset_mm": Vector3(94.25, 0.0, 0.0), "target": TARGET_LANDSCAPE_DOWN_DOWN, "is_eye_test": false},
	"landscape_nosedown_eyesup.jpg": {"cam_geom": CAM_GEOM_1920, "sensor_orientation_deg": -90.0, "screen_pt": Vector2(982.0, 1512.0), "screen_mm": Vector2(188.5, 301.5), "camera_offset_mm": Vector3(94.25, 0.0, 0.0), "target": TARGET_LANDSCAPE_NOSEDOWN_EYESUP, "is_eye_test": false},
	"landscape_noseleft_eyesright.jpg": {"cam_geom": CAM_GEOM_1920, "sensor_orientation_deg": -90.0, "screen_pt": Vector2(982.0, 1512.0), "screen_mm": Vector2(188.5, 301.5), "camera_offset_mm": Vector3(94.25, 0.0, 0.0), "target": TARGET_LANDSCAPE_NOSELEFT_EYESRIGHT, "is_eye_test": false},
	"landscape_noseright_eyesleft.jpg": {"cam_geom": CAM_GEOM_1920, "sensor_orientation_deg": -90.0, "screen_pt": Vector2(982.0, 1512.0), "screen_mm": Vector2(188.5, 301.5), "camera_offset_mm": Vector3(94.25, 0.0, 0.0), "target": TARGET_LANDSCAPE_NOSERIGHT_EYESLEFT, "is_eye_test": false},
	"landscape_nosetop_eyesdown.jpg": {"cam_geom": CAM_GEOM_1920, "sensor_orientation_deg": -90.0, "screen_pt": Vector2(982.0, 1512.0), "screen_mm": Vector2(188.5, 301.5), "camera_offset_mm": Vector3(94.25, 0.0, 0.0), "target": TARGET_LANDSCAPE_NOSETOP_EYESDOWN, "is_eye_test": false},
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

func px_to_screen_mm(px: Vector2, scr_pt: Vector2, scr_mm: Vector2) -> Vector2:
	if px.x == INF or px.y == INF or px.x <= -9000.0 or px.y <= -9000.0:
		return Vector2(-9999.0, -9999.0)
	var x_mm = (px.x - scr_pt.x * 0.5) * (scr_mm.x / scr_pt.x)
	var y_mm = (px.y - scr_pt.y * 0.5) * (scr_mm.y / scr_pt.y)
	return Vector2(x_mm, y_mm)

func mm_to_screen_pt(pos_mm_from_center: Vector2, scr_pt: Vector2, scr_mm: Vector2) -> Vector2:
	var x_pt = (pos_mm_from_center.x * (scr_pt.x / scr_mm.x)) + (scr_pt.x * 0.5)
	var y_pt = (pos_mm_from_center.y * (scr_pt.y / scr_mm.y)) + (scr_pt.y * 0.5)
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
	var any_metric_improved = false
	var current_results_json: Dictionary = {}
	var html_table_rows: Array[Dictionary] = []
	var gallery_cards: Array[Dictionary] = []

	var total_fixtures_count = 0
	var passed_fixtures_count = 0
	var regressed_fixtures_count = 0
	var identical_count = 0
	var similar_count = 0
	var change_better_count = 0
	var change_worse_count = 0
	var no_face_count = 0

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
		var sensor_rot_deg = meta.get("sensor_orientation_deg", 0.0)

		# Sensor orientation compensation: rotate sideways camera captures
		if sensor_rot_deg == -90.0:
			img.rotate_90(COUNTERCLOCKWISE)
		elif sensor_rot_deg == 90.0:
			img.rotate_90(CLOCKWISE)
		elif sensor_rot_deg == 180.0:
			img.rotate_180()

		var cur_w = img.get_width()
		var cur_h = img.get_height()
		var frame_focal = float(cur_w) / (2.0 * tan(deg_to_rad(cam_geom.hfov_deg) * 0.5))
		vs.camera_set_resolution(cam_rid, cur_w, cur_h)
		vs.camera_set_focal_length(cam_rid, frame_focal)

		var cur_screen_pt = meta.get("screen_pt", Vector2(SCREEN_WIDTH_PT, SCREEN_HEIGHT_PT))
		var cur_screen_mm = meta.get("screen_mm", Vector2(SCREEN_WIDTH_MM, SCREEN_HEIGHT_MM))
		var cur_cam_offset_mm = meta.get("camera_offset_mm", Vector3(0.0, 0.0, 0.0))

		profile.set_logical_size_px(Vector2i(int(cur_screen_pt.x), int(cur_screen_pt.y)))
		profile.set_physical_size_mm(cur_screen_mm)
		profile.set_camera_offset_mm(cur_cam_offset_mm)
		profile.set_camera_roll_deg(0.0)
		gs.set_device_profile(profile)

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

		var target_spec = meta.get("target", TARGET_CENTER)
		var rel_offset = target_spec.get("relative_to", Vector2(0.0, 0.0))
		var nose_target = rel_offset + target_spec.get("nose", Vector2(0.0, 0.0))
		var gaze_target = rel_offset + target_spec.get("eye", Vector2(0.0, 0.0))
		var rot_target_deg = target_spec.get("head_rot_deg", null)

		var nose_proj_px = gs.project_ray_to_viewport(head_trans, head_fwd) if face_detected else Vector2(-9999.0, -9999.0)
		var gaze_proj_px = gs.get_projected_gaze(false) if face_detected else Vector2(-9999.0, -9999.0)
		var nose_proj_mm = px_to_screen_mm(nose_proj_px, cur_screen_pt, cur_screen_mm) if face_detected else Vector2(-9999.0, -9999.0)
		var gaze_proj_mm = gs.get_projected_gaze_mm(false) if face_detected else Vector2(-9999.0, -9999.0)

		var nose_diff = nose_proj_mm - nose_target
		var nose_err_mag = sqrt(nose_diff.x * nose_diff.x + nose_diff.y * nose_diff.y) if is_valid_pt(nose_proj_mm) else 9999.0

		var gaze_diff = gaze_proj_mm - gaze_target
		var gaze_err_mag = sqrt(gaze_diff.x * gaze_diff.x + gaze_diff.y * gaze_diff.y) if is_valid_pt(gaze_proj_mm) else 9999.0

		var head_err_deg = Vector3.ZERO
		var rot_err_mag = 0.0
		var rot_err_str = "N/A"
		if rot_target_deg != null and face_detected:
			head_err_deg = head_rot - rot_target_deg
			rot_err_mag = sqrt(head_err_deg.x * head_err_deg.x + head_err_deg.y * head_err_deg.y + head_err_deg.z * head_err_deg.z)
			rot_err_str = "%.1f deg" % rot_err_mag
		elif face_detected:
			var P_cam_target = Vector3(nose_target.x, -(nose_target.y + (cur_screen_mm.y * 0.5)), 0.0)
			var diff_vec = P_cam_target - head_trans
			if diff_vec.length() > 0.0:
				var pitch_exp = rad_to_deg(atan2(-diff_vec.y, sqrt(diff_vec.x * diff_vec.x + diff_vec.z * diff_vec.z)))
				var yaw_exp = rad_to_deg(atan2(diff_vec.x, -diff_vec.z))
				var yaw_diff = head_rot.y - yaw_exp
				while yaw_diff > 180.0: yaw_diff -= 360.0
				while yaw_diff < -180.0: yaw_diff += 360.0
				var pitch_diff = head_rot.x - pitch_exp
				rot_err_mag = sqrt(yaw_diff * yaw_diff + pitch_diff * pitch_diff)
				rot_err_str = "%.1f deg" % rot_err_mag

		var fixture_regressed = false
		var fixture_improved = false
		var fixture_similar = false
		var fixture_record: Dictionary = {}

		var props = [
			{"prop": "head_pos_mm", "val": format_vec3(head_trans) if face_detected else "NO_FACE", "err_str": "N/A", "err_mag": 0.0},
			{"prop": "head_rot_deg", "val": format_vec3(head_rot) if face_detected else "NO_FACE", "err_str": rot_err_str, "err_mag": rot_err_mag},
			{"prop": "nose_mm", "val": format_vec2(nose_proj_mm) if face_detected else "NO_FACE", "err_str": ("%.1f mm" % nose_err_mag) if is_valid_pt(nose_proj_mm) else "N/A", "err_mag": nose_err_mag if is_valid_pt(nose_proj_mm) else 0.0},
			{"prop": "gaze_mm", "val": format_vec2(gaze_proj_mm) if face_detected else "NO_FACE", "err_str": ("%.1f mm" % gaze_err_mag) if is_valid_pt(gaze_proj_mm) else "N/A", "err_mag": gaze_err_mag if is_valid_pt(gaze_proj_mm) else 0.0}
		]

		var golden_fixture = golden_data.get(img_file, {})

		var card_metrics: Array[Dictionary] = []
		for p in props:
			var prop_key = p["prop"]
			fixture_record[prop_key] = p["val"]

			var prev_val_str = "N/A"
			var prev_err_str = "N/A"
			var delta_str = "0.0 mm"
			var status_str = "IDENTICAL"

			if golden_fixture.has(prop_key):
				var prev_entry = golden_fixture[prop_key]
				if prev_entry is Dictionary:
					prev_val_str = prev_entry.get("val", "N/A")
				elif prev_entry is String:
					prev_val_str = prev_entry
				else:
					prev_val_str = str(prev_entry)

				var prev_mag = 0.0
				var has_prev_err = false

				if prev_val_str != "N/A" and prev_val_str != "NO_FACE":
					if prop_key == "nose_mm":
						var v = parse_vector(prev_val_str)
						var pt = Vector2(v.x, v.y)
						if is_valid_pt(pt):
							var diff = pt - nose_target
							prev_mag = diff.length()
							prev_err_str = "%.1f mm" % prev_mag
							has_prev_err = true
					elif prop_key == "gaze_mm":
						var v = parse_vector(prev_val_str)
						var pt = Vector2(v.x, v.y)
						if is_valid_pt(pt):
							var diff = pt - gaze_target
							prev_mag = diff.length()
							prev_err_str = "%.1f mm" % prev_mag
							has_prev_err = true
					elif prop_key == "head_rot_deg":
						var prev_rot = parse_vector(prev_val_str)
						if rot_target_deg != null:
							var diff = prev_rot - rot_target_deg
							prev_mag = sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z)
							prev_err_str = "%.1f deg" % prev_mag
							has_prev_err = true
						else:
							var prev_pos_entry = golden_fixture.get("head_pos_mm", null)
							var prev_pos_str = ""
							if prev_pos_entry is Dictionary:
								prev_pos_str = prev_pos_entry.get("val", "")
							elif prev_pos_entry is String:
								prev_pos_str = prev_pos_entry
							if prev_pos_str != "" and prev_pos_str != "NO_FACE":
								var prev_pos = parse_vector(prev_pos_str)
								var P_cam = Vector3(nose_target.x, -(nose_target.y + (cur_screen_mm.y * 0.5)), 0.0)
								var diff_vec = P_cam - prev_pos
								if diff_vec.length() > 0.0:
									var pitch_exp = rad_to_deg(atan2(-diff_vec.y, sqrt(diff_vec.x * diff_vec.x + diff_vec.z * diff_vec.z)))
									var yaw_exp = rad_to_deg(atan2(diff_vec.x, -diff_vec.z))
									var yaw_diff = prev_rot.y - yaw_exp
									while yaw_diff > 180.0: yaw_diff -= 360.0
									while yaw_diff < -180.0: yaw_diff += 360.0
									var pitch_diff = prev_rot.x - pitch_exp
									prev_mag = sqrt(yaw_diff * yaw_diff + pitch_diff * pitch_diff)
									prev_err_str = "%.1f deg" % prev_mag
									has_prev_err = true

				if has_prev_err and p["err_mag"] > 0.0 and not meta.get("is_eye_test", false):
					var delta = p["err_mag"] - prev_mag
					var unit = "deg" if prop_key == "head_rot_deg" else "mm"
					if delta > 15.0:
						delta_str = "+%.1f %s" % [delta, unit]
						status_str = "CHANGE_WORSE"
						any_metric_regressed = true
						fixture_regressed = true
						printerr("CHANGE_WORSE (REGRESSION): %s %s error regressed from %s to %s (delta: %s)" % [img_file, prop_key, prev_err_str, p["err_str"], delta_str])
					elif delta < -15.0:
						delta_str = "%.1f %s" % [delta, unit]
						status_str = "CHANGE_BETTER"
						any_metric_improved = true
						fixture_improved = true
						print("CHANGE_BETTER (IMPROVEMENT): %s %s error improved from %s to %s (delta: %s)" % [img_file, prop_key, prev_err_str, p["err_str"], delta_str])
					elif abs(delta) > 0.1:
						delta_str = ("+%.1f %s" % [delta, unit]) if delta > 0 else ("%.1f %s" % [delta, unit])
						status_str = "SIMILAR"
						fixture_similar = true
					else:
						delta_str = "0.0 %s" % unit
						status_str = "IDENTICAL"
				elif prev_val_str == p["val"]:
					status_str = "IDENTICAL"
				else:
					status_str = "SIMILAR"
					fixture_similar = true
			else:
				status_str = "SIMILAR"
				fixture_similar = true

			card_metrics.append({
				"name": prop_key,
				"val": p["val"],
				"err": p["err_str"],
				"prev_val": prev_val_str,
				"prev_err": prev_err_str,
				"delta": delta_str,
				"status": status_str
			})

			report_lines.append("| %s | %s | %s | %s | %s | %s | %s |" % [
				img_file, prop_key, p["val"], p["err_str"], prev_err_str, delta_str, status_str
			])

		current_results_json[img_file] = fixture_record

		var card_status = "IDENTICAL"
		var chip_class = "chip-same"
		var chip_label = "IDENTICAL"
		if not face_detected:
			card_status = "NO_FACE"
			chip_class = "chip-noface"
			chip_label = "NO_FACE"
			no_face_count += 1
		elif fixture_regressed:
			card_status = "CHANGE_WORSE"
			chip_class = "chip-worse"
			chip_label = "CHANGE_WORSE"
			change_worse_count += 1
		elif fixture_improved:
			card_status = "CHANGE_BETTER"
			chip_class = "chip-better"
			chip_label = "CHANGE_BETTER"
			change_better_count += 1
		elif fixture_similar:
			card_status = "SIMILAR"
			chip_class = "chip-diff"
			chip_label = "SIMILAR"
			similar_count += 1
		else:
			card_status = "IDENTICAL"
			chip_class = "chip-same"
			chip_label = "IDENTICAL"
			identical_count += 1

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

		# Extract Previous Golden Gaze & Nose points
		var prev_nose_entry = golden_fixture.get("nose_mm", "")
		var prev_gaze_entry = golden_fixture.get("gaze_mm", "")
		var prev_nose_mm_str = prev_nose_entry.get("val", "") if prev_nose_entry is Dictionary else str(prev_nose_entry)
		var prev_gaze_mm_str = prev_gaze_entry.get("val", "") if prev_gaze_entry is Dictionary else str(prev_gaze_entry)
		var prev_nose_vec = parse_vector(prev_nose_mm_str)
		var prev_gaze_vec = parse_vector(prev_gaze_mm_str)
		var prev_nose_pt = mm_to_screen_pt(Vector2(prev_nose_vec.x, prev_nose_vec.y), cur_screen_pt, cur_screen_mm) if prev_nose_mm_str != "" and prev_nose_mm_str != "NO_FACE" else Vector2(-9999.0, -9999.0)
		var prev_gaze_pt = mm_to_screen_pt(Vector2(prev_gaze_vec.x, prev_gaze_vec.y), cur_screen_pt, cur_screen_mm) if prev_gaze_mm_str != "" and prev_gaze_mm_str != "NO_FACE" else Vector2(-9999.0, -9999.0)

		var target_nose_pt = mm_to_screen_pt(nose_target, cur_screen_pt, cur_screen_mm)
		var target_eye_pt = mm_to_screen_pt(gaze_target, cur_screen_pt, cur_screen_mm)

		# Render Diagnostic Overlays
		var diag_overlay_rel_path = "benchmark_overlays/" + base_fn + "_diagnostic.png"
		var left_panel = render_camera_panel(img, head_trans, head_xform, eye_orig, gaze_dir, lm_pts, face_detected, meta.get("roll_hint_deg", 0.0), frame_focal, left_crop, right_crop)
		var right_panel = render_display_panel(
			img, img_file, cur_screen_pt, cur_screen_mm, cur_cam_offset_mm,
			nose_target, gaze_target,
			target_nose_pt, target_eye_pt,
			nose_proj_px, gaze_proj_px,
			prev_nose_pt, prev_gaze_pt,
			nose_err_mag if is_valid_pt(nose_proj_mm) else 0.0,
			gaze_err_mag if is_valid_pt(gaze_proj_mm) else 0.0
		)
		var composite = create_side_by_side_composite(left_panel, right_panel, img_file)
		composite.save_png(overlays_dir + "/" + base_fn + "_diagnostic.png")

		gallery_cards.append({
			"file": img_file,
			"anchor_id": img_file.replace(".", "_"),
			"status": card_status,
			"chip_class": chip_class,
			"chip_label": chip_label,
			"roll_hint": meta.get("roll_hint_deg", 0.0),
			"sensor_orient": meta.get("sensor_orientation_deg", 0.0),
			"left_open": left_open,
			"right_open": right_open,
			"metrics": card_metrics,
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

	var has_significant_change = any_metric_regressed or any_metric_improved

	# Generate HTML Dashboard Report
	var html_report_path = global_artifacts_dir + "/benchmark_report.html"
	generate_html_report(
		html_report_path, gallery_cards, total_fixtures_count,
		identical_count, similar_count, change_better_count, change_worse_count, no_face_count,
		has_significant_change
	)

	print("\n" + full_report)
	print("Benchmark candidate JSON written to: ", current_json_path)
	print("Benchmark HTML report written to:     ", html_report_path)

	gs.stop_tracking(true)
	vs.camera_stop(cam_rid)
	vs.camera_free(cam_rid)

	# Assert that HTML report and overlays were successfully generated
	assert(FileAccess.file_exists(html_report_path), "HTML report was not generated!")

	if has_significant_change:
		printerr("\n==========================================================")
		printerr("GAZE BENCHMARK: SIGNIFICANT METRIC CHANGES DETECTED!")
		printerr("CHANGE_WORSE: %d | CHANGE_BETTER: %d | SIMILAR: %d | IDENTICAL: %d" % [change_worse_count, change_better_count, similar_count, identical_count])
		printerr("Visual Report:    ", html_report_path)
		printerr("Candidate Golden: ", current_json_path)
		printerr("")
		printerr("NOTE: Golden baselines must NOT be updated without human review.")
		printerr("If changes are verified and approved by project leadership:")
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
	img: Image, _img_name: String,
	screen_pt_size: Vector2, _screen_mm_size: Vector2, cam_offset_mm: Vector3,
	_target_nose_mm: Vector2, _target_eye_mm: Vector2,
	target_nose_pt: Vector2, target_eye_pt: Vector2,
	nose_rep: Vector2, eye_rep: Vector2,
	prev_nose_pt: Vector2, prev_gaze_pt: Vector2,
	nose_err_mm: float, gaze_err_mm: float
) -> Image:
	var disp_w = screen_pt_size.x
	var disp_h = screen_pt_size.y
	var disp_center_pt = Vector2(disp_w * 0.5, disp_h * 0.5)

	# Physical Camera Notch location on display perimeter
	var cam_notch_pt = Vector2(disp_w * 0.5, 0.0) # Top bezel default
	if cam_offset_mm.x > 30.0:
		cam_notch_pt = Vector2(disp_w, disp_h * 0.5) # Right bezel
	elif cam_offset_mm.x < -30.0:
		cam_notch_pt = Vector2(0.0, disp_h * 0.5) # Left bezel

	# Window inside display
	var win_w = disp_w * 0.5
	var win_h = disp_h * 0.5
	var win_pos = Vector2(disp_w * 0.25, disp_h * 0.25)
	if disp_w < disp_h: # Portrait orientation
		win_w = disp_w * 0.75
		win_h = disp_h * 0.38
		win_pos = Vector2(disp_w * 0.125, disp_h * 0.31)

	# Dynamic world bounding box
	var min_x = 0.0
	var max_x = disp_w
	var min_y = 0.0
	var max_y = disp_h

	var pts_to_check = [cam_notch_pt, disp_center_pt, target_eye_pt, target_nose_pt, eye_rep, nose_rep, prev_gaze_pt, prev_nose_pt]
	for p in pts_to_check:
		if is_valid_pt(p):
			min_x = min(min_x, p.x)
			max_x = max(max_x, p.x)
			min_y = min(min_y, p.y)
			max_y = max(max_y, p.y)

	var pad_x = max(260.0, (max_x - min_x) * 0.25)
	var pad_y = max(220.0, (max_y - min_y) * 0.25)
	min_x -= pad_x
	max_x += pad_x
	min_y -= pad_y
	max_y += pad_y

	var world_w = max_x - min_x
	var world_h = max_y - min_y

	var canvas_w = 1600
	var canvas_h = 1200
	var canvas = Image.create(canvas_w, canvas_h, false, Image.FORMAT_RGBA8)
	canvas.fill(Color(0.07, 0.07, 0.09, 1.0))

	var scale = min(float(canvas_w) / world_w, float(canvas_h) / world_h) * 0.94
	var offset_x = (canvas_w - world_w * scale) * 0.5 - min_x * scale
	var offset_y = (canvas_h - world_h * scale) * 0.5 - min_y * scale

	var to_canvas = func(p: Vector2) -> Vector2i:
		return Vector2i(int(p.x * scale + offset_x), int(p.y * scale + offset_y))

	# 1. Screen Bezel & Display Area
	var s0 = to_canvas.call(Vector2(0, 0))
	var s1 = to_canvas.call(Vector2(disp_w, disp_h))
	var sw = s1.x - s0.x
	var sh = s1.y - s0.y

	# Outer device bezel
	draw_filled_rect(canvas, s0.x - 12, s0.y - 12, sw + 24, sh + 24, Color(0.12, 0.12, 0.15, 1.0))
	draw_rect(canvas, s0.x - 12, s0.y - 12, sw + 24, sh + 24, Color(0.35, 0.35, 0.40, 1.0), 2)

	# Inner display glass
	draw_filled_rect(canvas, s0.x, s0.y, sw, sh, Color(0.14, 0.14, 0.18, 1.0))
	draw_rect(canvas, s0.x, s0.y, sw, sh, Color(0.45, 0.45, 0.55, 1.0), 2)

	# 2. Camera Notch
	var cn_c = to_canvas.call(cam_notch_pt)
	if cam_offset_mm.x > 30.0: # Right bezel
		draw_filled_rect(canvas, s1.x, cn_c.y - 20, 10, 40, Color(0.04, 0.04, 0.06, 1.0))
		draw_circle(canvas, s1.x + 5, cn_c.y, 4, Color(0.1, 0.85, 1.0, 1.0))
	elif cam_offset_mm.x < -30.0: # Left bezel
		draw_filled_rect(canvas, s0.x - 10, cn_c.y - 20, 10, 40, Color(0.04, 0.04, 0.06, 1.0))
		draw_circle(canvas, s0.x - 5, cn_c.y, 4, Color(0.1, 0.85, 1.0, 1.0))
	else: # Top bezel
		draw_filled_rect(canvas, cn_c.x - 20, s0.y - 10, 40, 10, Color(0.04, 0.04, 0.06, 1.0))
		draw_circle(canvas, cn_c.x, s0.y - 5, 4, Color(0.1, 0.85, 1.0, 1.0))

	# 3. Game Window & Mirrored Feed
	var w0 = to_canvas.call(win_pos)
	var w1 = to_canvas.call(win_pos + Vector2(win_w, win_h))
	var ww = w1.x - w0.x
	var wh = w1.y - w0.y
	var titlebar_h = 24

	draw_filled_rect(canvas, w0.x, w0.y, ww, wh, Color(0.09, 0.09, 0.11, 1.0))
	draw_rect(canvas, w0.x, w0.y, ww, wh, Color(0.35, 0.55, 0.85, 1.0), 2)
	draw_filled_rect(canvas, w0.x, w0.y, ww, titlebar_h, Color(0.18, 0.18, 0.22, 1.0))

	draw_circle(canvas, w0.x + 12, w0.y + 12, 4, Color(1.0, 0.38, 0.35, 1.0))
	draw_circle(canvas, w0.x + 24, w0.y + 12, 4, Color(1.0, 0.75, 0.25, 1.0))
	draw_circle(canvas, w0.x + 36, w0.y + 12, 4, Color(0.30, 0.85, 0.40, 1.0))

	var iw = ww - 4
	var ih = wh - titlebar_h - 3
	var ix = w0.x + 2
	var iy = w0.y + titlebar_h + 1
	draw_filled_rect(canvas, ix, iy, iw, ih, Color(0.05, 0.05, 0.07, 1.0))

	var aspect_src = float(img.get_width()) / float(img.get_height())
	var aspect_dst = float(iw) / float(ih)
	var fit_w = iw
	var fit_h = ih
	if aspect_src > aspect_dst:
		fit_h = int(float(iw) / aspect_src)
	else:
		fit_w = int(float(ih) * aspect_src)

	var fit_x = ix + (iw - fit_w) / 2
	var fit_y = iy + (ih - fit_h) / 2

	var mirrored_img = img.duplicate()
	mirrored_img.convert(Image.FORMAT_RGBA8)
	mirrored_img.flip_x()
	mirrored_img.resize(fit_w, fit_h, Image.INTERPOLATE_BILINEAR)
	canvas.blit_rect(mirrored_img, Rect2i(0, 0, fit_w, fit_h), Vector2i(fit_x, fit_y))
	draw_rect(canvas, fit_x, fit_y, fit_w, fit_h, Color(0.2, 0.5, 0.8, 0.4), 1)

	# 4. Screen Center Marker
	var sc_c = to_canvas.call(disp_center_pt)
	draw_crosshair(canvas, sc_c.x, sc_c.y, 8, Color(0.7, 0.7, 0.8, 0.8))
	draw_circle(canvas, sc_c.x, sc_c.y, 3, Color(0.9, 0.9, 1.0, 0.9))

	# 5. Target Markers: White for Nose Target, Faint for Eye Target
	var te_c = to_canvas.call(target_eye_pt) if is_valid_pt(target_eye_pt) else Vector2i(-9999, -9999)
	var tn_c = to_canvas.call(target_nose_pt) if is_valid_pt(target_nose_pt) else Vector2i(-9999, -9999)

	# Nose Target: Distinct White Crosshair & Ring
	if is_valid_pt(target_nose_pt):
		draw_circle(canvas, tn_c.x, tn_c.y, 7, Color(1.0, 1.0, 1.0, 0.9))
		draw_crosshair(canvas, tn_c.x, tn_c.y, 14, Color.WHITE)
		draw_bitmap_text(canvas, tn_c.x + 10, tn_c.y - 12, "NOSE TARGET", Color(1.0, 1.0, 1.0, 0.9), 1)

	# Eye Target: Faint Mark
	if is_valid_pt(target_eye_pt):
		draw_circle(canvas, te_c.x, te_c.y, 5, Color(1.0, 1.0, 1.0, 0.35))
		draw_crosshair(canvas, te_c.x, te_c.y, 10, Color(1.0, 1.0, 1.0, 0.45))
		if (target_eye_pt - target_nose_pt).length() > 20.0:
			draw_bitmap_text(canvas, te_c.x + 10, te_c.y + 6, "EYE TARGET", Color(0.8, 0.8, 0.8, 0.5), 1)

	# 6. Previous Golden Baselines (Desaturated Lines & Smaller Markers)
	# Prev Eye Gaze: Center -> Prev Gaze Pt -> Target Eye Pt (Desaturated Cyan)
	if is_valid_pt(prev_gaze_pt):
		var pge_c = to_canvas.call(prev_gaze_pt)
		var desat_cyan = Color(0.25, 0.55, 0.60, 0.55)
		draw_thick_line(canvas, sc_c.x, sc_c.y, pge_c.x, pge_c.y, desat_cyan, 2)
		if is_valid_pt(target_eye_pt):
			draw_thick_line(canvas, pge_c.x, pge_c.y, te_c.x, te_c.y, desat_cyan, 2)
		draw_circle(canvas, pge_c.x, pge_c.y, 4, desat_cyan)
		draw_bitmap_text(canvas, pge_c.x + 8, pge_c.y - 12, "PREV GAZE", desat_cyan, 1)

	# Prev Nose: Center -> Prev Nose Pt -> Target Nose Pt (Desaturated Green)
	if is_valid_pt(prev_nose_pt):
		var pnr_c = to_canvas.call(prev_nose_pt)
		var desat_green = Color(0.30, 0.60, 0.35, 0.55)
		draw_thick_line(canvas, sc_c.x, sc_c.y, pnr_c.x, pnr_c.y, desat_green, 2)
		if is_valid_pt(target_nose_pt):
			draw_thick_line(canvas, pnr_c.x, pnr_c.y, tn_c.x, tn_c.y, desat_green, 2)
		draw_circle(canvas, pnr_c.x, pnr_c.y, 4, desat_green)
		draw_bitmap_text(canvas, pnr_c.x + 8, pnr_c.y + 4, "PREV NOSE", desat_green, 1)

	# 7. Current Eye Gaze: Center -> Current Gaze Pt -> Target Eye Pt (Cyan)
	if is_valid_pt(eye_rep):
		var ge_c = to_canvas.call(eye_rep)
		var cyan_col = Color(0.0, 0.95, 1.0, 0.90)
		draw_thick_line(canvas, sc_c.x, sc_c.y, ge_c.x, ge_c.y, cyan_col, 3)
		if is_valid_pt(target_eye_pt):
			draw_thick_line(canvas, ge_c.x, ge_c.y, te_c.x, te_c.y, cyan_col, 2)
		draw_circle(canvas, ge_c.x, ge_c.y, 8, cyan_col)
		draw_crosshair(canvas, ge_c.x, ge_c.y, 14, Color.WHITE)
		draw_bitmap_text(canvas, ge_c.x + 12, ge_c.y - 14, "EYE GAZE (%.1fmm)" % gaze_err_mm, cyan_col, 1)

	# 8. Current Nose Gaze: Center -> Current Nose Pt -> Target Nose Pt (Green)
	if is_valid_pt(nose_rep):
		var nr_c = to_canvas.call(nose_rep)
		var green_col = Color(0.15, 1.0, 0.30, 0.95)
		draw_thick_line(canvas, sc_c.x, sc_c.y, nr_c.x, nr_c.y, green_col, 3)
		if is_valid_pt(target_nose_pt):
			draw_thick_line(canvas, nr_c.x, nr_c.y, tn_c.x, tn_c.y, green_col, 2)
		draw_circle(canvas, nr_c.x, nr_c.y, 7, green_col)
		draw_crosshair(canvas, nr_c.x, nr_c.y, 12, Color.WHITE)
		draw_bitmap_text(canvas, nr_c.x + 10, nr_c.y + 6, "NOSE GAZE (%.1fmm)" % nose_err_mm, green_col, 1)

	# 9. Header Bar & Legend Card
	draw_filled_rect(canvas, 0, 0, canvas_w, 36, Color(0.05, 0.05, 0.08, 0.90))
	draw_bitmap_text(canvas, 14, 10, "DISPLAY PROJECTION MOCKUP (CENTER-ORIGIN PATHS)", Color.WHITE, 2)
	draw_rect(canvas, 0, 0, canvas_w, canvas_h, Color(0.3, 0.3, 0.35, 1.0), 2)

	var leg_w = 480
	var leg_h = 130
	var leg_x = 16
	var leg_y = canvas_h - leg_h - 16
	draw_filled_rect(canvas, leg_x, leg_y, leg_w, leg_h, Color(0.05, 0.05, 0.08, 0.92))
	draw_rect(canvas, leg_x, leg_y, leg_w, leg_h, Color(0.3, 0.4, 0.5, 1.0), 1)
	draw_bitmap_text(canvas, leg_x + 12, leg_y + 12, "CYAN:       CURRENT EYE GAZE (CENTER -> GAZE -> TARGET)", Color(0.0, 0.95, 1.0, 1.0), 1)
	draw_bitmap_text(canvas, leg_x + 12, leg_y + 32, "GREEN:      CURRENT NOSE GAZE (CENTER -> NOSE -> TARGET)", Color(0.2, 1.0, 0.3, 1.0), 1)
	draw_bitmap_text(canvas, leg_x + 12, leg_y + 52, "DESAT CYAN: PREVIOUS GOLDEN EYE GAZE", Color(0.35, 0.65, 0.70, 1.0), 1)
	draw_bitmap_text(canvas, leg_x + 12, leg_y + 72, "DESAT GRN:  PREVIOUS GOLDEN NOSE GAZE", Color(0.4, 0.7, 0.45, 1.0), 1)
	draw_bitmap_text(canvas, leg_x + 12, leg_y + 92, "WHITE MARK: NOSE TARGET CUE", Color.WHITE, 1)
	draw_bitmap_text(canvas, leg_x + 12, leg_y + 110, "FAINT MARK: EYE TARGET CUE", Color(0.7, 0.7, 0.7, 1.0), 1)

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
	out_path: String, cards: Array[Dictionary],
	total_cnt: int,
	identical_cnt: int, similar_cnt: int, change_better_cnt: int, change_worse_cnt: int, no_face_cnt: int,
	has_significant_change: bool
):
	var status_class = "status-failed" if has_significant_change else "status-passed"
	var status_text = "CHANGES DETECTED (TRIAGE REQUIRED)" if has_significant_change else "PASSED (ALL IDENTICAL / SIMILAR)"

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
    --grey: #8b949e;
  }
  html { scroll-behavior: smooth; }
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
    margin-bottom: 20px;
    display: flex;
    justify-content: space-between;
    align-items: center;
  }
  .header-title h1 { margin: 0 0 8px 0; color: var(--text-bright); font-size: 24px; }
  .header-title p { margin: 0; color: var(--grey); font-size: 14px; }
  .badge {
    padding: 6px 14px;
    border-radius: 20px;
    font-weight: 600;
    font-size: 14px;
    display: inline-block;
  }
  .status-passed { background: rgba(46, 160, 67, 0.2); color: var(--green); border: 1px solid var(--green); }
  .status-failed { background: rgba(248, 81, 73, 0.2); color: var(--red); border: 1px solid var(--red); }
  
  .badge-same { background: rgba(46, 160, 67, 0.15); color: var(--green); border: 1px solid rgba(46, 160, 67, 0.4); }
  .badge-diff { background: rgba(139, 148, 158, 0.15); color: var(--grey); border: 1px solid rgba(139, 148, 158, 0.3); }
  .badge-better { background: rgba(88, 166, 255, 0.18); color: var(--blue); border: 1px solid rgba(88, 166, 255, 0.5); }
  .badge-worse { background: rgba(248, 81, 73, 0.2); color: var(--red); border: 1px solid rgba(248, 81, 73, 0.5); }
  .badge-noface { background: rgba(110, 118, 129, 0.1); color: #6e7681; border: 1px solid #30363d; }
  
  .kpi-row {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
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
  .kpi-lbl { font-size: 12px; text-transform: uppercase; color: var(--grey); letter-spacing: 0.5px; }
  
  /* Quick Navigation Grid */
  .nav-grid-container {
    background: var(--card-bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 20px;
    margin-bottom: 32px;
  }
  .nav-grid-title {
    font-size: 15px;
    font-weight: 600;
    color: var(--text-bright);
    margin-bottom: 14px;
    display: flex;
    justify-content: space-between;
    align-items: center;
  }
  .nav-legend {
    font-size: 12px;
    display: flex;
    gap: 16px;
    flex-wrap: wrap;
  }
  .nav-legend-item { display: flex; align-items: center; gap: 6px; }
  .dot { width: 8px; height: 8px; border-radius: 50%; }
  .dot-same { background: var(--green); }
  .dot-diff { background: var(--grey); }
  .dot-better { background: var(--blue); }
  .dot-worse { background: var(--red); }
  .dot-noface { background: #484f58; }

  .nav-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(220px, 1fr));
    gap: 10px;
  }
  .nav-chip {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 8px 12px;
    border-radius: 6px;
    font-size: 12px;
    text-decoration: none;
    transition: transform 0.15s ease, border-color 0.15s ease;
  }
  .nav-chip:hover {
    transform: translateY(-2px);
    text-decoration: none;
  }
  .chip-same {
    background: rgba(46, 160, 67, 0.10);
    border: 1px solid rgba(46, 160, 67, 0.4);
    color: #3fb950;
  }
  .chip-diff {
    background: rgba(139, 148, 158, 0.10);
    border: 1px solid rgba(139, 148, 158, 0.35);
    color: #8b949e;
  }
  .chip-better {
    background: rgba(88, 166, 255, 0.12);
    border: 1px solid rgba(88, 166, 255, 0.45);
    color: #79c0ff;
  }
  .chip-worse {
    background: rgba(248, 81, 73, 0.15);
    border: 1px solid rgba(248, 81, 73, 0.6);
    color: #ff7b72;
  }
  .chip-noface {
    background: rgba(110, 118, 129, 0.08);
    border: 1px solid #30363d;
    color: #6e7681;
  }
  .chip-fn { font-family: monospace; font-size: 12px; font-weight: 500; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; max-width: 130px; }
  .chip-tag { font-size: 10px; font-weight: 700; padding: 2px 6px; border-radius: 10px; }
  .chip-same .chip-tag { background: rgba(46, 160, 67, 0.25); color: #3fb950; }
  .chip-diff .chip-tag { background: rgba(139, 148, 158, 0.25); color: #8b949e; }
  .chip-better .chip-tag { background: rgba(88, 166, 255, 0.3); color: #79c0ff; }
  .chip-worse .chip-tag { background: rgba(248, 81, 73, 0.3); color: #ff7b72; }
  .chip-noface .chip-tag { background: rgba(110, 118, 129, 0.2); color: #6e7681; }

  /* Gallery Fixture Cards */
  .gallery-grid {
    display: grid;
    grid-template-columns: 1fr;
    gap: 28px;
  }
  .fixture-card {
    background: var(--card-bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 24px;
    scroll-margin-top: 24px;
  }
  .fixture-card.card-worse { border-color: var(--red); }
  .fixture-card.card-better { border-color: var(--blue); }
  .fixture-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 18px;
    padding-bottom: 12px;
    border-bottom: 1px solid var(--border);
  }
  .fixture-title { font-size: 20px; font-weight: 600; color: var(--text-bright); font-family: monospace; }
  .fixture-meta { font-size: 13px; color: var(--grey); margin-left: 10px; font-weight: normal; font-family: -apple-system, BlinkMacSystemFont, sans-serif; }

  /* Embedded Metrics Table */
  .card-table {
    width: 100%;
    border-collapse: collapse;
    background: #0d1117;
    border: 1px solid var(--border);
    border-radius: 6px;
    overflow: hidden;
    margin-bottom: 20px;
    font-size: 13px;
  }
  .card-table th, .card-table td {
    padding: 8px 12px;
    text-align: left;
    border-bottom: 1px solid #21262d;
  }
  .card-table th { background: #161b22; color: var(--text-bright); font-size: 12px; text-transform: uppercase; letter-spacing: 0.5px; }
  .card-table tr:last-child td { border-bottom: none; }
  .card-table code { font-family: monospace; color: #79c0ff; }
  .delta-ok { color: var(--green); font-weight: 600; }
  .delta-reg { color: var(--red); font-weight: 700; }
  .delta-imp { color: var(--blue); font-weight: 600; }
  .delta-diff { color: var(--grey); }

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
  .eye-lbl { font-size: 11px; color: var(--grey); font-weight: 600; }
  .eye-val { font-size: 12px; color: var(--cyan); font-weight: bold; }
  .overlay-col img {
    width: 100%;
    border-radius: 6px;
    border: 1px solid var(--border);
    display: block;
    transition: opacity 0.15s ease;
  }
  .overlay-col a:hover img { opacity: 0.95; }
  a { color: var(--blue); text-decoration: none; }
  a:hover { text-decoration: underline; }
</style>
</head>
<body>
<div class="container">
  <div class="header-card">
    <div class="header-title">
      <h1>Godot-Gaze Visual Benchmark Dashboard</h1>
      <p>Generated on """ + Time.get_datetime_string_from_system(true) + """Z &bull; Complete Fixture Suite</p>
    </div>
    <div class="badge """ + status_class + """">""" + status_text + """</div>
  </div>

  <div class="kpi-row">
    <div class="kpi-card"><div class="kpi-lbl">Total Fixtures</div><div class="kpi-val">""" + str(total_cnt) + """</div></div>
    <div class="kpi-card"><div class="kpi-lbl">Identical</div><div class="kpi-val" style="color: var(--green);">""" + str(identical_cnt) + """</div></div>
    <div class="kpi-card"><div class="kpi-lbl">Similar (&le;15mm)</div><div class="kpi-val" style="color: var(--grey);">""" + str(similar_cnt) + """</div></div>
    <div class="kpi-card"><div class="kpi-lbl">Change Better</div><div class="kpi-val" style="color: var(--blue);">""" + str(change_better_cnt) + """</div></div>
    <div class="kpi-card"><div class="kpi-lbl">Change Worse</div><div class="kpi-val" style="color: var(--red);">""" + str(change_worse_cnt) + """</div></div>
  </div>

  <!-- Navigation Grid (Anchors to Cards Below) -->
  <div class="nav-grid-container">
    <div class="nav-grid-title">
      <span>Fixture Index & Quick Navigation</span>
      <div class="nav-legend">
        <span class="nav-legend-item"><span class="dot dot-same"></span> Identical</span>
        <span class="nav-legend-item"><span class="dot dot-diff"></span> Similar (&le; 15mm)</span>
        <span class="nav-legend-item"><span class="dot dot-better"></span> Change Better (&lt; -15mm)</span>
        <span class="nav-legend-item"><span class="dot dot-worse"></span> Change Worse (&gt; +15mm)</span>
        <span class="nav-legend-item"><span class="dot dot-noface"></span> No Face</span>
      </div>
    </div>
    <div class="nav-grid">
"""

	for c in cards:
		html += """      <a href='#fixture-""" + c["anchor_id"] + """' class='nav-chip """ + c["chip_class"] + """'>
        <span class='chip-fn'>""" + c["file"] + """</span>
        <span class='chip-tag'>""" + c["chip_label"] + """</span>
      </a>\n"""

	html += """    </div>
  </div>

  <!-- Diagnostic Fixture Gallery with Merged Metrics Tables -->
  <div class="gallery-grid">
"""

	for c in cards:
		var card_modifier = ""
		var badge_cls = "badge-same"
		if c["status"] == "CHANGE_WORSE":
			card_modifier = "card-worse"
			badge_cls = "badge-worse"
		elif c["status"] == "CHANGE_BETTER":
			card_modifier = "card-better"
			badge_cls = "badge-better"
		elif c["status"] == "SIMILAR":
			badge_cls = "badge-diff"
		elif c["status"] == "NO_FACE":
			badge_cls = "badge-noface"

		var meta_str = "Roll Hint: %s&deg;" % str(c["roll_hint"])
		if c.get("sensor_orient", 0.0) != 0.0:
			meta_str += " &bull; Sensor Orient: %s&deg;" % str(c["sensor_orient"])

		html += """    <div id='fixture-""" + c["anchor_id"] + """' class='fixture-card """ + card_modifier + """'>
      <div class='fixture-header'>
        <div class='fixture-title'>""" + c["file"] + """ <span class='fixture-meta'>(""" + meta_str + """)</span></div>
        <div class='badge """ + badge_cls + """'>""" + c["status"] + """</div>
      </div>

      <!-- Merged Property & Baseline Metric Table -->
      <table class='card-table'>
        <thead>
          <tr>
            <th>Metric</th>
            <th>Actual Value</th>
            <th>Actual Error</th>
            <th>Prev Golden Value</th>
            <th>Prev Golden Error</th>
            <th>Delta</th>
            <th>Status</th>
          </tr>
        </thead>
        <tbody>
"""
		for m in c["metrics"]:
			var d_cls = "delta-diff"
			var st_badge_cls = "badge-same"

			if m["status"] == "CHANGE_WORSE":
				d_cls = "delta-reg"
				st_badge_cls = "badge-worse"
			elif m["status"] == "CHANGE_BETTER":
				d_cls = "delta-imp"
				st_badge_cls = "badge-better"
			elif m["status"] == "SIMILAR":
				d_cls = "delta-diff"
				st_badge_cls = "badge-diff"
			elif m["status"] == "IDENTICAL":
				d_cls = "delta-ok"
				st_badge_cls = "badge-same"

			html += """          <tr>
            <td><strong>""" + m["name"] + """</strong></td>
            <td><code>""" + m["val"] + """</code></td>
            <td>""" + m["err"] + """</td>
            <td><code>""" + m["prev_val"] + """</code></td>
            <td>""" + m["prev_err"] + """</td>
            <td><span class='""" + d_cls + """'>""" + m["delta"] + """</span></td>
            <td><span class='badge """ + st_badge_cls + """' style='padding: 2px 8px; font-size: 11px;'>""" + m["status"] + """</span></td>
          </tr>\n"""

		# Openness summary row
		html += """          <tr>
            <td><strong>eye_openness</strong></td>
            <td colspan='6'>Left Eye (p0-1): <code>""" + ("%.2f" % c["left_open"]) + """</code> &bull; Right Eye (p2-3): <code>""" + ("%.2f" % c["right_open"]) + """</code></td>
          </tr>\n"""

		html += """        </tbody>
      </table>

      <div class='fixture-body'>
        <div class='eye-crops-col'>
"""
		if c["left_eye_rel"] != "":
			html += """          <div class='eye-box'>
            <img src='""" + c["left_eye_rel"] + """' alt='Left Eye' />
            <div class='eye-lbl'>LEFT EYE (p0-1)</div>
            <div class='eye-val'>""" + ("%.2f" % c["left_open"]) + """</div>
          </div>\n"""
		if c["right_eye_rel"] != "":
			html += """          <div class='eye-box'>
            <img src='""" + c["right_eye_rel"] + """' alt='Right Eye' />
            <div class='eye-lbl'>RIGHT EYE (p2-3)</div>
            <div class='eye-val'>""" + ("%.2f" % c["right_open"]) + """</div>
          </div>\n"""
		if c["left_eye_rel"] == "" and c["right_eye_rel"] == "":
			html += """          <div class='eye-box' style='padding: 24px 8px; color: #8b949e; font-size: 11px;'>
            NO FACE DETECTED
          </div>\n"""

		html += """        </div>
        <div class='overlay-col'>
          <a href='""" + c["overlay_rel"] + """' target='_blank' title='Click to open full resolution diagnostic overlay'>
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
