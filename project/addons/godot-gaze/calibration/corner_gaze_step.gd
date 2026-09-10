extends "res://addons/godot-gaze/calibration/gaze_calibration_step.gd"

@export var target_hold_time: float = 1.2

var targets = [
	Vector2(0.5, 0.5), # Center
	Vector2(0.1, 0.1), # Top-Left
	Vector2(0.9, 0.1), # Top-Right
	Vector2(0.9, 0.9), # Bottom-Right
	Vector2(0.1, 0.9)  # Bottom-Left
]

var current_target_idx: int = 0
var target_timer: float = 0.0
var latest_gaze_pos: Vector2 = Vector2.ZERO
var face_tracked: bool = false
var target_errors: Array[float] = []

@onready var prompt_label: Label = $PromptLabel
@onready var cancel_btn: Button = $CancelButton

func _ready() -> void:
	if cancel_btn:
		cancel_btn.pressed.connect(_on_cancel_pressed)

func start_step(p_profile: GazeDeviceProfile) -> void:
	super.start_step(p_profile)
	current_target_idx = 0
	target_timer = 0.0
	target_errors.clear()
	var gs = Engine.get_singleton("GazeServer")
	if gs and gs.has_method("start_tracking"):
		gs.start_tracking()
	queue_redraw()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventGaze:
		latest_gaze_pos = event.position
		face_tracked = event.is_face_tracked()

func _process(delta: float) -> void:
	if not is_visible_in_tree() or current_target_idx >= targets.size():
		return

	target_timer += delta
	var viewport_size = get_viewport().get_visible_rect().size
	var target_norm = targets[current_target_idx]
	var target_px = target_norm * viewport_size

	if target_timer >= target_hold_time:
		if face_tracked:
			var err = latest_gaze_pos.distance_to(target_px)
			target_errors.append(err)
		current_target_idx += 1
		target_timer = 0.0

		if current_target_idx >= targets.size():
			_finish_corner_calibration()
			return

	queue_redraw()

func _finish_corner_calibration() -> void:
	var avg_err: float = 0.0
	if target_errors.size() > 0:
		for e in target_errors:
			avg_err += e
		avg_err /= float(target_errors.size())
	if prompt_label:
		prompt_label.text = "Calibration complete! Avg Error: %.1f px" % avg_err
	complete_step()

func _draw() -> void:
	if current_target_idx >= targets.size():
		return

	var viewport_size = get_viewport().get_visible_rect().size
	var target_norm = targets[current_target_idx]
	var target_px = target_norm * viewport_size

	# Draw target ring and center
	draw_circle(target_px, 14.0, Color(0.9, 0.2, 0.2, 0.9))
	draw_circle(target_px, 5.0, Color(1.0, 1.0, 1.0, 1.0))

	# Draw progress ring
	var progress = clamp(target_timer / target_hold_time, 0.0, 1.0)
	var radius = lerp(45.0, 16.0, progress)
	draw_arc(target_px, radius, 0, TAU, 32, Color(0.3, 0.8, 1.0, 0.9), 3.0)

func _on_cancel_pressed() -> void:
	cancel_step()
