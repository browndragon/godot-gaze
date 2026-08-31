extends SceneTree

# ==============================================================================
# render_diagnostic_overlays.gd
#
# Generates side-by-side diagnostic visual overlays for all benchmark fixtures:
# - Left Panel:  Raw camera fixture with 10cm 3D coordinate transform axes at
#                Nose (+X right ear, +Y up head, -Z forward) and Eye Center,
#                plus insets for Left & Right Eye Crops with openness values.
# - Right Panel: Physical display mockup of 2021 14" MacBook Pro (301.5x188.5mm, 1512x982 pt)
#                with inset game window (756x491 pt) featuring a mock title bar
#                and letterboxed horizontally-mirrored webcam frame, overlaid with
#                5 distinct screen intersection points and a crisp legible legend.
# ==============================================================================

const SCREEN_WIDTH_MM: float = 301.5
const SCREEN_HEIGHT_MM: float = 188.5
const SCREEN_WIDTH_PT: float = 1512.0
const SCREEN_HEIGHT_PT: float = 982.0

const WIN_WIDTH_PT: float = 756.0
const WIN_HEIGHT_PT: float = 491.0
const WIN_POS_X_PT: float = 378.0
const WIN_POS_Y_PT: float = 245.5

const CAM_FOCAL_PX: float = 1440.0 / (2.0 * 0.6370702608) # 65 deg HFOV default
const CAM_CX_PX: float = 720.0
const CAM_CY_PX: float = 480.0

var gs: Object
var vs: Object
var cam_rid: RID
var s_cam_rid: RID
var disp_rid: RID
var face_rid: RID
var eye_rid: RID
var dev_cal: MockDeviceCalibration

var target_points_mm: Dictionary = {
	"self_center.jpg": Vector2(0.0, 0.0),
	"self_center2.jpg": Vector2(0.0, 0.0),
	"self_left_left.jpg": Vector2(-150.75, 0.0),
	"self_right_right.jpg": Vector2(150.75, 0.0),
	"self_top_top.jpg": Vector2(0.0, -94.25),
	"self_down_down.jpg": Vector2(0.0, 94.25),
	"self_nosedown_eyesup.jpg": Vector2(0.0, 0.0),
	"self_noseleft_eyesright.jpg": Vector2(150.75, 0.0),
	"self_noseright_eyesleft.jpg": Vector2(-150.75, 0.0),
	"self_nosetop_eyesdown.jpg": Vector2(0.0, 94.25),
	"self_roll_left.jpg": Vector2(0.0, 0.0),
	"self_roll_right.jpg": Vector2(0.0, 0.0),
	"self_yaw_left_roll_left.jpg": Vector2(-150.75, 0.0),
	"self_yaw_right_roll_left.jpg": Vector2(150.75, 0.0),
	"eyes_both_open.jpg": Vector2(0.0, 0.0),
	"eyes_both_wink.jpg": Vector2(0.0, 0.0),
	"eyes_anatomical_left_wink.jpg": Vector2(0.0, 0.0),
	"eyes_anatomical_right_wink.jpg": Vector2(0.0, 0.0),
	"eyes_tilted_anatomical_right_wink.jpg": Vector2(0.0, 0.0),
	"eyes_anatomical_left_obscured.jpg": Vector2(0.0, 0.0),
}

func _init() -> void:
	call_deferred("run_tool")

