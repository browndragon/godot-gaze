class_name GazeCalibrationWizard
extends Control

const GazeCalibrationStep = preload("res://addons/godot-gaze/calibration/gaze_calibration_step.gd")

signal wizard_completed(profile: GazeDeviceProfile)
signal wizard_canceled()

@export var auto_save_to_user_dir: bool = true
@export var target_profile_path: String = "user://gaze_device_profile.tres"

@onready var step_container: Control = $StepContainer
@onready var display_size_step: Control = $StepContainer/DisplaySizeStep
@onready var sensor_orientation_step: Control = $StepContainer/SensorOrientationStep
@onready var camera_placement_step: Control = $StepContainer/CameraPlacementStep
@onready var screen_placement_step: Control = $StepContainer/ScreenPlacementStep
@onready var corner_gaze_step: Control = $StepContainer/CornerGazeStep

var _steps: Array = []
var _current_step_index: int = 0
var profile: GazeDeviceProfile

func _ready() -> void:
	_steps = [
		display_size_step,
		sensor_orientation_step,
		camera_placement_step,
		screen_placement_step,
		corner_gaze_step
	]
	
	for step in _steps:
		if step:
			step.hide()
			step.step_finished.connect(_on_step_finished)
			step.calibration_canceled.connect(_on_step_canceled)

func start_wizard(p_profile: GazeDeviceProfile = null) -> void:
	if p_profile != null:
		profile = p_profile
	else:
		var gs = Engine.get_singleton("GazeServer")
		if gs and gs.has_method("get_device_profile") and gs.get_device_profile() != null:
			profile = gs.get_device_profile()
		else:
			profile = GazeDeviceProfile.create_system_guess()
	
	_current_step_index = 0
	_show_current_step()

func _show_current_step() -> void:
	for step in _steps:
		if step:
			step.hide()
	
	if _current_step_index < _steps.size():
		var step = _steps[_current_step_index]
		if step:
			step.start_step(profile)
	else:
		_finish_wizard()

func _on_step_finished(updated_profile: GazeDeviceProfile) -> void:
	profile = updated_profile
	_current_step_index += 1
	_show_current_step()

func _on_step_canceled() -> void:
	for step in _steps:
		if step:
			step.hide()
	wizard_canceled.emit()

func _finish_wizard() -> void:
	var gs = Engine.get_singleton("GazeServer")
	if gs and gs.has_method("set_device_profile"):
		gs.set_device_profile(profile)
	
	if auto_save_to_user_dir and profile:
		ResourceSaver.save(profile, target_profile_path)
	
	wizard_completed.emit(profile)
