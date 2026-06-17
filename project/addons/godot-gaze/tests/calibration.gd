# Interactive Calibration & Verification Dashboard
extends Control

@onready var tracker = $GazeTracker
@onready var cursor = $Cursor

# State variables
enum State { STATE_FREE_PLAY, STATE_CALIBRATING, STATE_VERIFYING }
var current_state = State.STATE_FREE_PLAY

# UI References
var sidebar: PanelContainer
var status_label: Label
var report_label: RichTextLabel
var config_tilt: SpinBox
var config_offset_x: SpinBox
var config_offset_y: SpinBox
var config_offset_z: SpinBox
var config_size_w: SpinBox
var config_size_h: SpinBox
var mode_option: OptionButton

# Calibration parameters
var calib_session: GazeCalibrationSession
var calib_points = [
	Vector2(0.5, 0.5),   # Center
	Vector2(0.1, 0.1),   # Top Left
	Vector2(0.9, 0.1),   # Top Right
	Vector2(0.1, 0.9),   # Bottom Left
	Vector2(0.9, 0.9)    # Bottom Right
]
var current_target_idx = 0
var target_timer = 0.0
var target_hold_time = 1.5
var current_target_screen_pos = Vector2.ZERO

# Verification parameters
var verify_points = []
var verify_results = [] # Array of dictionaries: {target, measured, distance, error_px, error_mm, error_deg}
var verify_sample_buffer = []

# Drawing helpers
var eye_gaze_pos = Vector2.ZERO
var nose_gaze_pos = Vector2.ZERO
var center_pos = Vector2.ZERO
var draw_target = false
var draw_report_vectors = false

func _ready():
	tracker.yunet_model_path = "res://models/face_detection_yunet_2023mar.onnx"
	tracker.gaze_onnx_path = "res://models/gaze-estimation-adas-0002.xml"
	tracker.initialize_tracker()
	
	setup_ui()
	auto_estimate_geometry()
	
	current_state = State.STATE_FREE_PLAY

