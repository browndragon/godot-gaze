extends "res://addons/godot-gaze/calibration/gaze_calibration_step.gd"

@onready var info_label: Label = $VBoxContainer/InfoLabel
@onready var win_pos_label: Label = $VBoxContainer/WinPosLabel
@onready var fullscreen_btn: Button = $VBoxContainer/FullscreenButton
@onready var confirm_btn: Button = $VBoxContainer/ButtonContainer/ConfirmButton
@onready var cancel_btn: Button = $VBoxContainer/ButtonContainer/CancelButton

func _ready() -> void:
	if fullscreen_btn:
		fullscreen_btn.pressed.connect(_on_fullscreen_toggled)
	if confirm_btn:
		confirm_btn.pressed.connect(_on_confirm_pressed)
	if cancel_btn:
		cancel_btn.pressed.connect(_on_cancel_pressed)
	_update_labels()

func _process(_delta: float) -> void:
	_update_labels()

func _update_labels() -> void:
	var screen_id = DisplayServer.window_get_current_screen()
	var screen_size = DisplayServer.screen_get_size(screen_id)
	var win_pos = DisplayServer.window_get_position()
	var win_size = DisplayServer.window_get_size()
	var mode = DisplayServer.window_get_mode()
	var is_fs = (mode == DisplayServer.WINDOW_MODE_FULLSCREEN or mode == DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN)

	if win_pos_label:
		win_pos_label.text = "Screen: %s (%dx%d)\nWindow: %s (%dx%d)\nMode: %s" % [
			screen_id, screen_size.x, screen_size.y,
			win_pos, win_size.x, win_size.y,
			"Fullscreen" if is_fs else "Windowed"
		]
	if fullscreen_btn:
		fullscreen_btn.text = "Switch to Windowed" if is_fs else "Switch to Fullscreen"

func _on_fullscreen_toggled() -> void:
	var mode = DisplayServer.window_get_mode()
	if mode == DisplayServer.WINDOW_MODE_FULLSCREEN or mode == DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN:
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
	else:
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
	_update_labels()

func _on_confirm_pressed() -> void:
	if profile:
		var screen_id = DisplayServer.window_get_current_screen()
		var screen_size = DisplayServer.screen_get_size(screen_id)
		profile.set_logical_size_px(screen_size)
	complete_step()

func _on_cancel_pressed() -> void:
	cancel_step()
