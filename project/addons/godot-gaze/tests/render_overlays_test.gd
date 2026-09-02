extends SceneTree

func _init():
	call_deferred("run_render")

func run_render():
	print("=================== RENDERING BENCHMARK OVERLAY IMAGES ===================")
	var vs = Engine.get_singleton("VisionServer")
	var gs = Engine.get_singleton("GazeServer")
	
	if not vs or not gs:
		printerr("Servers not available")
		quit(1)
		return

	# Setup calibrated MacBook Pro 14" geometry
	var dev_cal = MockDeviceCalibration.new()
	dev_cal.physical_size_mm = Vector2(301.5, 188.5)
	dev_cal.logical_size_px = Vector2i(1512, 982)
	dev_cal.camera_offset = Vector3(0.0, 94.25, 0.0) # top center
	dev_cal.camera_tilt = 0.0
	dev_cal.set_window_position_lpix(Vector2(180, 167)) # Centered 1152x648 window
	gs.set_device_calibration(dev_cal)

	var cam_rid = vs.camera_create()
	vs.camera_set_device_id(cam_rid, -1)
	vs.camera_set_resolution(cam_rid, 1440, 960)
	vs.camera_set_focal_length(cam_rid, 1440.0)
	vs.camera_start(cam_rid)

	gs.set_camera_offsets(Vector3(0.0, 94.25, 0.0), 0.0)
	gs.set_camera_vision_rid(cam_rid)

	gs.start_processing()

	var images = [
		"self_center.jpg",
		"self_left_left.jpg",
		"self_right_right.jpg",
		"self_top_top.jpg",
		"self_down_down.jpg",
		"self_nosedown_eyesup.jpg",
		"self_noseleft_eyesright.jpg",
		"self_noseright_eyesleft.jpg",
		"self_nosetop_eyesdown.jpg"
	]

	var out_dir = "/Users/acunningham/.gemini/antigravity/brain/fdec8b16-15ac-4c0a-93a9-4db14cdc4ef1/scratch/overlays"
	DirAccess.make_dir_recursive_absolute(out_dir)

	for img_name in images:
		var img_path = "tests/resources/" + img_name
		if not FileAccess.file_exists(img_path):
			img_path = "../tests/resources/" + img_name
		if not FileAccess.file_exists(img_path):
			printerr("Missing image: ", img_path)
			continue

		var img = Image.load_from_file(img_path)
		if not img:
			continue

		var tex = ImageTexture.create_from_image(img)
		vs.inject_texture(cam_rid, tex)

		for k in range(10):
			gs.trigger_process()
			await create_timer(0.04).timeout

		if not gs.is_face_detected():
			print("[%s] No face tracked or not converged" % img_name)
			continue

		var head_pos = gs.get_head_position()
		var head_rot = gs.get_head_rotation()
		var head_xform = gs.get_head_transform()
		var head_fwd = -head_xform.basis.z.normalized()
		var lm_pts = gs.get_face_landmarks_2d()

		# Project to window coordinates
		var nose_win = gs.project_ray_to_viewport(head_pos, head_fwd, false)
		var eye_win = gs.get_gaze_screen_px(false)

		# Screen coordinates
		var win_pos = dev_cal.get_window_position_lpix()
		var nose_screen = nose_win + win_pos
		var eye_screen = eye_win + win_pos

		print("--------------------------------------------------")
		print("Image: ", img_name)
		print("  Head Trans: ", head_pos, " | Rot (deg): ", head_rot)
		print("  Head Fwd:   ", head_fwd)
		print("  Nose Screen: ", nose_screen, " | Win: ", nose_win)
		print("  Eye Screen:  ", eye_screen, " | Win: ", eye_win)

		# Create a visual composite image:
		# Width: 1512 px (Screen width), Height: 982 px (Screen height)
		var canvas = Image.create(1512, 982, false, Image.FORMAT_RGBA8)
		canvas.fill(Color(0.10, 0.10, 0.12, 1.0)) # Dark Screen Background

		# Draw Screen Border (Light Gray)
		draw_rect(canvas, 0, 0, 1512, 982, Color(0.35, 0.35, 0.35, 1.0), 2)

		# Draw Screen Center Crosshair (Yellow)
		draw_crosshair(canvas, 1512 / 2, 982 / 2, 25, Color.YELLOW)

		# Draw Window Rectangle (Blue border, 1152x648 at 180, 167)
		var win_rect = Rect2i(180, 167, 1152, 648)
		draw_filled_rect(canvas, win_rect.position.x, win_rect.position.y, win_rect.size.x, win_rect.size.y, Color(0.16, 0.17, 0.20, 1.0))
		draw_rect(canvas, win_rect.position.x, win_rect.position.y, win_rect.size.x, win_rect.size.y, Color(0.2, 0.6, 1.0, 1.0), 3)

		# Draw Window Center Crosshair (Cyan)
		draw_crosshair(canvas, win_rect.position.x + win_rect.size.x / 2, win_rect.position.y + win_rect.size.y / 2, 18, Color(0.2, 0.8, 1.0, 1.0))

		# Draw Webcam at Top Center (0, 0 in mm -> 756, 0 in px)
		draw_circle(canvas, 756, 12, 10, Color(0.9, 0.2, 0.2, 1.0))
		draw_circle(canvas, 756, 12, 4, Color.WHITE)

		# Prepare mirrored camera preview (384x256 px) with landmarks & 3D ray overlays
		var preview = Image.new()
		preview.copy_from(img)
		preview.resize(384, 256)
		preview.flip_x() # Horizontally mirror for webcam view
		preview.convert(Image.FORMAT_RGBA8)

		# Draw 2D facial landmarks on mirrored preview
		var lms = gs.get_face_landmarks_2d()
		var orig_w = float(img.get_width())
		var orig_h = float(img.get_height())
		for pt in lms:
			var px = int((1.0 - pt.x / orig_w) * 384.0) # mirror X
			var py = int((pt.y / orig_h) * 256.0)
			draw_circle(preview, px, py, 2, Color(0.0, 0.95, 1.0, 1.0))

		# Inset mirrored camera preview in upper-right of the Godot window
		var prev_x = win_rect.position.x + win_rect.size.x - 404
		var prev_y = win_rect.position.y + 20
		canvas.blit_rect(preview, Rect2i(0, 0, 384, 256), Vector2i(prev_x, prev_y))
		draw_rect(canvas, prev_x, prev_y, 384, 256, Color(0.0, 0.8, 1.0, 0.8), 2)

		# Draw Nose Gaze Point & Ray Line (Cyan)
		var cl_nx = int(clamp(nose_screen.x, 0.0, 1511.0))
		var cl_ny = int(clamp(nose_screen.y, 0.0, 981.0))
		draw_line(canvas, 756, 12, cl_nx, cl_ny, Color(0.0, 0.8, 0.9, 0.5), 2)
		draw_circle(canvas, cl_nx, cl_ny, 12, Color(0.0, 0.95, 1.0, 1.0))
		draw_circle(canvas, cl_nx, cl_ny, 4, Color.WHITE)
		draw_crosshair(canvas, cl_nx, cl_ny, 18, Color(0.0, 0.95, 1.0, 1.0))

		# Draw Eye Gaze Point & Ray Line (Green)
		var cl_ex = int(clamp(eye_screen.x, 0.0, 1511.0))
		var cl_ey = int(clamp(eye_screen.y, 0.0, 981.0))
		draw_line(canvas, 756, 12, cl_ex, cl_ey, Color(0.2, 0.9, 0.1, 0.5), 2)
		draw_circle(canvas, cl_ex, cl_ey, 12, Color(0.2, 1.0, 0.1, 1.0))
		draw_circle(canvas, cl_ex, cl_ey, 4, Color.WHITE)
		draw_crosshair(canvas, cl_ex, cl_ey, 18, Color(0.2, 1.0, 0.1, 1.0))

		var out_file = out_dir + "/" + img_name.replace(".jpg", "_overlay.png")
		canvas.save_png(out_file)
		print("Saved overlay to: ", out_file)

	gs.stop_tracking(true)
	vs.camera_stop(cam_rid)
	vs.camera_free(cam_rid)

	print("=================== OVERLAYS GENERATED SUCCESSFULLY ===================")
	quit(0)

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

func draw_line(img: Image, x0: int, y0: int, x1: int, y1: int, col: Color, thickness: int = 1):
	var dx = abs(x1 - x0)
	var dy = -abs(y1 - y0)
	var sx = 1 if x0 < x1 else -1
	var sy = 1 if y0 < y1 else -1
	var err = dx + dy
	var cx = x0
	var cy = y0
	while true:
		for tx in range(-thickness / 2, thickness / 2 + 1):
			for ty in range(-thickness / 2, thickness / 2 + 1):
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