func run_tool() -> void:
	print("================================================================================")
	print("       GODOT-GAZE VISUAL DIAGNOSTIC OVERLAY TOOL (HEADLESS)")
	print("================================================================================")

	vs = Engine.get_singleton("VisionServer")
	gs = Engine.get_singleton("GazeServer")
	if not vs or not gs:
		printerr("FATAL: VisionServer or GazeServer singleton not found!")
		quit(1)
		return

	# Configure Synthetic Camera (Device ID -1 for software injection)
	cam_rid = vs.camera_create()
	vs.camera_set_device_id(cam_rid, -1)
	vs.camera_set_resolution(cam_rid, 1440, 960)
	vs.camera_set_focal_length(cam_rid, CAM_FOCAL_PX)
	vs.camera_start(cam_rid)

	# Configure Reference Snapshot Geometry (2021 14" MacBook Pro Capture Hardware)
	disp_rid = gs.display_create()
	dev_cal = MockDeviceCalibration.new()
	dev_cal.physical_size_mm = Vector2(SCREEN_WIDTH_MM, SCREEN_HEIGHT_MM)
	dev_cal.logical_size_px = Vector2i(int(SCREEN_WIDTH_PT), int(SCREEN_HEIGHT_PT))
	dev_cal.camera_offset = Vector3(0.0, 0.0, 0.0) # Top bezel center (X=150.75mm, Y=0.0mm)
	dev_cal.camera_tilt = 0.0
	dev_cal.set_window_position(Vector2(WIN_POS_X_PT, WIN_POS_Y_PT))
	gs.display_set_device_calibration(disp_rid, dev_cal)
	gs.set_device_calibration(dev_cal)

	s_cam_rid = gs.camera_create(disp_rid)
	gs.camera_set_offsets(s_cam_rid, Vector3(0.0, 0.0, 0.0), 0.0)
	gs.camera_set_vision_rid(s_cam_rid, cam_rid)
	face_rid = gs.face_tracker_create(s_cam_rid)
	eye_rid = gs.eye_tracker_create(face_rid)

	gs.camera_set_preview_requested(true)
	gs.eye_tracker_set_crop_requested(eye_rid, true)

	gs.start_processing()

	# Prepare output directory
	var global_out_dir = ProjectSettings.globalize_path("res://../build/tests/artifacts/overlays")
	DirAccess.make_dir_recursive_absolute(global_out_dir)

	# Discover test fixture files
	var global_res_dir = ProjectSettings.globalize_path("res://../tests/resources")
	if not DirAccess.dir_exists_absolute(global_res_dir):
		global_res_dir = ProjectSettings.globalize_path("res://tests/resources")

	var dir = DirAccess.open(global_res_dir)
	var fixture_files: Array[String] = []
	if dir:
		dir.list_dir_begin()
		var fn = dir.get_next()
		while fn != "":
			if not dir.current_is_dir() and (fn.begins_with("self_") or fn.begins_with("eyes_")) and fn.ends_with(".jpg"):
				fixture_files.append(fn)
			fn = dir.get_next()
		dir.list_dir_end()

	fixture_files.sort()
	print("Discovered %d test fixtures in %s\n" % [fixture_files.size(), global_res_dir])

	var summary_rows: Array[String] = [
		"# Visual Diagnostic Overlay Summary",
		"Generated on: %s\n" % Time.get_datetime_string_from_system(true),
		"| Image File | Nose Proj (pt) | Eye Proj (pt) | Delta Nose | Delta Eye | Target Err Nose | Target Err Eye | Status |",
		"| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |"
	]

	for img_name in fixture_files:
		var img_path = global_res_dir + "/" + img_name
		var img = Image.load_from_file(img_path)
		if not img:
			printerr("Skipping unreadable fixture: ", img_path)
			continue

		var ok = await inject_and_sync_frame(img)
		if not ok or not gs.is_face_detected(face_rid):
			print("[WARN] %-30s -> Face not detected or pipeline timeout" % img_name)
			summary_rows.append("| %s | N/A | N/A | N/A | N/A | N/A | N/A | NO_FACE |" % img_name)
			continue

		# 1. Read Tracking Data and Diagnostic Textures from GazeServer & VisionServer
		var head_pos = gs.get_head_pose_origin_mm(face_rid)
		var head_xform = gs.get_relative_transform(face_rid)
		var _head_rot = gs.get_head_pose_euler_deg(face_rid)
		var head_fwd = -head_xform.basis.z.normalized()
		var lm_pts = gs.get_face_landmarks(face_rid)

		var eye_orig = gs.get_gaze_origin_from_eye_tracker(eye_rid)
		if eye_orig == Vector3.ZERO:
			eye_orig = head_pos + Vector3(0.0, 30.0, 0.0) # Fallback to inter-ocular center
		var gaze_dir = gs.get_gaze_direction_from_eye_tracker(eye_rid)
		if gaze_dir == Vector3.ZERO:
			gaze_dir = head_fwd

		# Get diagnostic camera texture from GazeServer / VisionServer
		var cam_tex = vs.get_camera_current_texture(cam_rid)
		var camera_img: Image = cam_tex.get_image() if cam_tex != null else img

		# Get eye crops from GazeServer
		var eye_crops = gs.get_eye_crops(eye_rid)
		var left_crop: Image = eye_crops[0] if eye_crops.size() > 0 and eye_crops[0] != null else null
		var right_crop: Image = eye_crops[1] if eye_crops.size() > 1 and eye_crops[1] != null else null

		# 2. GazeServer Reported Points (Viewport -> Screen)
		var nose_win_reported = gs.project_ray_to_viewport(head_pos, head_fwd, false)
		var eye_win_reported = gs.get_projected_gaze_from_eye_tracker(eye_rid, false)

		var nose_screen_reported = nose_win_reported + Vector2(WIN_POS_X_PT, WIN_POS_Y_PT)
		var eye_screen_reported = eye_win_reported + Vector2(WIN_POS_X_PT, WIN_POS_Y_PT)

		# 3. Analytical 3D Ray-Plane Intersections
		var nose_screen_analytical = analytical_ray_plane_screen_pt(head_pos, head_fwd)
		var eye_screen_analytical = analytical_ray_plane_screen_pt(eye_orig, gaze_dir)

		if img_name == "self_left_left.jpg":
			print("DEBUG self_left_left: head_pos=", head_pos, " head_fwd=", head_fwd, " eye_orig=", eye_orig, " gaze_dir=", gaze_dir)

		# 4. Target Ground Truth
		var target_mm = target_points_mm.get(img_name, Vector2(0.0, 0.0))
		var target_screen_pt = mm_to_screen_pt(target_mm)

		# 5. Discrepancy Deltas
		var delta_nose_pt = (nose_screen_analytical - nose_screen_reported).length()
		var delta_eye_pt = (eye_screen_analytical - eye_screen_reported).length()
		var delta_nose_mm = delta_nose_pt * (SCREEN_WIDTH_MM / SCREEN_WIDTH_PT)
		var delta_eye_mm = delta_eye_pt * (SCREEN_WIDTH_MM / SCREEN_WIDTH_PT)

		var err_nose_mm = (nose_screen_reported - target_screen_pt).length() * (SCREEN_WIDTH_MM / SCREEN_WIDTH_PT)
		var err_eye_mm = (eye_screen_reported - target_screen_pt).length() * (SCREEN_WIDTH_MM / SCREEN_WIDTH_PT)

		var status_str = "OK" if (delta_nose_mm < 0.5 and delta_eye_mm < 0.5) else "MATH_DRIFT"

		print("  -> %-30s | Nose: (%6.1f, %6.1f) | Gaze: (%6.1f, %6.1f) | ΔNose: %4.1fmm | ΔEye: %4.1fmm | %s" % [
			img_name, nose_screen_reported.x, nose_screen_reported.y,
			eye_screen_reported.x, eye_screen_reported.y,
			delta_nose_mm, delta_eye_mm, status_str
		])

		summary_rows.append("| %s | (%d,%d) | (%d,%d) | %.1f mm | %.1f mm | %.1f mm | %.1f mm | %s |" % [
			img_name, int(nose_screen_reported.x), int(nose_screen_reported.y),
			int(eye_screen_reported.x), int(eye_screen_reported.y),
			delta_nose_mm, delta_eye_mm, err_nose_mm, err_eye_mm, status_str
		])

		# 6. Render Left Panel (Camera View + 3D Axes + Eye Crop Insets)
		var left_panel = render_left_panel(camera_img, head_pos, head_xform, eye_orig, gaze_dir, lm_pts, left_crop, right_crop)

		# 7. Render Right Panel (Physical Display Mockup + Central Window with Titlebar & Mirror Photo)
		var right_panel = render_right_panel(
			camera_img, img_name, target_screen_pt,
			nose_screen_analytical, eye_screen_analytical,
			nose_screen_reported, eye_screen_reported,
			delta_nose_mm, delta_eye_mm, err_nose_mm, err_eye_mm
		)

		# 8. Create Side-by-Side Composite
		var composite = create_side_by_side_composite(left_panel, right_panel, img_name)
		var out_png_path = global_out_dir + "/" + img_name.replace(".jpg", "_diagnostic.png")
		composite.save_png(out_png_path)

	# Write summary report markdown
	var summary_text = "\n".join(summary_rows) + "\n"
	var sum_file = FileAccess.open(global_out_dir + "/overlay_summary.md", FileAccess.WRITE)
	if sum_file:
		sum_file.store_string(summary_text)
		sum_file.close()

	print("\n================================================================================")
	print("DIAGNOSTIC OVERLAYS GENERATED SUCCESSFULLY IN: %s" % global_out_dir)
	print("================================================================================")

	# Cleanup
	gs.eye_tracker_free(eye_rid)
	gs.face_tracker_free(face_rid)
	gs.camera_free(s_cam_rid)
	gs.display_free(disp_rid)
	gs.stop_tracking(true)
	vs.camera_stop(cam_rid)
	vs.camera_free(cam_rid)

	quit(0)

