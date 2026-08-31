# Clean, simple end-user 5-point calibration scene.
extends Control

signal calibration_completed(resource)

@export var target_hold_time: float = 1.5

var calib_session: GazeCalibrationSession
var latest_gaze_event: InputEventGaze = null
var calib_points = [
	Vector2(0.5, 0.5),   # Center
	Vector2(0.1, 0.1),   # Top Left
	Vector2(0.9, 0.1),   # Top Right
	Vector2(0.1, 0.9),   # Bottom Left
	Vector2(0.9, 0.9)    # Bottom Right
]

var current_target_idx = 0
var target_timer = 0.0
var current_target_screen_pos = Vector2.ZERO
var draw_target = true

func _ready():
	# Center the window on start
	var screen_id = DisplayServer.window_get_current_screen()
	var screen_size = DisplayServer.screen_get_size(screen_id)
	var window_size = DisplayServer.window_get_size()
	DisplayServer.window_set_position((screen_size - window_size) / 2)

	# Ensure Control layout fills screen
	anchors_preset = Control.PRESET_FULL_RECT
	anchor_right = 1.0
	anchor_bottom = 1.0
	
	var gs = Engine.get_singleton("GazeServer")
	if gs:
		gs.start_tracking()
		gs.set_device_calibration(null)
		gs.set_bio_calibration(null)
		
	calib_session = GazeCalibrationSession.new()
	calib_session.clear()
	current_target_idx = 0
	target_timer = 0.0
	draw_target = true

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventGaze:
		latest_gaze_event = event

func _process(delta):
	if current_target_idx >= calib_points.size():
		return
		
	target_timer += delta
	var viewport_size = get_viewport().get_visible_rect().size
	var target_norm = calib_points[current_target_idx]
	var target_window_pos = target_norm * viewport_size
	
	var win_pos = get_window().position if get_window() else Vector2i.ZERO
	current_target_screen_pos = Vector2(win_pos) + target_window_pos
	
	if target_timer >= target_hold_time:
		if latest_gaze_event != null and latest_gaze_event.is_face_tracked():
			var gaze_orig = latest_gaze_event.gaze_transform.origin
			var gaze_dir = latest_gaze_event.gaze_transform.basis.z * -1.0
			calib_session.add_sample(current_target_screen_pos, gaze_orig, gaze_dir)
			
		current_target_idx += 1
		target_timer = 0.0
		
		if current_target_idx >= calib_points.size():
			complete_calibration()
			
	queue_redraw()

func complete_calibration():
	draw_target = false
	var success = calib_session.calculate_calibration(null)
	var dev_cal = calib_session.get_device_calibration()
	var bio_cal = calib_session.get_bio_calibration()
	var gs = Engine.get_singleton("GazeServer")
	if gs and success:
		if dev_cal:
			gs.set_device_calibration(dev_cal)
		if bio_cal:
			gs.set_bio_calibration(bio_cal)
	var res_dict = {
		"success": success,
		"device_calibration": dev_cal,
		"bio_calibration": bio_cal
	}
	calibration_completed.emit(res_dict)

func _draw():
	if not draw_target or current_target_idx >= calib_points.size():
		return
		
	var viewport_size = get_viewport().get_visible_rect().size
	var target_norm = calib_points[current_target_idx]
	var target_win = target_norm * viewport_size
	
	# Draw target dot
	draw_circle(target_win, 12, Color.RED)
	draw_circle(target_win, 4, Color.WHITE)
	
	# Draw contracting progress circle
	var progress = target_timer / target_hold_time
	var radius = lerp(45.0, 15.0, progress)
	draw_arc(target_win, radius, 0, TAU, 32, Color.YELLOW, 2.0)
