# Camera Calibration Screen: calibrates the webcam's focal length / FOV using a physical card reference.
extends Control

signal camera_calibration_completed(fov_degrees)
signal camera_calibration_cancelled()

# Credit card ISO/IEC 7810 ID-1 standard dimensions: 85.603 mm x 53.98 mm
const CARD_PHYSICAL_WIDTH_MM = 85.603
const CARD_ASPECT_RATIO = 85.603 / 53.98

# Default calibration distance (user holds the card 50 cm away from the webcam)
@export var card_distance_mm: float = 500.0
@export var default_fov_degrees: float = 53.13 # OpenCV's standard default (f = W)

var current_fov: float = default_fov_degrees
var frame_width_pixels: float = 640.0

@onready var camera_feed_rect = $CameraFeedTexture
@onready var guide_box = $GuideBox
@onready var fov_slider = $Controls/SliderContainer/HSlider
@onready var fov_val_label = $Controls/SliderContainer/ValLabel
@onready var save_button = $Controls/BtnContainer/SaveBtn

func _ready():
	anchors_preset = PRESET_FULL_RECT
	current_fov = default_fov_degrees
	
	if fov_slider:
		fov_slider.min_value = 30.0
		fov_slider.max_value = 90.0
		fov_slider.step = 0.1
		fov_slider.value = current_fov
		fov_slider.value_changed.connect(_on_fov_slider_changed)
		
	_update_guide_box()

func _on_fov_slider_changed(value: float):
	current_fov = value
	if fov_val_label:
		fov_val_label.text = "%.1f deg" % current_fov
	_update_guide_box()

func _update_guide_box():
	# Retrieve expected pixel width of the card using our static DeviceCalibration helper
	var card_width_px = DeviceCalibration.get_card_width_px(
		current_fov,
		card_distance_mm,
		frame_width_pixels,
		CARD_PHYSICAL_WIDTH_MM
	)
	
	var card_height_px = card_width_px / CARD_ASPECT_RATIO
	
	# Position and resize the guide box in the center of the camera preview
	if guide_box:
		var center = get_viewport().get_visible_rect().size * 0.5
		guide_box.size = Vector2(card_width_px, card_height_px)
		guide_box.position = center - guide_box.size * 0.5

func _on_save_button_pressed():
	emit_signal("camera_calibration_completed", current_fov)

func _on_cancel_button_pressed():
	emit_signal("camera_calibration_cancelled")
