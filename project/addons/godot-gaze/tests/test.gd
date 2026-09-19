# Testing toy that draws a ray from center screen to the projected head pose ("nose gaze") and eye gaze.
extends Control

@onready var cursor = $Cursor
@onready var status_label = $StatusLabel

var latest_gaze_event: InputEventGazeBase = null
var eye_gaze_pos: Vector2 = Vector2.ZERO
var raw_eye_gaze_pos: Vector2 = Vector2.ZERO
var nose_gaze_pos: Vector2 = Vector2.ZERO
var center_pos: Vector2 = Vector2.ZERO
var coords_label: Label
var is_maximized: bool = false: set=_set_maximized

const GazeContinuousCalibratorScript = preload("res://addons/godot-gaze/calibration/gaze_continuous_calibrator.gd")

var continuous_calibrator = null
var calib_status_message: String = ""

func _ready():
	# Center the window on start
	var screen_id = DisplayServer.window_get_current_screen()
	var screen_size = DisplayServer.screen_get_size(screen_id)
	var window_size = DisplayServer.window_get_size()
	var win_pos = (screen_size - window_size) / 2
	DisplayServer.window_set_position(win_pos)

	var gs = Engine.get_singleton("GazeServer")
	if gs:
		gs.start_tracking()

	# Create a coordinate feedback label near screen center
	coords_label = Label.new()
	add_child(coords_label)
	coords_label.text = ""

	continuous_calibrator = GazeContinuousCalibratorScript.new()
	add_child(continuous_calibrator)
	continuous_calibrator.calibration_finished.connect(_on_calib_finished)
	continuous_calibrator.calibration_converged.connect(_on_calib_converged)
	continuous_calibrator.calibration_canceled.connect(_on_calib_canceled)

	if "--run-automated-toggles" in OS.get_cmdline_args() or "--run-automated-toggles" in OS.get_cmdline_user_args():
		_run_automated_toggles()

func _on_calib_finished(bio: GazeBioProfile) -> void:
	if bio:
		calib_status_message = "Calibration Complete! Yaw: %+.2f° | Pitch: %+.2f°" % [bio.bias_yaw_deg, bio.bias_pitch_deg]
	else:
		calib_status_message = "Calibration finished."
	print("[Calibration] " + calib_status_message)

func _on_calib_converged(bio: GazeBioProfile) -> void:
	print("[Calibration] Converged! Yaw: %+.2f° | Pitch: %+.2f°" % [bio.bias_yaw_deg, bio.bias_pitch_deg])

func _on_calib_canceled() -> void:
	calib_status_message = "Calibration canceled."
	print("[Calibration] " + calib_status_message)

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.is_echo():
		if continuous_calibrator and continuous_calibrator.is_calibrating:
			if event.keycode == KEY_SPACE:
				continuous_calibrator.force_capture()
				return
			elif event.keycode == KEY_ESCAPE:
				continuous_calibrator.finish_calibration()
				return

		if event.keycode == KEY_C:
			if event.shift_pressed:
				# Shift + C: Instant 1-point centering calibration on screen center
				if latest_gaze_event is InputEventGaze:
					var cal = GazeCalibration.new()
					cal.add_event(latest_gaze_event)
					var bio = cal.install()
					if bio:
						calib_status_message = "1-Point Centered! Yaw bias: %.2f deg, Pitch bias: %.2f deg" % [bio.bias_yaw_deg, bio.bias_pitch_deg]
						print("[Calibration] " + calib_status_message)
					else:
						calib_status_message = "1-Point Centering failed!"
			else:
				# Key C: Start continuous random dwell calibration
				if continuous_calibrator:
					continuous_calibrator.start_calibration()
					calib_status_message = "Continuous dwell calibration active. Look at dot to dwell."
					print("[Calibration] " + calib_status_message)
		elif event.keycode == KEY_R:
			var bio = GazeBioProfile.new()
			bio.bias_yaw_deg = 0.0
			bio.bias_pitch_deg = 0.0
			var gs = Engine.get_singleton("GazeServer")
			if gs:
				gs.set_bio_profile(bio)
			bio.save_to_file("user://calibrations/bio_profile.cfg")
			calib_status_message = "Reset bio profile to default (0 bias)."
			print("[Calibration] " + calib_status_message)

	if event is InputEventGazeBase:
		latest_gaze_event = event
		if event is InputEventGaze:
			eye_gaze_pos = event.get_eye_gaze()
			raw_eye_gaze_pos = event.get_raw_eye_gaze()
			nose_gaze_pos = event.get_nose_gaze()

			if is_instance_valid(cursor):
				cursor.visible = true
				cursor.color = Color.GREEN
				cursor.global_position = eye_gaze_pos - cursor.size / 2.0
			if is_instance_valid(status_label):
				if continuous_calibrator and continuous_calibrator.is_calibrating:
					status_label.text = "CALIBRATION: Look at dot to dwell | [SPACE] Force | [ESC] Finish"
				else:
					status_label.text = "Status: Face Tracked (Openness L: %.2f, R: %.2f) | [C] Continuous Dwell Calib, [Shift+C] Center, [R] Reset" % [event.left_eye_openness, event.right_eye_openness]
		elif event is InputEventGazeMissing:
			eye_gaze_pos = Vector2.ZERO
			raw_eye_gaze_pos = Vector2.ZERO
			nose_gaze_pos = Vector2.ZERO
			if is_instance_valid(cursor):
				cursor.visible = false
				cursor.color = Color.RED
			if is_instance_valid(status_label):
				status_label.text = "Status: Face Lost (Reason: %d)" % event.reason