# ==============================================================================
# Helper Functions: Math & Projections
# ==============================================================================

func inject_and_sync_frame(img: Image) -> bool:
	var tex = ImageTexture.create_from_image(img)
	vs.inject_texture(cam_rid, tex)

	for _i in range(15):
		gs.trigger_process()
		await create_timer(0.04).timeout

	return gs.is_face_detected(face_rid)

func project_cam_3d_to_img_2d(p3d: Vector3, img_w: float, img_h: float) -> Vector2:
	var z_depth = -p3d.z
	if z_depth <= 1.0:
		return Vector2(-9999.0, -9999.0)
	var u = (img_w * 0.5) + (CAM_FOCAL_PX * p3d.x) / z_depth
	var v = (img_h * 0.5) - (CAM_FOCAL_PX * p3d.y) / z_depth
	return Vector2(u, v)

func analytical_ray_plane_screen_pt(origin_cam: Vector3, dir_cam: Vector3) -> Vector2:
	if dir_cam.z <= 0.001:
		return Vector2(-9999.0, -9999.0)
	var t = -origin_cam.z / dir_cam.z
	var pos_mm_x = (SCREEN_WIDTH_MM * 0.5) - (origin_cam.x + dir_cam.x * t)
	var pos_mm_y = -(origin_cam.y + dir_cam.y * t)
	var scale_x = SCREEN_WIDTH_PT / SCREEN_WIDTH_MM
	var scale_y = SCREEN_HEIGHT_PT / SCREEN_HEIGHT_MM
	return Vector2(pos_mm_x * scale_x, pos_mm_y * scale_y)

