extends "res://addons/godot-gaze/calibration/gaze_calibration_step.gd"

@onready var card_rect: ColorRect = $CardContainer/CardRect
@onready var size_slider: HSlider = $VBoxContainer/SliderContainer/HSlider
@onready var value_label: Label = $VBoxContainer/SliderContainer/ValueLabel
@onready var info_label: Label = $VBoxContainer/InfoLabel
@onready var confirm_btn: Button = $VBoxContainer/ButtonContainer/ConfirmButton
@onready var cancel_btn: Button = $VBoxContainer/ButtonContainer/CancelButton

const CARD_WIDTH_MM: float = 85.603
const CARD_ASPECT: float = 53.98 / 85.603

func _ready() -> void:
	if size_slider:
		size_slider.value_changed.connect(_on_slider_value_changed)
	if confirm_btn:
		confirm_btn.pressed.connect(_on_confirm_pressed)
	if cancel_btn:
		cancel_btn.pressed.connect(_on_cancel_pressed)
	_update_card_display(350.0)

func start_step(p_profile: GazeDeviceProfile) -> void:
	super.start_step(p_profile)
	var initial_width_px: float = 350.0
	if profile and profile.get_pixel_pitch_mm().x > 0.0:
		initial_width_px = CARD_WIDTH_MM / profile.get_pixel_pitch_mm().x
	if size_slider:
		size_slider.value = initial_width_px
	_update_card_display(initial_width_px)

func _on_slider_value_changed(value: float) -> void:
	_update_card_display(value)

func _update_card_display(width_px: float) -> void:
	if card_rect:
		card_rect.custom_minimum_size = Vector2(width_px, width_px * CARD_ASPECT)
		card_rect.size = card_rect.custom_minimum_size
	var pitch_mm: float = CARD_WIDTH_MM / max(width_px, 1.0)
	var dpi: float = 25.4 / pitch_mm
	if value_label:
		value_label.text = "Card Width: %d px | Pitch: %.3f mm | DPI: %.1f" % [int(width_px), pitch_mm, dpi]

func _on_confirm_pressed() -> void:
	var width_px: float = size_slider.value if size_slider else 350.0
	if profile:
		var pitch: float = CARD_WIDTH_MM / max(width_px, 1.0)
		profile.set_pixel_pitch_mm(Vector2(pitch, pitch))
	complete_step()

func _on_cancel_pressed() -> void:
	cancel_step()
