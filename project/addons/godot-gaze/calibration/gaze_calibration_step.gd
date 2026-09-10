class_name GazeCalibrationStep
extends Control

signal step_finished(profile: GazeDeviceProfile)
signal calibration_canceled()

var profile: GazeDeviceProfile

func start_step(p_profile: GazeDeviceProfile) -> void:
	profile = p_profile
	show()

func complete_step() -> void:
	step_finished.emit(profile)

func cancel_step() -> void:
	calibration_canceled.emit()
