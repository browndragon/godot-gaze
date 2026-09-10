extends "res://addons/godot-gaze/calibration/gaze_calibration_step.gd"

enum CameraMountEdge {
	TOP = 0,
	RIGHT = 1,
	BOTTOM = 2,
	LEFT = 3
}

@onready var top_btn: Button = $VBoxContainer/BezelContainer/TopButton
@onready var right_btn: Button = $VBoxContainer/BezelContainer/RightButton
@onready var bottom_btn: Button = $VBoxContainer/BezelContainer/BottomButton
@onready var left_btn: Button = $VBoxContainer/BezelContainer/LeftButton
@onready var status_label: Label = $VBoxContainer/StatusLabel
@onready var confirm_btn: Button = $VBoxContainer/ButtonContainer/ConfirmButton
@onready var cancel_btn: Button = $VBoxContainer/ButtonContainer/CancelButton

var selected_edge: CameraMountEdge = CameraMountEdge.TOP

func _ready() -> void:
	if top_btn:
		top_btn.pressed.connect(func(): _select_edge(CameraMountEdge.TOP))
	if right_btn:
		right_btn.pressed.connect(func(): _select_edge(CameraMountEdge.RIGHT))
	if bottom_btn:
		bottom_btn.pressed.connect(func(): _select_edge(CameraMountEdge.BOTTOM))
	if left_btn:
		left_btn.pressed.connect(func(): _select_edge(CameraMountEdge.LEFT))
	if confirm_btn:
		confirm_btn.pressed.connect(_on_confirm_pressed)
	if cancel_btn:
		cancel_btn.pressed.connect(_on_cancel_pressed)
	_update_ui()

func start_step(p_profile: GazeDeviceProfile) -> void:
	super.start_step(p_profile)
	if profile:
		var off = profile.get_camera_offset_mm()
		if off.x > 10.0:
			selected_edge = CameraMountEdge.RIGHT
		elif off.x < -10.0:
			selected_edge = CameraMountEdge.LEFT
		elif off.y < -10.0:
			selected_edge = CameraMountEdge.BOTTOM
		else:
			selected_edge = CameraMountEdge.TOP
	_update_ui()

func _select_edge(edge: CameraMountEdge) -> void:
	selected_edge = edge
	_update_ui()

func _update_ui() -> void:
	if top_btn:
		top_btn.flat = (selected_edge != CameraMountEdge.TOP)
	if right_btn:
		right_btn.flat = (selected_edge != CameraMountEdge.RIGHT)
	if bottom_btn:
		bottom_btn.flat = (selected_edge != CameraMountEdge.BOTTOM)
	if left_btn:
		left_btn.flat = (selected_edge != CameraMountEdge.LEFT)
	if status_label:
		match selected_edge:
			CameraMountEdge.TOP:
				status_label.text = "Selected Mount: Top Bezel Center (Standard)"
			CameraMountEdge.RIGHT:
				status_label.text = "Selected Mount: Right Bezel (Rotated Device)"
			CameraMountEdge.BOTTOM:
				status_label.text = "Selected Mount: Bottom Bezel"
			CameraMountEdge.LEFT:
				status_label.text = "Selected Mount: Left Bezel (Rotated Device)"

func _on_confirm_pressed() -> void:
	if profile:
		var screen_w_mm = profile.get_physical_size_mm().x
		var screen_h_mm = profile.get_physical_size_mm().y
		match selected_edge:
			CameraMountEdge.TOP:
				profile.set_camera_offset_mm(Vector3(0.0, 0.0, 0.0))
			CameraMountEdge.RIGHT:
				profile.set_camera_offset_mm(Vector3(screen_w_mm * 0.5, 0.0, 0.0))
			CameraMountEdge.BOTTOM:
				profile.set_camera_offset_mm(Vector3(0.0, -screen_h_mm, 0.0))
			CameraMountEdge.LEFT:
				profile.set_camera_offset_mm(Vector3(-screen_w_mm * 0.5, 0.0, 0.0))
	complete_step()

func _on_cancel_pressed() -> void:
	cancel_step()
