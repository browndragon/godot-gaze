# Testing toy that draws a ray from center screen to the projected head pose ("nose gaze") and eye gaze.
extends Control

var tracker: GazeTracker = null
@onready var cursor = $Cursor
@onready var status_label = $StatusLabel

var left_eye_pos: Vector2 = Vector2.ZERO
var right_eye_pos: Vector2 = Vector2.ZERO
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
	DisplayServer.window_set_position((screen_size - window_size) / 2)

	if not tracker:
		tracker = get_node_or_null("GazeTracker")
		if not tracker:
			tracker = GazeTracker.new()
			add_child(tracker)
			tracker.initialize_tracker()

	# Connect to GDExtension signals
	tracker.gaze_updated.connect(_on_gaze_updated)
	tracker.face_detection_changed.connect(_on_face_detected)
	tracker.lifecycle_changed.connect(_on_lifecycle_changed)
	
	# Sync status with current state (since tracker is persistent and might already be running)
	_on_lifecycle_changed(tracker.get_lifecycle_state())
	if tracker.is_face_detected():
		_on_face_detected(true)
	
	# Create a coordinate feedback label near screen center
	coords_label = Label.new()
	add_child(coords_label)
	coords_label.text = ""

	if "--run-automated-toggles" in OS.get_cmdline_args():
		_run_automated_toggles()

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

func _process(_delta):
	if Engine.get_frames_drawn() % 60 == 0:
		print("==================== GAZE DEBUG ====================")
		print("Face Tracked: ", tracker.is_face_detected())
		if tracker.is_face_detected():
			print("Gaze Origin: ", tracker.get_gaze_origin(), " Gaze Dir: ", tracker.get_gaze_direction())
			print("Head Transform:\n", tracker.get_head_transform())
			print("Eye Gaze Pos: ", eye_gaze_pos, " Nose Gaze Pos: ", nose_gaze_pos)
			print("Camera-to-Screen Transform:\n", tracker.get_camera_to_screen_transform())
		print("====================================================")

	if Input.is_key_pressed(KEY_SPACE) and tracker.debug_logging_frames <= 0:
		print("[GDScript] Spacebar pressed, requesting 5-frame telemetry burst...")
		tracker.debug_logging_frames = 5

	center_pos = get_viewport().get_visible_rect().size / 2.0
	
	if tracker.is_face_detected():
		# 1. Head Pose / Nose Gaze projection
		var head_forward = tracker.get_head_forward()
		if tracker.debug_logging_frames != 0:
			print("[GazeTracker Debug] -- Starting Nose Pose --")
		var nose_pixel = tracker.project_gaze_ray_to_viewport(tracker.get_head_position(), head_forward)
		if nose_pixel != Vector2.INF:
			nose_gaze_pos = nose_pixel
		else:
			nose_gaze_pos = Vector2.ZERO
			
		# 2. Unified Eye Gaze projection
		eye_gaze_pos = tracker.get_latest_filtered_gaze()
	else:
		eye_gaze_pos = Vector2.ZERO
		nose_gaze_pos = Vector2.ZERO
		
	# Draw cursor and coordinates
	if eye_gaze_pos != Vector2.ZERO:
		cursor.visible = true
		cursor.global_position = eye_gaze_pos - cursor.size / 2.0
		coords_label.text = "Gaze: (%d, %d)\nNose: (%d, %d)" % [
			int(eye_gaze_pos.x), int(eye_gaze_pos.y),
			int(nose_gaze_pos.x), int(nose_gaze_pos.y)
		]
		coords_label.global_position = center_pos + Vector2(-80, 40)
	else:
		cursor.visible = false
		coords_label.text = ""
		
	queue_redraw()
 
func _draw():
	# Draw center point reference
	draw_circle(center_pos, 6, Color.WHITE)
	
	# Draw line from center to eye gaze (green)
	if eye_gaze_pos != Vector2.ZERO:
		draw_line(center_pos, eye_gaze_pos, Color.GREEN, 2.0)
		draw_circle(eye_gaze_pos, 8, Color.GREEN)
		
	# Draw line from center to nose gaze (cyan)
	if nose_gaze_pos != Vector2.ZERO:
		draw_line(center_pos, nose_gaze_pos, Color.CYAN, 2.0)
		draw_circle(nose_gaze_pos, 8, Color.CYAN)
 
func _on_gaze_updated(_pixel: Vector2):
	# Coordinate math is run dynamically in _process to handle window shifts
	pass
 
func _on_face_detected(detected: bool):
	if not is_instance_valid(cursor) or not is_instance_valid(status_label):
		return
	if detected:
		cursor.color = Color.GREEN
		status_label.text = "Status: Face Tracked"
	else:
		cursor.color = Color.RED
		status_label.text = "Status: Face Lost"

func _on_lifecycle_changed(state):
	if not is_instance_valid(status_label):
		return
	match state:
		GazeTracker.GazeLifecycle.LIFECYCLE_UNKNOWN:
			status_label.text = "Status: Stopped"
		GazeTracker.GazeLifecycle.LIFECYCLE_PERM_REQ:
			status_label.text = "Status: Requesting Camera Permission..."
		GazeTracker.GazeLifecycle.LIFECYCLE_INITIALIZING:
			status_label.text = "Status: Initializing..."
		GazeTracker.GazeLifecycle.LIFECYCLE_RUNNING:
			status_label.text = "Status: Running"
		GazeTracker.GazeLifecycle.LIFECYCLE_ERROR:
			status_label.text = "Status: Error / Permission Denied"

func _set_maximized(v: bool) -> void:
	print("Setting maximized: ", v)
	is_maximized = v
	if status_label: status_label.text = "Status: Full screen" if is_maximized else "Status: Windowed"