func setup_ui():
	# Root container for layout: sidebar on left, calibration arena on right
	var main_hbox = HBoxContainer.new()
	main_hbox.anchors_preset = Control.PRESET_FULL_RECT
	main_hbox.anchor_right = 1.0
	main_hbox.anchor_bottom = 1.0
	add_child(main_hbox)
	
	# Create sidebar panel
	sidebar = PanelContainer.new()
	sidebar.custom_minimum_size = Vector2(360, 0)
	main_hbox.add_child(sidebar)
	
	# Scroll container inside sidebar for settings
	var scroll = ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	sidebar.add_child(scroll)
	
	var vbox = VBoxContainer.new()
	vbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	vbox.add_theme_constant_override("separation", 10)
	scroll.add_child(vbox)
	
	# Add padding
	var margin = MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 15)
	margin.add_theme_constant_override("margin_right", 15)
	margin.add_theme_constant_override("margin_top", 15)
	margin.add_theme_constant_override("margin_bottom", 15)
	vbox.add_child(margin)
	
	var content = VBoxContainer.new()
	content.add_theme_constant_override("separation", 12)
	margin.add_child(content)
	
	# Title
	var title = Label.new()
	title.text = "GAZE TRACKER CALIBRATION"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	content.add_child(title)
	
	# Status Label
	status_label = Label.new()
	status_label.text = "Status: Idle"
	status_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	content.add_child(status_label)
	
	content.add_child(HSeparator.new())
	
	# Camera Settings
	var cam_label = Label.new()
	cam_label.text = "Camera Offset (mm relative to center):"
	content.add_child(cam_label)
	
	var offset_hbox = HBoxContainer.new()
	content.add_child(offset_hbox)
	
	config_offset_x = create_spinbox(-500, 500, tracker.camera_offset.x, "X", offset_hbox)
	config_offset_y = create_spinbox(-500, 500, tracker.camera_offset.y, "Y", offset_hbox)
	config_offset_z = create_spinbox(-500, 500, tracker.camera_offset.z, "Z", offset_hbox)
	
	config_offset_x.value_changed.connect(func(v): tracker.camera_offset.x = v)
	config_offset_y.value_changed.connect(func(v): tracker.camera_offset.y = v)
	config_offset_z.value_changed.connect(func(v): tracker.camera_offset.z = v)
	
	var tilt_label = Label.new()
	tilt_label.text = "Camera Tilt Angle (degrees):"
	content.add_child(tilt_label)
	
	config_tilt = create_spinbox(-90, 90, tracker.camera_tilt, "Tilt", content)
	config_tilt.value_changed.connect(func(v): tracker.camera_tilt = v)
	
	content.add_child(HSeparator.new())
	
	# Display Settings
	var disp_label = Label.new()
	disp_label.text = "Screen Physical Size (mm):"
	content.add_child(disp_label)
	
	var size_hbox = HBoxContainer.new()
	content.add_child(size_hbox)
	config_size_w = create_spinbox(50, 2000, tracker.screen_size_mm.x, "W", size_hbox)
	config_size_h = create_spinbox(50, 2000, tracker.screen_size_mm.y, "H", size_hbox)
	
	config_size_w.value_changed.connect(func(v): tracker.screen_size_mm.x = v)
	config_size_h.value_changed.connect(func(v): tracker.screen_size_mm.y = v)
	
	var auto_btn = Button.new()
	auto_btn.text = "Estimate Size from DPI"
	auto_btn.pressed.connect(auto_estimate_geometry)
	content.add_child(auto_btn)
	
	content.add_child(HSeparator.new())
	
	# Calibration controls
	var cal_label = Label.new()
	cal_label.text = "Calibration Settings:"
	content.add_child(cal_label)
	
	mode_option = OptionButton.new()
	mode_option.add_item("3D Spherical Angular", 0)
	mode_option.add_item("2D Screen Pixel-Space", 1)
	content.add_child(mode_option)
	
	var cal_btn = Button.new()
	cal_btn.text = "Run 5-Point Calibration"
	cal_btn.pressed.connect(start_calibration_challenge)
	content.add_child(cal_btn)
	
	var clear_btn = Button.new()
	clear_btn.text = "Clear Calibration"
	clear_btn.pressed.connect(clear_tracker_calibration)
	content.add_child(clear_btn)
	
	# Save/Load
	var io_hbox = HBoxContainer.new()
	content.add_child(io_hbox)
	
	var save_btn = Button.new()
	save_btn.text = "Save Session"
	save_btn.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	save_btn.pressed.connect(save_calibration_session)
	io_hbox.add_child(save_btn)
	
	var load_btn = Button.new()
	load_btn.text = "Load Session"
	load_btn.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	load_btn.pressed.connect(load_calibration_session)
	io_hbox.add_child(load_btn)
	
	content.add_child(HSeparator.new())
	
	# Verification controls
	var ver_btn = Button.new()
	ver_btn.text = "Run Verification Challenge"
	ver_btn.pressed.connect(start_verification_challenge)
	content.add_child(ver_btn)
	
	content.add_child(HSeparator.new())
	
	# Diagnostic Report Label
	var rep_label = Label.new()
	rep_label.text = "Diagnostic Report:"
	content.add_child(rep_label)
	
	report_label = RichTextLabel.new()
	report_label.custom_minimum_size = Vector2(0, 240)
	report_label.bbcode_enabled = true
	report_label.size_flags_vertical = Control.SIZE_EXPAND_FILL
	report_label.text = "[color=gray]No evaluation run yet.[/color]"
	content.add_child(report_label)
	
	# Right side: Calibration arena
	var arena = Control.new()
	arena.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	arena.size_flags_vertical = Control.SIZE_EXPAND_FILL
	main_hbox.add_child(arena)

func create_spinbox(min_val: float, max_val: float, val: float, prefix: String, parent: Node) -> SpinBox:
	var label = Label.new()
	label.text = prefix + ":"
	parent.add_child(label)
	
	var spin = SpinBox.new()
	spin.min_value = min_val
	spin.max_value = max_val
	spin.value = val
	spin.step = 0.5
	spin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	parent.add_child(spin)
	return spin

func auto_estimate_geometry():
	var screen_id = DisplayServer.window_get_current_screen()
	var resolution = DisplayServer.screen_get_size(screen_id)
	var dpi = DisplayServer.screen_get_dpi(screen_id)
	
	# Update screen pixels resolution parameter on the tracker node
	tracker.screen_size_pixels = resolution
	
	# Estimate physical width/height in millimeters from DPI
	var w_mm = 527.0
	var h_mm = 296.0 # Fallbacks
	if dpi > 0:
		w_mm = (resolution.x / float(dpi)) * 25.4
		h_mm = (resolution.y / float(dpi)) * 25.4
		
	# Update inputs
	if config_size_w:
		config_size_w.value = w_mm
	if config_size_h:
		config_size_h.value = h_mm
		
	tracker.screen_size_mm = Vector2(w_mm, h_mm)
	print("Auto Estimated Screen: Resolution %s px, DPI %d, Phys Size %dx%d mm" % [resolution, dpi, w_mm, h_mm])

