# Helper script to test the GazeTracker GDExtension module integration.

extends Node2D

@onready var tracker = $GazeTracker
@onready var cursor = $Cursor
@onready var status_label = $StatusLabel

func _ready():
	# Connect to GDExtension signals
	tracker.gaze_updated.connect(_on_gaze_updated)
	tracker.face_detected.connect(_on_face_detected)
	
	# Start tracking
	var success = tracker.initialize_tracker()
	if success:
		status_label.text = "Status: Tracker Initialized"
	else:
		status_label.text = "Status: Initialization Failed (Check OpenCV / models)"

func _on_gaze_updated(pixel: Vector2):
	# Position the visual cursor at the estimated screen coordinate
	cursor.global_position = pixel - cursor.size / 2.0

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
		tracker.calibrate_3d(viewport_center)
		status_label.text = "Status: Calibrated at Screen Center"