func _process(_delta):
	center_pos = get_viewport().get_visible_rect().size / 2.0

	if continuous_calibrator and continuous_calibrator.is_calibrating:
		coords_label.text = ""
	elif latest_gaze_event is InputEventGaze and eye_gaze_pos != Vector2.ZERO:
		var gaze_str = "(%d, %d)" % [int(eye_gaze_pos.x), int(eye_gaze_pos.y)]
		var raw_str = "(%d, %d)" % [int(raw_eye_gaze_pos.x), int(raw_eye_gaze_pos.y)]
		var diff = eye_gaze_pos - raw_eye_gaze_pos
		var diff_str = "Δ: (%.1f, %.1f) px" % [diff.x, diff.y]
		var nose_str = "(%d, %d)" % [int(nose_gaze_pos.x), int(nose_gaze_pos.y)]
		var status_line = calib_status_message if not calib_status_message.is_empty() else "[C] Continuous Dwell Calib | [Shift+C] Center | [R] Reset"
		coords_label.text = "Eye Gaze (Calibrated): %s\nEye Gaze (Raw): %s (%s)\nNose Gaze: %s\n%s" % [gaze_str, raw_str, diff_str, nose_str, status_line]
		coords_label.global_position = center_pos + Vector2(-150, 40)
	else:
		coords_label.text = calib_status_message

	queue_redraw()

func _draw():
	# Draw center point reference
	draw_circle(center_pos, 6, Color.WHITE)

	# Draw line from center to nose gaze (cyan)
	if nose_gaze_pos != Vector2.ZERO:
		draw_line(center_pos, nose_gaze_pos, Color.CYAN, 2.0)
		draw_circle(nose_gaze_pos, 6, Color.CYAN)

	# Draw uncalibrated (raw) eye gaze (matching green with 50% alpha)
	var raw_color = Color(0.0, 1.0, 0.0, 0.5)
	if raw_eye_gaze_pos != Vector2.ZERO:
		draw_line(center_pos, raw_eye_gaze_pos, raw_color, 2.0)
		draw_circle(raw_eye_gaze_pos, 7.0, raw_color)
		draw_arc(raw_eye_gaze_pos, 12.0, 0, TAU, 32, raw_color, 1.5)

	# Draw calibrated eye gaze (solid green)
	if eye_gaze_pos != Vector2.ZERO:
		draw_line(center_pos, eye_gaze_pos, Color.GREEN, 2.0)
		draw_circle(eye_gaze_pos, 8.0, Color.GREEN)

	# If both are visible, draw connecting line showing calibration delta
	if raw_eye_gaze_pos != Vector2.ZERO and eye_gaze_pos != Vector2.ZERO:
		draw_line(raw_eye_gaze_pos, eye_gaze_pos, Color(1.0, 1.0, 0.0, 0.6), 1.5)

func _set_maximized(v: bool) -> void:
	print("Setting maximized: ", v)
	is_maximized = v
	if status_label: status_label.text = "Status: Full screen" if is_maximized else "Status: Windowed"

func _run_automated_toggles():
	print("=================== STARTING AUTOMATED SCENE TOGGLES ===================")
	for i in range(10):
		print("Automated iteration ", i)
		var ksm = get_node_or_null("DebugCamFeedControl")
		if ksm:
			var event = InputEventKey.new()
			event.keycode = KEY_D
			event.pressed = true
			ksm._unhandled_input(event)
		await get_tree().create_timer(0.1).timeout

		var fs = get_node_or_null("FullScreener")
		if fs:
			var event = InputEventKey.new()
			event.keycode = KEY_F
			event.pressed = true
			fs._input(event)
		await get_tree().create_timer(0.1).timeout

		if ksm:
			var event = InputEventKey.new()
			event.keycode = KEY_D
			event.pressed = true
			ksm._unhandled_input(event)
		await get_tree().create_timer(0.1).timeout

		if fs:
			var event = InputEventKey.new()
			event.keycode = KEY_F
			event.pressed = true
			fs._input(event)
		await get_tree().create_timer(0.1).timeout

	print("Automated toggles finished, calling get_tree().quit(0)...")
	get_tree().quit(0)