func clear_tracker_calibration():
	tracker.clear_calibration()
	calib_session = null
	report_label.text = "[color=yellow]Calibration Cleared.[/color]"
	draw_report_vectors = false
	queue_redraw()

func start_calibration_session_inst():
	calib_session = GazeCalibrationSession.new()
	calib_session.clear()

func save_calibration_session():
	if not calib_session or calib_session.get_sample_count() == 0:
		report_label.text = "[color=red]Error: No active session to save. Run calibration first.[/color]"
		return
	var err = ResourceSaver.save(calib_session, "user://calibration_run.tres")
	if err == OK:
		report_label.text = "[color=green]Session saved to user://calibration_run.tres[/color]"
	else:
		report_label.text = "[color=red]Error saving session: %d[/color]" % err

func load_calibration_session():
	if not FileAccess.file_exists("user://calibration_run.tres"):
		report_label.text = "[color=red]Error: No saved session file found.[/color]"
		return
	calib_session = ResourceLoader.load("user://calibration_run.tres")
	if calib_session:
		var use_3d = mode_option.selected == 0
		var resource = calib_session.calculate_calibration(tracker, use_3d)
		tracker.calibration_resource = resource
		report_label.text = "[color=green]Session loaded. Calibration calculated and applied![/color]\nSamples: %d" % calib_session.get_sample_count()
	else:
		report_label.text = "[color=red]Error loading session resource.[/color]"

func start_calibration_challenge():
	if not tracker.is_face_detected():
		report_label.text = "[color=red]Error: Cannot start. No face detected![/color]"
		return
		
	start_calibration_session_inst()
	tracker.clear_calibration() # Clear old calibration first
	current_state = State.STATE_CALIBRATING
	current_target_idx = 0
	target_timer = 0.0
	draw_target = true
	draw_report_vectors = false
	sidebar.visible = false # Hide sidebar during calibration focus

func start_verification_challenge():
	if not tracker.is_face_detected():
		report_label.text = "[color=red]Error: Cannot start. No face detected![/color]"
		return
		
	# Define 5 randomized evaluation positions (inset from edges)
	verify_points = [
		Vector2(0.5, 0.5), # Center
		Vector2(randf_range(0.15, 0.35), randf_range(0.15, 0.35)), # Top-Left quadrant
		Vector2(randf_range(0.65, 0.85), randf_range(0.15, 0.35)), # Top-Right quadrant
		Vector2(randf_range(0.15, 0.35), randf_range(0.65, 0.85)), # Bottom-Left quadrant
		Vector2(randf_range(0.65, 0.85), randf_range(0.65, 0.85))  # Bottom-Right quadrant
	]
	verify_results.clear()
	current_state = State.STATE_VERIFYING
	current_target_idx = 0
	target_timer = 0.0
	draw_target = true
	draw_report_vectors = false
	verify_sample_buffer.clear()
	sidebar.visible = false # Hide sidebar during verification focus

func _process(delta):
	# Status bar reporting
	if tracker.is_face_detected():
		status_label.text = "Status: Face Tracked"
		status_label.add_theme_color_override("font_color", Color.GREEN)
	else:
		status_label.text = "Status: Face Lost"
		status_label.add_theme_color_override("font_color", Color.RED)
		
	# Get real-time gaze position
	center_pos = get_viewport().get_visible_rect().size / 2.0
	
	if tracker.is_face_detected():
		var left_origin = tracker.get_left_eye_center()
		var left_dir = tracker.get_left_eye_gaze_direction()
		var left_pixel = project_ray_to_screen_pixels(left_origin, left_dir)
		
		var right_origin = tracker.get_right_eye_center()
		var right_dir = tracker.get_right_eye_gaze_direction()
		var right_pixel = project_ray_to_screen_pixels(right_origin, right_dir)
		
		var window_pos = DisplayServer.window_get_position()
		
		var lp_win = left_pixel - Vector2(window_pos) if left_pixel != Vector2(-1, -1) else Vector2.ZERO
		var rp_win = right_pixel - Vector2(window_pos) if right_pixel != Vector2(-1, -1) else Vector2.ZERO
		
		if lp_win != Vector2.ZERO and rp_win != Vector2.ZERO:
			eye_gaze_pos = (lp_win + rp_win) * 0.5
		elif lp_win != Vector2.ZERO:
			eye_gaze_pos = lp_win
		elif rp_win != Vector2.ZERO:
			eye_gaze_pos = rp_win
		else:
			eye_gaze_pos = Vector2.ZERO
			
		# Head forward / nose gaze
		var head_transform: Transform3D = tracker.get_head_transform()
		var head_forward: Vector3 = -head_transform.basis.z.normalized()
		var nose_pixel = project_ray_to_screen_pixels(head_transform.origin, head_forward)
		if nose_pixel != Vector2(-1, -1):
			nose_gaze_pos = nose_pixel - Vector2(window_pos)
		else:
			nose_gaze_pos = Vector2.ZERO
			
		# Position visual cursor
		if eye_gaze_pos != Vector2.ZERO:
			cursor.visible = true
			cursor.global_position = eye_gaze_pos - cursor.size / 2.0
		else:
			cursor.visible = false
	else:
		eye_gaze_pos = Vector2.ZERO
		nose_gaze_pos = Vector2.ZERO
		cursor.visible = false
		
	# Challenge State Machine logic
	if current_state == State.STATE_CALIBRATING:
		process_calibration(delta)
	elif current_state == State.STATE_VERIFYING:
		process_verification(delta)
		
	queue_redraw()

