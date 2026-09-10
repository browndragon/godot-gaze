extends "res://addons/godot-gaze/calibration/gaze_calibration_step.gd"

@onready var gravity_label: Label = $VBoxContainer/GravityLabel
@onready var status_label: Label = $VBoxContainer/StatusLabel
@onready var confirm_btn: Button = $VBoxContainer/ButtonContainer/ConfirmButton
@onready var cancel_btn: Button = $VBoxContainer/ButtonContainer/CancelButton

var _timer: float = 0.0

func _ready() -> void:
	if confirm_btn:
		confirm_btn.pressed.connect(_on_confirm_pressed)
	if cancel_btn:
		cancel_btn.pressed.connect(_on_cancel_pressed)

func _process(delta: float) -> void:
	_timer += delta
	if _timer >= 0.1:
		_timer = 0.0
		_update_telemetry()

func _update_telemetry() -> void:
	var vs = Engine.get_singleton("VisionServer")
	if vs and vs.has_method("get_gravity_vector"):
		var grav: Vector3 = vs.get_gravity_vector()
		if gravity_label:
			gravity_label.text = "Gravity Vector: (%.2f, %.2f, %.2f)" % [grav.x, grav.y, grav.z]
		if status_label:
			if abs(grav.y) > 0.7:
				status_label.text = "Orientation: Upright / Standard"
				status_label.modulate = Color(0.3, 0.9, 0.3)
			elif abs(grav.x) > 0.7:
				status_label.text = "Orientation: Rotated Landscape (90° / 270°)"
				status_label.modulate = Color(0.3, 0.7, 1.0)
			else:
				status_label.text = "Orientation: Incline / Tilted"
				status_label.modulate = Color(0.9, 0.8, 0.2)

func _on_confirm_pressed() -> void:
	complete_step()

func _on_cancel_pressed() -> void:
	cancel_step()