func mm_to_screen_pt(pos_mm_from_center: Vector2) -> Vector2:
	var x_pt = (pos_mm_from_center.x * (SCREEN_WIDTH_PT / SCREEN_WIDTH_MM)) + (SCREEN_WIDTH_PT * 0.5)
	var y_pt = (pos_mm_from_center.y * (SCREEN_HEIGHT_PT / SCREEN_HEIGHT_MM)) + (SCREEN_HEIGHT_PT * 0.5)
	return Vector2(x_pt, y_pt)

# ==============================================================================
# Rendering: Left Panel (Camera Fixture View + 3D Axes + Eye Crops)
# ==============================================================================

func render_left_panel(
	img: Image, head_pos: Vector3, head_xform: Transform3D,
	eye_orig: Vector3, gaze_dir: Vector3, lm_pts: PackedVector2Array,
	left_crop: Image, right_crop: Image
) -> Image:
	var panel = img.duplicate()
	panel.convert(Image.FORMAT_RGBA8)
	var w = float(panel.get_width())
	var h = float(panel.get_height())

	# Draw 2D Facial Landmarks
	for pt in lm_pts:
		draw_circle(panel, int(pt.x), int(pt.y), 3, Color(0.0, 0.9, 1.0, 0.9))

	# 10cm (100mm) Nose Transform Axis (Green Theme)
	var nose_origin_2d = project_cam_3d_to_img_2d(head_pos, w, h)
	var axis_len = 100.0 # 10cm

	var nose_x_3d = head_pos + head_xform.basis.x.normalized() * axis_len
	var nose_y_3d = head_pos + head_xform.basis.y.normalized() * axis_len
	var nose_z_3d = head_pos - head_xform.basis.z.normalized() * axis_len # -Z towards camera

	var nose_x_2d = project_cam_3d_to_img_2d(nose_x_3d, w, h)
	var nose_y_2d = project_cam_3d_to_img_2d(nose_y_3d, w, h)
	var nose_z_2d = project_cam_3d_to_img_2d(nose_z_3d, w, h)

	if is_valid_pt(nose_origin_2d):
		if is_valid_pt(nose_x_2d):
			draw_thick_line(panel, int(nose_origin_2d.x), int(nose_origin_2d.y), int(nose_x_2d.x), int(nose_x_2d.y), Color.RED, 4)
		if is_valid_pt(nose_y_2d):
			draw_thick_line(panel, int(nose_origin_2d.x), int(nose_origin_2d.y), int(nose_y_2d.x), int(nose_y_2d.y), Color.GREEN, 4)
		if is_valid_pt(nose_z_2d):
			draw_thick_line(panel, int(nose_origin_2d.x), int(nose_origin_2d.y), int(nose_z_2d.x), int(nose_z_2d.y), Color(0.1, 1.0, 0.2, 1.0), 5)
		draw_circle(panel, int(nose_origin_2d.x), int(nose_origin_2d.y), 6, Color.WHITE)

	# 10cm (100mm) Eye Center Gaze Axis (Teal Theme)
	var eye_origin_2d = project_cam_3d_to_img_2d(eye_orig, w, h)
	var eye_fwd_3d = eye_orig + gaze_dir.normalized() * axis_len
	var eye_fwd_2d = project_cam_3d_to_img_2d(eye_fwd_3d, w, h)

	if is_valid_pt(eye_origin_2d):
		if is_valid_pt(eye_fwd_2d):
			draw_thick_line(panel, int(eye_origin_2d.x), int(eye_origin_2d.y), int(eye_fwd_2d.x), int(eye_fwd_2d.y), Color(0.0, 0.95, 0.95, 1.0), 5)
		draw_circle(panel, int(eye_origin_2d.x), int(eye_origin_2d.y), 6, Color.CYAN)

	# Panel Header Bar
	draw_filled_rect(panel, 0, 0, int(w), 36, Color(0.05, 0.05, 0.08, 0.90))
	draw_bitmap_text(panel, 14, 10, "CAMERA VIEW & 3D TRANSFORM AXES", Color.WHITE, 2)
	draw_rect(panel, 0, 0, int(w), int(h), Color(0.3, 0.3, 0.35, 1.0), 2)

	# Draw Inset Eye Crop Cards at bottom-left
	var card_y = int(h) - 170
	var card_w = 340
	var card_h = 154
	draw_filled_rect(panel, 16, card_y, card_w, card_h, Color(0.05, 0.05, 0.08, 0.92))
	draw_rect(panel, 16, card_y, card_w, card_h, Color(0.3, 0.6, 0.9, 1.0), 2)
	draw_bitmap_text(panel, 26, card_y + 10, "MODEL EYE CROP INPUTS (60x60)", Color(0.3, 0.8, 1.0, 1.0), 1)

	var crop_display_sz = 100

	# Left Crop (Anatomical Left / Image Right)
	if left_crop:
		var lc = left_crop.duplicate()
		lc.convert(Image.FORMAT_RGBA8)
		lc.resize(crop_display_sz, crop_display_sz, Image.INTERPOLATE_BILINEAR)
		panel.blit_rect(lc, Rect2i(0, 0, crop_display_sz, crop_display_sz), Vector2i(28, card_y + 32))
		draw_rect(panel, 28, card_y + 32, crop_display_sz, crop_display_sz, Color.CYAN, 1)
		draw_bitmap_text(panel, 36, card_y + 136, "LEFT EYE", Color.WHITE, 1)

	# Right Crop (Anatomical Right / Image Left)
	if right_crop:
		var rc = right_crop.duplicate()
		rc.convert(Image.FORMAT_RGBA8)
		rc.resize(crop_display_sz, crop_display_sz, Image.INTERPOLATE_BILINEAR)
		panel.blit_rect(rc, Rect2i(0, 0, crop_display_sz, crop_display_sz), Vector2i(180, card_y + 32))
		draw_rect(panel, 180, card_y + 32, crop_display_sz, crop_display_sz, Color.CYAN, 1)
		draw_bitmap_text(panel, 184, card_y + 136, "RIGHT EYE", Color.WHITE, 1)

	return panel

