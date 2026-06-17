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
			print("Left Eye Center: ", tracker.get_left_eye_center(), " Gaze Dir: ", tracker.get_left_eye_gaze_direction())
			print("Right Eye Center: ", tracker.get_right_eye_center(), " Gaze Dir: ", tracker.get_right_eye_gaze_direction())
			print("Head Transform:\n", tracker.get_head_transform())
			print("Eye Gaze Pos: ", eye_gaze_pos, " Nose Gaze Pos: ", nose_gaze_pos)
			print("Camera-to-Screen Transform:\n", tracker.get_camera_to_screen_transform())
		print("====================================================")
		
	center_pos = get_viewport().get_visible_rect().size / 2.0
	
	if tracker.is_face_detected():
		# Left Eye Gaze projection
		var left_origin = tracker.get_left_eye_center()
		var left_dir = tracker.get_left_eye_gaze_direction()
		var left_pixel = project_ray_to_screen_pixels(left_origin, left_dir)
		
		# Right Eye Gaze projection
		var right_origin = tracker.get_right_eye_center()
		var right_dir = tracker.get_right_eye_gaze_direction()
		var right_pixel = project_ray_to_screen_pixels(right_origin, right_dir)
		
		var window_pos = DisplayServer.window_get_position()
		
		if left_pixel != Vector2(-1, -1):
			left_eye_pos = left_pixel - Vector2(window_pos)
		else:
			left_eye_pos = Vector2.ZERO
			
		if right_pixel != Vector2(-1, -1):
			right_eye_pos = right_pixel - Vector2(window_pos)
		else:
			right_eye_pos = Vector2.ZERO
			
		# Weighted average of eye gaze intersections (50/50 split)
		if left_eye_pos != Vector2.ZERO and right_eye_pos != Vector2.ZERO:
			eye_gaze_pos = (left_eye_pos + right_eye_pos) * 0.5
		elif left_eye_pos != Vector2.ZERO:
			eye_gaze_pos = left_eye_pos
		elif right_eye_pos != Vector2.ZERO:
			eye_gaze_pos = right_eye_pos
		else:
			eye_gaze_pos = Vector2.ZERO
			
		# Head Pose / Nose Gaze projection
		var head_transform: Transform3D = tracker.get_head_transform()
		# Z-axis basis represents the forward direction vector in this space, negated to point toward screen
		var head_forward: Vector3 = -head_transform.basis.z.normalized()
		var nose_pixel = project_ray_to_screen_pixels(head_transform.origin, head_forward)
		if nose_pixel != Vector2(-1, -1):
			nose_gaze_pos = nose_pixel - Vector2(window_pos)
		else:
			nose_gaze_pos = Vector2.ZERO
	else:
		left_eye_pos = Vector2.ZERO
		right_eye_pos = Vector2.ZERO
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
