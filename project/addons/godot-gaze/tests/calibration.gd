# Clean, simple end-user 5-point calibration scene.
extends Control

signal calibration_completed(resource)

@export var target_hold_time: float = 1.5

var tracker: GazeTracker = null
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
	
	if not tracker:
		# Fallback if tracker not injected by parent
		var child_tracker = get_node_or_null("GazeTracker")
		if child_tracker:
			tracker = child_tracker
		else:
			tracker = GazeTracker.new()
			add_child(tracker)
			tracker.initialize_tracker()
		
	tracker.clear_calibration()
	calib_session = GazeCalibrationSession.new()
	calib_session.clear()
	current_target_idx = 0
	target_timer = 0.0
	draw_target = true

func _process(delta):
	if current_target_idx >= calib_points.size():
		return
		
	target_timer += delta
	var viewport_size = get_viewport().get_visible_rect().size
	var target_norm = calib_points[current_target_idx]
	var target_window_pos = target_norm * viewport_size
	
	current_target_screen_pos = tracker.map_logical_to_physical_screen(target_window_pos)
	
	if target_timer >= target_hold_time:
		if tracker.is_face_detected():
			var gaze_orig = tracker.get_gaze_origin()
			var gaze_dir = tracker.get_gaze_direction(false)
			calib_session.add_sample(current_target_screen_pos, gaze_orig, gaze_dir)
			
		current_target_idx += 1
		target_timer = 0.0
		
		if current_target_idx >= calib_points.size():
			complete_calibration()
			
	queue_redraw()

func complete_calibration():
	draw_target = false
	var resource = calib_session.calculate_calibration(tracker)
	tracker.calibration_resource = resource
	calibration_completed.emit(resource)

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
