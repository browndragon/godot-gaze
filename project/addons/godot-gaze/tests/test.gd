# Helper script to test the GazeTracker GDExtension module integration.
extends Node2D

@onready var tracker = $GazeTracker
@onready var cursor = $Cursor
@onready var status_label = $StatusLabel

var left_eye_pos: Vector2 = Vector2.ZERO
var right_eye_pos: Vector2 = Vector2.ZERO
var eye_gaze_pos: Vector2 = Vector2.ZERO
var nose_gaze_pos: Vector2 = Vector2.ZERO
var center_pos: Vector2 = Vector2.ZERO
var coords_label: Label

func _ready():
	# Connect to GDExtension signals
	tracker.gaze_updated.connect(_on_gaze_updated)
	tracker.face_detected.connect(_on_face_detected)
	
	# Create a coordinate feedback label near screen center
	coords_label = Label.new()
	add_child(coords_label)
	coords_label.text = ""
	
	# Set model paths (relative to project root)
	tracker.yunet_model_path = "res://models/face_detection_yunet_2023mar.onnx"
	tracker.gaze_onnx_path = "res://models/gaze-estimation-adas-0002.xml"
	
	# Start tracking
	var success = tracker.initialize_tracker()
	if success:
		status_label.text = "Status: Tracker Initialized"
	else:
		status_label.text = "Status: Initialization Failed (Check OpenCV / models)"

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
		
	center_pos = get_viewport().get_visible_rect().size / 2.0
	
	if tracker.is_face_detected():
		# 1. Head Pose / Nose Gaze projection
		var head_forward = tracker.get_head_forward()
		var nose_pixel = tracker.project_gaze_ray_to_viewport(tracker.get_head_position(), head_forward)
		if nose_pixel != Vector2.INF:
			nose_gaze_pos = nose_pixel
		else:
			nose_gaze_pos = Vector2.ZERO
			
		# 2. Unified Eye Gaze projection
		var gaze_origin = tracker.get_gaze_origin()
		var gaze_dir = tracker.get_gaze_direction()
		var gaze_pixel = tracker.project_gaze_ray_to_viewport(gaze_origin, gaze_dir)
		if gaze_pixel != Vector2.INF:
			eye_gaze_pos = gaze_pixel
		else:
			eye_gaze_pos = Vector2.ZERO
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
	if detected:
		cursor.color = Color.GREEN
		status_label.text = "Status: Face Tracked"
	else:
		cursor.color = Color.RED
		status_label.text = "Status: Face Lost"
 
func _input(event):
	# Press SPACE to trigger a 3D calibration at the center of the screen
	if event.is_action_pressed("ui_select"):
		var viewport_center = get_viewport().get_visible_rect().size / 2.0
		# Map window coordinate to screen coordinate
		var screen_center = viewport_center + Vector2(DisplayServer.window_get_position())
		tracker.calibrate_3d(screen_center)
		status_label.text = "Status: Calibrated at Screen Center"
 