func process_calibration(delta):
	target_timer += delta
	var viewport_size = get_viewport().get_visible_rect().size
	var target_norm = calib_points[current_target_idx]
	# Account for sidebar width offset (calibration happens in the right arena)
	var arena_origin = Vector2(sidebar.size.x, 0)
	var arena_size = Vector2(viewport_size.x - sidebar.size.x, viewport_size.y)
	var target_window_pos = arena_origin + target_norm * arena_size
	
	current_target_screen_pos = target_window_pos + Vector2(DisplayServer.window_get_position())
	
	# If time elapsed, collect sample and advance
	if target_timer >= target_hold_time:
		if tracker.is_face_detected():
			var left_orig = tracker.get_left_eye_center()
			var left_dir = tracker.get_left_eye_gaze_direction()
			var right_orig = tracker.get_right_eye_center()
			var right_dir = tracker.get_right_eye_gaze_direction()
			
			calib_session.add_sample(current_target_screen_pos, left_orig, left_dir, right_orig, right_dir)
			
			# Print/Log raw-ish instrumentation data for unit test creation
			print("--- CALIBRATION SAMPLE POINT %d ---" % current_target_idx)
			print("Target Pixel Screen: Vector2(%f, %f)" % [current_target_screen_pos.x, current_target_screen_pos.y])
			print("Raw Head Rotation (Pitch, Yaw, Roll deg): Vector3(%f, %f, %f)" % [tracker.get_raw_head_rotation().x, tracker.get_raw_head_rotation().y, tracker.get_raw_head_rotation().z])
			print("Raw Head Translation (X, Y, Z mm): Vector3(%f, %f, %f)" % [tracker.get_raw_head_translation().x, tracker.get_raw_head_translation().y, tracker.get_raw_head_translation().z])
			print("Raw Left Eye Center (X, Y, Z mm): Vector3(%f, %f, %f)" % [tracker.get_raw_left_eye_center().x, tracker.get_raw_left_eye_center().y, tracker.get_raw_left_eye_center().z])
			print("Raw Right Eye Center (X, Y, Z mm): Vector3(%f, %f, %f)" % [tracker.get_raw_right_eye_center().x, tracker.get_raw_right_eye_center().y, tracker.get_raw_right_eye_center().z])
			print("Raw Gaze Direction (X, Y, Z): Vector3(%f, %f, %f)" % [tracker.get_raw_gaze_direction().x, tracker.get_raw_gaze_direction().y, tracker.get_raw_gaze_direction().z])
			print("Godot Head Transform: ", tracker.get_head_transform())
			print("Godot Left Eye Center: ", tracker.get_left_eye_center())
			print("Godot Right Eye Center: ", tracker.get_right_eye_center())
			print("Godot Left Eye Direction: ", tracker.get_left_eye_gaze_direction())
			print("Godot Right Eye Direction: ", tracker.get_right_eye_gaze_direction())
			print("---------------------------------")
			
		current_target_idx += 1
		target_timer = 0.0
		if current_target_idx >= calib_points.size():
			complete_calibration()