# ==============================================================================
# Rendering: Right Panel (Physical Display Mockup + Ray Intersections)
# ==============================================================================

func render_right_panel(
	img: Image, _img_name: String, target_pt: Vector2,
	nose_ana: Vector2, eye_ana: Vector2,
	nose_rep: Vector2, eye_rep: Vector2,
	delta_nose_mm: float, delta_eye_mm: float,
	err_nose_mm: float, err_eye_mm: float
) -> Image:
	# Canvas dimensions with margin around 1512x982 display
	var margin_x = 160
	var margin_y = 120
	var canvas_w = int(SCREEN_WIDTH_PT) + margin_x * 2 # 1832 px
	var canvas_h = int(SCREEN_HEIGHT_PT) + margin_y * 2 # 1222 px

	var canvas = Image.create(canvas_w, canvas_h, false, Image.FORMAT_RGBA8)
	canvas.fill(Color(0.08, 0.08, 0.10, 1.0)) # Dark surrounding desk

	# Screen bezel & background
	var screen_x = margin_x
	var screen_y = margin_y
	draw_filled_rect(canvas, screen_x, screen_y, int(SCREEN_WIDTH_PT), int(SCREEN_HEIGHT_PT), Color(0.14, 0.14, 0.17, 1.0))
	draw_rect(canvas, screen_x, screen_y, int(SCREEN_WIDTH_PT), int(SCREEN_HEIGHT_PT), Color(0.5, 0.5, 0.55, 1.0), 3)

	# Screen Center Crosshair (Subtle gray)
	var screen_cx = screen_x + int(SCREEN_WIDTH_PT * 0.5)
	var screen_cy = screen_y + int(SCREEN_HEIGHT_PT * 0.5)
	draw_crosshair(canvas, screen_cx, screen_cy, 24, Color(0.35, 0.35, 0.40, 1.0))

	# Webcam Notch / Bezel Indicator (Top center)
	var cam_px_x = screen_cx
	var cam_px_y = screen_y
	draw_circle(canvas, cam_px_x, cam_px_y, 8, Color.DARK_RED)
	draw_circle(canvas, cam_px_x, cam_px_y, 4, Color.RED)

	# Inset Centered Game Viewport Window (756x491 pt)
	var win_x = screen_x + int(WIN_POS_X_PT)
	var win_y = screen_y + int(WIN_POS_Y_PT)
	var win_w = int(WIN_WIDTH_PT)
	var win_h = int(WIN_HEIGHT_PT)

	# 1. Window Mock Title Bar (macOS Style)
	var titlebar_h = 28
	draw_filled_rect(canvas, win_x, win_y, win_w, titlebar_h, Color(0.20, 0.20, 0.24, 1.0))
	draw_rect(canvas, win_x, win_y, win_w, win_h, Color(0.3, 0.6, 1.0, 1.0), 2)
	draw_line(canvas, win_x, win_y + titlebar_h, win_x + win_w, win_y + titlebar_h, Color(0.3, 0.3, 0.35, 1.0), 1)

	# Traffic light buttons
	draw_circle(canvas, win_x + 16, win_y + 14, 5, Color(1.0, 0.38, 0.35, 1.0)) # Close (Red)
	draw_circle(canvas, win_x + 32, win_y + 14, 5, Color(1.0, 0.75, 0.25, 1.0)) # Minimize (Yellow)
	draw_circle(canvas, win_x + 48, win_y + 14, 5, Color(0.30, 0.85, 0.40, 1.0)) # Maximize (Green)
	draw_bitmap_text(canvas, win_x + 70, win_y + 9, "Godot Gaze Game Viewport (756x491 pt)", Color(0.8, 0.8, 0.85, 1.0), 1)

	# 2. Interior Viewport: Full Horizontally-Mirrored Letterboxed Camera Frame
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

	# Helper to map screen points to canvas pixel coordinates
	var to_canvas = func(p_screen: Vector2) -> Vector2i:
		return Vector2i(screen_x + int(p_screen.x), screen_y + int(p_screen.y))

	# Draw Ray Intersections:
	# 1. Target Point (Red)
	if is_valid_pt(target_pt):
		var cp = to_canvas.call(target_pt)
		draw_circle(canvas, cp.x, cp.y, 8, Color.RED)
		draw_crosshair(canvas, cp.x, cp.y, 16, Color.RED)

	# 2. Analytical 3D Nose Ray-Plane (Dim Green)
	if is_valid_pt(nose_ana):
		var cp = to_canvas.call(nose_ana)
		draw_thick_line(canvas, cam_px_x, cam_px_y, cp.x, cp.y, Color(0.1, 0.6, 0.2, 0.35), 2)
		draw_circle(canvas, cp.x, cp.y, 10, Color(0.1, 0.7, 0.2, 0.7))

	# 3. Analytical 3D Eye Gaze Ray-Plane (Dim Teal)
	if is_valid_pt(eye_ana):
		var cp = to_canvas.call(eye_ana)
		draw_thick_line(canvas, cam_px_x, cam_px_y, cp.x, cp.y, Color(0.0, 0.6, 0.7, 0.35), 2)
		draw_circle(canvas, cp.x, cp.y, 10, Color(0.0, 0.7, 0.8, 0.7))

	# 4. GazeServer Reported 2D Nose Point (Bright Green)
	if is_valid_pt(nose_rep):
		var cp = to_canvas.call(nose_rep)
		draw_thick_line(canvas, cam_px_x, cam_px_y, cp.x, cp.y, Color(0.2, 1.0, 0.3, 0.6), 2)
		draw_circle(canvas, cp.x, cp.y, 8, Color(0.2, 1.0, 0.3, 1.0))
		draw_crosshair(canvas, cp.x, cp.y, 14, Color.WHITE)

	# 5. GazeServer Reported 2D Eye Gaze Point (Bright Teal)
	if is_valid_pt(eye_rep):
		var cp = to_canvas.call(eye_rep)
		draw_thick_line(canvas, cam_px_x, cam_px_y, cp.x, cp.y, Color(0.0, 0.95, 1.0, 0.6), 2)
		draw_circle(canvas, cp.x, cp.y, 8, Color(0.0, 0.95, 1.0, 1.0))
		draw_crosshair(canvas, cp.x, cp.y, 14, Color.WHITE)

	# Crisp Legible Legend Box (Top Left)
	var leg_w = 480
	var leg_h = 175
	var leg_x = screen_x + 16
	var leg_y = screen_y + 16
	draw_filled_rect(canvas, leg_x, leg_y, leg_w, leg_h, Color(0.04, 0.04, 0.07, 0.94))
	draw_rect(canvas, leg_x, leg_y, leg_w, leg_h, Color(0.4, 0.5, 0.65, 1.0), 2)

	# Title
	draw_bitmap_text(canvas, leg_x + 14, leg_y + 12, "SCREEN PROJECTION DIAGNOSTICS", Color(0.3, 0.8, 1.0, 1.0), 1)

	# Legend Items with Crisp Text
	# Target Point
	draw_circle(canvas, leg_x + 22, leg_y + 36, 6, Color.RED)
	draw_bitmap_text(canvas, leg_x + 36, leg_y + 32, "Target Point: (%d, %d) pt" % [int(target_pt.x), int(target_pt.y)], Color(1.0, 0.6, 0.6, 1.0), 1)

	# Nose Point
	draw_circle(canvas, leg_x + 22, leg_y + 58, 6, Color(0.2, 1.0, 0.3, 1.0))
	draw_bitmap_text(canvas, leg_x + 36, leg_y + 54, "Nose Gaze:    (%d, %d) pt  | Err: %.1f mm" % [int(nose_rep.x), int(nose_rep.y), err_nose_mm], Color(0.6, 1.0, 0.6, 1.0), 1)

	# Eye Point
	draw_circle(canvas, leg_x + 22, leg_y + 80, 6, Color(0.0, 0.95, 1.0, 1.0))
	draw_bitmap_text(canvas, leg_x + 36, leg_y + 76, "Eye Gaze:     (%d, %d) pt  | Err: %.1f mm" % [int(eye_rep.x), int(eye_rep.y), err_eye_mm], Color(0.5, 0.95, 1.0, 1.0), 1)

	# Delta Indicators
	var delta_box_col = Color(0.1, 0.7, 0.2, 1.0) if (delta_nose_mm < 0.5 and delta_eye_mm < 0.5) else Color(1.0, 0.4, 0.2, 1.0)
	draw_filled_rect(canvas, leg_x + 10, leg_y + 104, leg_w - 20, 58, Color(0.08, 0.08, 0.12, 1.0))
	draw_rect(canvas, leg_x + 10, leg_y + 104, leg_w - 20, 58, delta_box_col, 1)

	draw_bitmap_text(canvas, leg_x + 18, leg_y + 114, "MATH DELTA (Analytical vs GazeServer):", Color.WHITE, 1)
	draw_bitmap_text(canvas, leg_x + 18, leg_y + 136, "Delta Nose = %.2f mm  |  Delta Eye = %.2f mm  [%s]" % [delta_nose_mm, delta_eye_mm, "EXACT 0.0mm" if delta_nose_mm < 0.01 else "MISMATCH"], delta_box_col, 1)

	return canvas

