# Testing toy that draws a ray from center screen to the projected head pose ("nose gaze") and eye gaze.
extends Control

@onready var cursor = $Cursor
@onready var status_label = $StatusLabel

var latest_gaze_event: InputEventGazeBase = null
var eye_gaze_pos: Vector2 = Vector2.ZERO
var nose_gaze_pos: Vector2 = Vector2.ZERO
var center_pos: Vector2 = Vector2.ZERO
var coords_label: Label
var is_maximized: bool = false: set=_set_maximized

func _ready():
	# Center the window on start
	var screen_id = DisplayServer.window_get_current_screen()
	var screen_size = DisplayServer.screen_get_size(screen_id)
	var window_size = DisplayServer.window_get_size()
	var win_pos = (screen_size - window_size) / 2
	DisplayServer.window_set_position(win_pos)

	var gs = Engine.get_singleton("GazeServer")
	if gs:
		gs.display_set_window_parameters(gs.get_default_display_rid(), win_pos, get_viewport().get_final_transform())
		gs.start_tracking()

	# Create a coordinate feedback label near screen center
	coords_label = Label.new()
	add_child(coords_label)
	coords_label.text = ""

	if "--run-automated-toggles" in OS.get_cmdline_args():
		_run_automated_toggles()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventGazeBase:
		latest_gaze_event = event
		if event is InputEventGaze:
			eye_gaze_pos = event.position
			
			# Project head pose ray ("nose gaze") to window pixel coordinates using calibrated geometry
			var xform = event.head_transform
			var nose_orig = xform.origin
			var nose_fwd = -xform.basis.z.normalized()
			
			var gs = Engine.get_singleton("GazeServer")
			var dp: DisplayProfile = gs.get_display_profile() if gs else null
			if not dp:
				dp = DisplayProfile.new()
				dp.estimate_from_os()
			var dev_cal: DeviceCalibration = gs.get_device_calibration() if gs else null
			
			var phys = dp.physical_size_mm
			var log_sz = dp.logical_size_px
			var scale_x = log_sz.x / phys.x if phys.x > 0 else 1.0
			var scale_y = log_sz.y / phys.y if phys.y > 0 else 1.0
			var cam_offset = dev_cal.get_camera_offset(null) if dev_cal else Vector3(0, phys.y * 0.5, 0)
			var cam_tilt = dev_cal.get_camera_tilt(null) if dev_cal else 0.0
			
			var theta_rad = deg_to_rad(cam_tilt)
			var cos_t = cos(theta_rad)
			var sin_t = sin(theta_rad)
			
			var o_disp_z = sin_t * nose_orig.y - cos_t * nose_orig.z + cam_offset.z
			var v_disp_z = sin_t * nose_fwd.y - cos_t * nose_fwd.z
			
			if abs(v_disp_z) > 1e-6:
				var t = -o_disp_z / v_disp_z
				if t >= 0.0:
					var w_half = phys.x * 0.5
					var h_half = phys.y * 0.5
					var o_disp_x = w_half - (nose_orig.x + cam_offset.x)
					var o_disp_y = -(cos_t * nose_orig.y + sin_t * nose_orig.z + cam_offset.y) + h_half
					var v_disp_x = -nose_fwd.x
					var v_disp_y = -(cos_t * nose_fwd.y + sin_t * nose_fwd.z)
					var hit_mm_x = o_disp_x + v_disp_x * t
					var hit_mm_y = o_disp_y + v_disp_y * t
					var screen_px = Vector2(hit_mm_x * scale_x, hit_mm_y * scale_y)
					nose_gaze_pos = screen_px - Vector2(DisplayServer.window_get_position())

			if is_instance_valid(cursor):
				cursor.visible = true
				cursor.color = Color.GREEN
				cursor.global_position = eye_gaze_pos - cursor.size / 2.0
			if is_instance_valid(status_label):
				status_label.text = "Status: Face Tracked (Openness L: %.2f, R: %.2f)" % [event.left_eye_openness, event.right_eye_openness]
		elif event is InputEventGazeMissing:
			eye_gaze_pos = Vector2.ZERO
			nose_gaze_pos = Vector2.ZERO
			if is_instance_valid(cursor):
				cursor.visible = false
				cursor.color = Color.RED
			if is_instance_valid(status_label):
				status_label.text = "Status: Face Lost (Reason: %d)" % event.reason

func _process(_delta):
	center_pos = get_viewport().get_visible_rect().size / 2.0
	
	if latest_gaze_event is InputEventGaze and eye_gaze_pos != Vector2.ZERO:
		var gaze_str = "(%d, %d)" % [int(eye_gaze_pos.x), int(eye_gaze_pos.y)]
		var nose_str = "(%d, %d)" % [int(nose_gaze_pos.x), int(nose_gaze_pos.y)]
		coords_label.text = "Eye Gaze: %s\nNose Gaze: %s" % [gaze_str, nose_str]
		coords_label.global_position = center_pos + Vector2(-80, 40)
	else:
		coords_label.text = ""
		
	queue_redraw()
 
func _draw():
	# Draw center point reference
	draw_circle(center_pos, 6, Color.WHITE)
	
	# Draw line from center to nose gaze (cyan)
	if nose_gaze_pos != Vector2.ZERO:
		draw_line(center_pos, nose_gaze_pos, Color.CYAN, 2.0)
		draw_circle(nose_gaze_pos, 6, Color.CYAN)

	# Draw line from center to eye gaze (bright green)
	if eye_gaze_pos != Vector2.ZERO:
		draw_line(center_pos, eye_gaze_pos, Color.GREEN, 2.0)
		draw_circle(eye_gaze_pos, 8, Color.GREEN)

func _set_maximized(v: bool) -> void:
	print("Setting maximized: ", v)
	is_maximized = v
	if status_label: status_label.text = "Status: Full screen" if is_maximized else "Status: Windowed"
	var gs = Engine.get_singleton("GazeServer")
	if gs:
		gs.display_set_window_parameters(gs.get_default_display_rid(), DisplayServer.window_get_position(), get_viewport().get_final_transform())

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
