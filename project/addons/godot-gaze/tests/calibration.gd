# Clean, simple end-user 5-point calibration scene.
extends Control

signal calibration_completed(resource)

@export var target_hold_time: float = 1.5

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
var target_errors: Array[float] = []

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
		
	current_target_idx = 0
	target_timer = 0.0
	draw_target = true
	target_errors.clear()

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
			var gaze_pos = latest_gaze_event.position
			var err = gaze_pos.distance_to(target_window_pos)
			target_errors.append(err)
			
		current_target_idx += 1
		target_timer = 0.0
		
		if current_target_idx >= calib_points.size():
			complete_calibration()
			
	queue_redraw()

func complete_calibration():
	draw_target = false
	var gs = Engine.get_singleton("GazeServer")
	var profile = gs.get_device_profile() if gs else null
	var avg_err = 0.0
	if target_errors.size() > 0:
		for e in target_errors:
			avg_err += e
		avg_err /= float(target_errors.size())
	var res_dict = {
		"success": true,
		"device_profile": profile,
		"average_error_px": avg_err
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