# ==============================================================================
# Rendering: Side-by-Side Composite
# ==============================================================================

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

	# Header Bar
	draw_filled_rect(comp, 0, 0, total_w, header_h, Color(0.08, 0.08, 0.12, 1.0))
	draw_rect(comp, 0, 0, total_w, header_h, Color(0.25, 0.25, 0.35, 1.0), 1)
	draw_bitmap_text(comp, 20, 14, "DIAGNOSTIC VISUALIZATION: " + title, Color(0.9, 0.9, 0.95, 1.0), 2)

	# Blit Panels
	comp.blit_rect(scaled_l, Rect2i(0, 0, scaled_l.get_width(), target_h), Vector2i(0, header_h))
	comp.blit_rect(scaled_r, Rect2i(0, 0, scaled_r.get_width(), target_h), Vector2i(scaled_l.get_width() + 16, header_h))

	# Divider line
	draw_thick_line(comp, scaled_l.get_width() + 8, header_h, scaled_l.get_width() + 8, target_h + header_h, Color(0.3, 0.3, 0.4, 1.0), 2)

	return comp

# ==============================================================================
# Drawing Primitives & Crisp Bitmap Font Engine
# ==============================================================================

func is_valid_pt(p: Vector2) -> bool:
	return p.x > -9000.0 and p.y > -9000.0 and not is_nan(p.x) and not is_nan(p.y) and not is_inf(p.x) and not is_inf(p.y)

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
	draw_thick_line(img, x0, y0, x1, y1, col, thickness)

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

# 5x7 Standard Crisp Bitmap Font Definitions
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
