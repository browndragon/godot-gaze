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
	print("=== EVALUATING RECORDED VIDEO FRAMES VIA GAZE TRACKING PIPELINE ===")
	print("==================================================================\n")

	var vs = Engine.get_singleton("VisionServer")
	var gs = Engine.get_singleton("GazeServer")

	if not vs or not gs:
		printerr("Gaze singletons not available!")
		quit(1)
		return

	var logical_sz = Vector2i(3024, 1964)
	var physical_mm = Vector2(301.5, 188.5)

	var dev_cal = MockDeviceCalibration.new()
	dev_cal.logical_size_px = logical_sz
	dev_cal.physical_size_mm = physical_mm
	dev_cal.camera_offset = Vector3(0.0, 0.0, 0.0)
	dev_cal.camera_tilt = 0.0
	dev_cal.set_window_position_lpix(Vector2(0, 0))
	gs.set_device_calibration(dev_cal)

	var cam_rid = vs.camera_create()
	vs.camera_set_device_id(cam_rid, -1)
	vs.camera_start(cam_rid)

	gs.set_camera_offsets(Vector3(0.0, 0.0, 0.0), 0.0)
	gs.set_camera_vision_rid(cam_rid)
	gs.set_smoother(null)
	gs.start_processing()

	var slice_dir = "res://../build/tests/artifacts/recorded_slices"
	var global_dir = ProjectSettings.globalize_path(slice_dir)

	var files = [
		"self_center.jpg",
		"self_top_left.jpg",
		"self_top_right.jpg",
		"self_bottom_left.jpg",
		"self_bottom_right.jpg",
		"self_yaw_left.jpg",
		"self_yaw_right.jpg",
		"self_pitch_down.jpg",
		"self_pitch_up.jpg",
		"self_roll_left.jpg",
		"self_roll_right.jpg",
		"eyes_both_open.jpg",
		"eyes_both_wink.jpg",
		"eyes_left_wink.jpg"
	]

	print("| %-22s | %-12s | %-20s | %-20s | %-17s | %-18s |" % [
		"Image File", "Face Detected", "Head Pos (mm)", "Head Rot (deg)", "Left/Right Open", "Gaze Proj (mm)"
	])
	print("|" + "-".repeat(24) + "|" + "-".repeat(14) + "|" + "-".repeat(22) + "|" + "-".repeat(22) + "|" + "-".repeat(19) + "|" + "-".repeat(20) + "|")

	for filename in files:
		var img_path = global_dir + "/" + filename
		if not FileAccess.file_exists(img_path):
			print("File not found: ", img_path)
			continue

		var img = Image.load_from_file(img_path)
		var w = img.get_width()
		var h = img.get_height()
		var focal = float(w) / (2.0 * tan(deg_to_rad(65.0) * 0.5))

		vs.camera_set_resolution(cam_rid, w, h)
		vs.camera_set_focal_length(cam_rid, focal)
		gs.reset()

		var tex = ImageTexture.create_from_image(img)
		vs.inject_texture(cam_rid, tex)
		gs.trigger_process()
		await gs.gaze_frame_ready

		var face_det = gs.is_face_detected()
		var head_pos = gs.get_head_position()
		var head_rot = gs.get_head_pose_euler_deg()
		var left_open = gs.get_left_eye_openness()
		var right_open = gs.get_right_eye_openness()
		var gaze_px = gs.get_gaze_screen_px(false)
		var gaze_mm = px_to_screen_mm(gaze_px, logical_sz, physical_mm)

		var pos_str = "(%.1f, %.1f, %.1f)" % [head_pos.x, head_pos.y, head_pos.z] if face_det else "NO_FACE"
		var rot_str = "(%.1f, %.1f, %.1f)" % [head_rot.x, head_rot.y, head_rot.z] if face_det else "NO_FACE"
		var open_str = "%.2f / %.2f" % [left_open, right_open] if face_det else "N/A"
		var gaze_str = "(%.1f, %.1f)" % [gaze_mm.x, gaze_mm.y] if face_det else "NO_FACE"

		print("| %-22s | %-12s | %-20s | %-20s | %-17s | %-18s |" % [
			filename, str(face_det), pos_str, rot_str, open_str, gaze_str
		])

	print("\n==================================================================\n")

	gs.stop_tracking(true)
	vs.camera_stop(cam_rid)
	vs.camera_free(cam_rid)
	quit(0)