func complete_calibration():
	current_state = State.STATE_FREE_PLAY
	draw_target = false
	sidebar.visible = true
	
	var use_3d = mode_option.selected == 0
	var resource = calib_session.calculate_calibration(tracker, use_3d)
	tracker.calibration_resource = resource
	
	report_label.text = "[color=green]Calibration Complete![/color]\nApplied averaged offsets successfully.\nSamples: %d" % calib_session.get_sample_count()

func process_verification(delta):
	target_timer += delta
	var viewport_size = get_viewport().get_visible_rect().size
	var target_norm = verify_points[current_target_idx]
	var arena_origin = Vector2(sidebar.size.x, 0)
	var arena_size = Vector2(viewport_size.x - sidebar.size.x, viewport_size.y)
	var target_window_pos = arena_origin + target_norm * arena_size
	
	current_target_screen_pos = target_window_pos + Vector2(DisplayServer.window_get_position())
	
	# Accumulate tracking coordinates during the last 700ms of the target gaze (to skip adjustment delay)
	if target_timer > 0.8:
		if tracker.is_face_detected() and eye_gaze_pos != Vector2.ZERO:
			# Store the screen coordinates of both target and measured gaze
			var gaze_screen = eye_gaze_pos + Vector2(DisplayServer.window_get_position())
			verify_sample_buffer.append(gaze_screen)
			
	if target_timer >= target_hold_time:
		# Average coordinates in buffer
		var avg_measured = Vector2.ZERO
		if verify_sample_buffer.size() > 0:
			for sample in verify_sample_buffer:
				avg_measured += sample
			avg_measured /= float(verify_sample_buffer.size())
		else:
			avg_measured = eye_gaze_pos + Vector2(DisplayServer.window_get_position())
			
		var distance = tracker.get_left_eye_center().z # user distance in mm
		if distance <= 0.0:
			distance = 600.0 # fallback
			
		var error_px = current_target_screen_pos.distance_to(avg_measured)
		
		# Map pixel error to mm error
		var px_size = tracker.screen_size_pixels
		var mm_size = tracker.screen_size_mm
		var err_mm = Vector2(
			(current_target_screen_pos.x - avg_measured.x) * (mm_size.x / px_size.x),
			(current_target_screen_pos.y - avg_measured.y) * (mm_size.y / px_size.y)
		).length()
		
		# Angle: degrees = atan(mm_error / distance) * 180 / PI
		var err_deg = rad_to_deg(atan(err_mm / distance))
		
		verify_results.append({
			"target": current_target_screen_pos,
			"measured": avg_measured,
			"distance": distance,
			"error_px": error_px,
			"error_mm": err_mm,
			"error_deg": err_deg
		})
		
		current_target_idx += 1
		target_timer = 0.0
		verify_sample_buffer.clear()
		if current_target_idx >= verify_points.size():
			complete_verification()

func complete_verification():
	current_state = State.STATE_FREE_PLAY
	draw_target = false
	sidebar.visible = true
	draw_report_vectors = true
	
	# Compute metrics
	var total_px = 0.0
	var total_mm = 0.0
	var total_deg = 0.0
	var bias_x = 0.0
	var bias_y = 0.0
	
	var center_err_deg = 0.0
	var corners_err_deg = 0.0
	
	for i in range(verify_results.size()):
		var res = verify_results[i]
		total_px += res.error_px
		total_mm += res.error_mm
		total_deg += res.error_deg
		
		# Drift vectors (Target - Measured)
		bias_x += (res.target.x - res.measured.x)
		bias_y += (res.target.y - res.measured.y)
		
		if i == 0:
			center_err_deg = res.error_deg
		else:
			corners_err_deg += res.error_deg
			
	corners_err_deg /= 4.0 # 4 corners
	var n = float(verify_results.size())
	var mean_px = total_px / n
	var mean_mm = total_mm / n
	var mean_deg = total_deg / n
	bias_x /= n
	bias_y /= n
	
	# Standard deviation (precision)
	var variance_deg = 0.0
	for res in verify_results:
		variance_deg += pow(res.error_deg - mean_deg, 2)
	var std_dev_deg = sqrt(variance_deg / n)
	
	# Build report text
	var text = "[b][color=green]EVALUATION REPORT[/color][/b]\n"
	text += "Accuracy (Mean Error):\n"
	text += "  - [b]%.2f°[/b] of visual angle\n" % mean_deg
	text += "  - [b]%.1f mm[/b] on screen\n" % mean_mm
	text += "  - [b]%.1f px[/b] resolution\n" % mean_px
	text += "Precision (Std Dev): [b]%.2f°[/b]\n" % std_dev_deg
	text += "Systematic Bias:\n"
	text += "  - Horizontal: [b]%.1f px[/b] (%s)\n" % [abs(bias_x), "Right" if bias_x > 0 else "Left"]
	text += "  - Vertical: [b]%.1f px[/b] (%s)\n" % [abs(bias_y), "Down" if bias_y > 0 else "Up"]
	
	# Analyze geometry
	text += "\nGeometry Analysis:\n"
	text += "  - Center Accuracy: [b]%.2f°[/b]\n" % center_err_deg
	text += "  - Corners Accuracy: [b]%.2f°[/b]\n" % corners_err_deg
	
	if center_err_deg < 2.0 and corners_err_deg / center_err_deg > 2.0 and mean_deg > 2.0:
		text += "\n[color=orange][b]WARNING: Corner distortion detected![/b]\n"
		text += "Your corner errors significantly exceed center errors. "
		text += "This indicates that screen physical size (mm) or camera offsets (XYZ) are incorrect. "
		text += "Please double-check and configure screen size and camera mount offsets.[/color]\n"
	elif mean_deg < 1.5:
		text += "\n[color=light_green][b]EXCELLENT ACCURACY![/b]\nTracking meets scientific standards (< 1.5°).[/color]\n"
	else:
		text += "\n[color=yellow][b]Acceptable Tracking.[/b]\nConsider running calibration again or adjusting room lighting.[/color]\n"
		
	report_label.text = text

func _draw():
	if current_state == State.STATE_FREE_PLAY:
		# Draw center point reference
		draw_circle(center_pos, 6, Color.WHITE)
		
		# Draw lines from center to eyes/nose
		if eye_gaze_pos != Vector2.ZERO:
			draw_line(center_pos, eye_gaze_pos, Color.GREEN, 2.0)
			draw_circle(eye_gaze_pos, 8, Color.GREEN)
		if nose_gaze_pos != Vector2.ZERO:
			draw_line(center_pos, nose_gaze_pos, Color.CYAN, 2.0)
			draw_circle(nose_gaze_pos, 8, Color.CYAN)
			
	elif draw_target:
		# Draw active challenge target
		var viewport_size = get_viewport().get_visible_rect().size
		var target_norm = calib_points[current_target_idx] if current_state == State.STATE_CALIBRATING else verify_points[current_target_idx]
		
		var arena_origin = Vector2(sidebar.size.x, 0)
		var arena_size = Vector2(viewport_size.x - sidebar.size.x, viewport_size.y)
		var target_win = arena_origin + target_norm * arena_size
		
		# Draw target dot
		draw_circle(target_win, 12, Color.RED)
		draw_circle(target_win, 4, Color.WHITE)
		
		# Draw contracting progress circle
		var progress = target_timer / target_hold_time
		var radius = lerp(45.0, 15.0, progress)
		draw_arc(target_win, radius, 0, TAU, 32, Color.YELLOW, 2.0)
		
	if draw_report_vectors:
		# Draw error vectors from verification run
		var window_pos = DisplayServer.window_get_position()
		for res in verify_results:
			var target_win = res.target - Vector2(window_pos)
			var measured_win = res.measured - Vector2(window_pos)
			
			# Draw target (red) and measured (cyan) dots
			draw_circle(target_win, 6, Color.RED)
			draw_circle(measured_win, 6, Color.CYAN)
			# Line from target to measured (orange)
			draw_line(target_win, measured_win, Color.ORANGE, 2.0)

func project_ray_to_screen_pixels(origin_cam: Vector3, dir_cam: Vector3) -> Vector2:
	var transform: Transform3D = tracker.get_camera_to_screen_transform()
	var origin_scr = transform * origin_cam
	var dir_scr = transform.basis * dir_cam # direction is rotated only
	
	if abs(dir_scr.z) < 1e-6:
		return Vector2(-1, -1)
		
	var t = -origin_scr.z / dir_scr.z
	if t < 0:
		return Vector2(-1, -1)
		
	var intersection_mm = Vector2(origin_scr.x + t * dir_scr.x, origin_scr.y + t * dir_scr.y)
	
	var px_size = tracker.screen_size_pixels
	var mm_size = tracker.screen_size_mm
	
	if mm_size.x <= 0 or mm_size.y <= 0:
		return Vector2(-1, -1)
		
	var px_x = px_size.x / 2.0 + intersection_mm.x * (px_size.x / mm_size.x)
	var px_y = px_size.y / 2.0 - intersection_mm.y * (px_size.y / mm_size.y)
	return Vector2(px_x, px_y)
