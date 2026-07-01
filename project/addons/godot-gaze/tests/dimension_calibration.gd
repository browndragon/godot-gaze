# Dimension Calibration Screen: calibrates the logical pixel-to-millimeter mapping using a physical card reference.
extends Control

signal dimension_calibration_completed(profile)
signal dimension_calibration_cancelled()

@export var tracker: GazeTracker = null

# Credit card ISO/IEC 7810 ID-1 standard dimensions: 85.60 mm x 53.98 mm
const CARD_PHYSICAL_WIDTH_MM = 85.6
const CARD_ASPECT_RATIO = 85.60 / 53.98

var screen_id: int = 0
var screen_scale: float = 1.0
var screen_size_lpix: Vector2 = Vector2(1280, 720)
var screen_width_viewport: float = 1280.0

@onready var background_rect = $Background
@onready var card_panel = $CardPanel
@onready var width_slider = $Controls/SliderContainer/HSlider
@onready var info_label = $Controls/InfoLabel
@onready var val_label = $Controls/SliderContainer/ValLabel
@onready var save_button = $Controls/BtnContainer/SaveBtn
@onready var cancel_button = $Controls/BtnContainer/CancelBtn

func _ready():
	# Ensure control fills the viewport
	anchors_preset = PRESET_FULL_RECT
	anchor_right = 1.0
	anchor_bottom = 1.0
	
	# Fetch screen metrics
	screen_id = DisplayServer.window_get_current_screen()
	screen_scale = DisplayServer.screen_get_scale(screen_id)
	screen_size_lpix = Vector2(DisplayServer.screen_get_size(screen_id)) / screen_scale
	
	# Calculate viewport-relative screen width to support high-DPI/Retina setups correctly
	var viewport_size = get_viewport().get_visible_rect().size
	var window_size = DisplayServer.window_get_size()
	var screen_size = DisplayServer.screen_get_size(screen_id)
	screen_width_viewport = (float(screen_size.x) / float(window_size.x)) * viewport_size.x
	
	# Try to find gaze tracker in tree if not injected
	if not tracker:
		tracker = get_node_or_null("/root/GazeTracker")
		if not tracker:
			var parent = get_parent()
			if parent and parent.has_node("GazeTracker"):
				tracker = parent.get_node("GazeTracker")
	
	# Configure default slider value from the DisplayProfile, falling back to estimation from OS
	var current_width_lpix = 300.0
	var profile: DisplayProfile = null
	if tracker and tracker.display_profile:
		profile = tracker.display_profile
	else:
		profile = DisplayProfile.estimate_from_os()
		
	if profile:
		var phys_size = profile.get_physical_size_mm()
		if phys_size.x > 0.0:
			var px_per_mm = screen_width_viewport / phys_size.x
			current_width_lpix = px_per_mm * CARD_PHYSICAL_WIDTH_MM
			
	card_panel.draw.connect(_draw_card_ruler)
			
	width_slider.max_value = max(width_slider.max_value, current_width_lpix * 2.0)
	width_slider.value = clamp(current_width_lpix, width_slider.min_value, width_slider.max_value)
	_on_slider_value_changed(width_slider.value)
	
	width_slider.value_changed.connect(_on_slider_value_changed)
	save_button.pressed.connect(_on_save_pressed)
	cancel_button.pressed.connect(_on_cancel_pressed)
	
	# Initial fade-in animation
	modulate.a = 0.0
	var tween = create_tween()
	tween.tween_property(self, "modulate:a", 1.0, 0.4).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_OUT)

func _on_slider_value_changed(val: float):
	# Resize card preview
	var w = val
	var h = w / CARD_ASPECT_RATIO
	card_panel.custom_minimum_size = Vector2(w, h)
	card_panel.size = Vector2(w, h)
	
	# Center the card panel on the screen
	card_panel.position = (size - card_panel.size) / 2.0 - Vector2(0, 80)
	
	# Request card to redraw its ruler markings
	card_panel.queue_redraw()
	
	# Calculate derived screen dimensions in viewport and logical units for preview label
	var px_per_mm_viewport = w / CARD_PHYSICAL_WIDTH_MM
	var px_per_mm_logical = px_per_mm_viewport * (screen_size_lpix.x / screen_width_viewport)
	
	var physical_w_mm = screen_size_lpix.x / px_per_mm_logical
	var physical_h_mm = screen_size_lpix.y / px_per_mm_logical
	
	# Update labels
	val_label.text = "%d px (%.1f px/mm)" % [int(val), px_per_mm_viewport]
	info_label.text = "Estimated Screen size: %.1f cm x %.1f cm (%.1f inches)" % [
		physical_w_mm / 10.0,
		physical_h_mm / 10.0,
		sqrt(physical_w_mm*physical_w_mm + physical_h_mm*physical_h_mm) / 25.4
	]

func _draw_card_ruler():
	var w = card_panel.size.x
	var h = card_panel.size.y
	var px_per_mm = w / CARD_PHYSICAL_WIDTH_MM
	
	var font = ThemeDB.get_fallback_font()
	var font_size = 10
	var tick_color = Color(0.9, 0.9, 0.9, 0.7)
	var label_color = Color(0.8, 0.8, 0.8, 0.9)
	
	# --- Top Edge: Inches ---
	var px_per_inch = px_per_mm * 25.4
	var max_inches = CARD_PHYSICAL_WIDTH_MM / 25.4
	
	# Draw minor ticks first (tenths of an inch)
	for i in range(0, int(max_inches * 10) + 1):
		var x = i * 0.1 * px_per_inch
		if x > w:
			break
		# Skip major ticks (which are drawn next)
		if i % 10 == 0 or i % 5 == 0:
			continue
		card_panel.draw_line(Vector2(x, 0), Vector2(x, 6), tick_color, 1.0)
		
	# Draw half-inch ticks
	for i in range(0, int(max_inches * 2) + 1):
		var x = i * 0.5 * px_per_inch
		if x > w:
			break
		if i % 2 == 0:
			continue
		card_panel.draw_line(Vector2(x, 0), Vector2(x, 10), tick_color, 1.0)
		
	# Draw major inch ticks + labels
	for i in range(0, int(max_inches) + 1):
		var x = i * px_per_inch
		if x > w:
			break
		card_panel.draw_line(Vector2(x, 0), Vector2(x, 14), tick_color, 1.5)
		card_panel.draw_string(font, Vector2(x + 4, 18), "%d\"" % i, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, label_color)

	# --- Bottom Edge: Centimeters ---
	var px_per_cm = px_per_mm * 10.0
	var max_cm = CARD_PHYSICAL_WIDTH_MM / 10.0
	
	# Draw millimeter ticks (10 per cm)
	for c in range(0, int(max_cm * 10) + 1):
		var x = c * 0.1 * px_per_cm
		if x > w:
			break
		if c % 10 == 0 or c % 5 == 0:
			continue
		card_panel.draw_line(Vector2(x, h), Vector2(x, h - 6), tick_color, 1.0)
		
	# Draw 0.5 cm ticks
	for c in range(0, int(max_cm * 2) + 1):
		var x = c * 0.5 * px_per_cm
		if x > w:
			break
		if c % 2 == 0:
			continue
		card_panel.draw_line(Vector2(x, h), Vector2(x, h - 10), tick_color, 1.0)
		
	# Draw major cm ticks + labels
	for c in range(0, int(max_cm) + 1):
		var x = c * px_per_cm
		if x > w:
			break
		card_panel.draw_line(Vector2(x, h), Vector2(x, h - 14), tick_color, 1.5)
		card_panel.draw_string(font, Vector2(x + 4, h - 18), "%d cm" % c, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, label_color)

func _on_save_pressed():
	var w_viewport = width_slider.value
	var px_per_mm_viewport = w_viewport / CARD_PHYSICAL_WIDTH_MM
	var px_per_mm_logical = px_per_mm_viewport * (screen_size_lpix.x / screen_width_viewport)
	var physical_size = screen_size_lpix / px_per_mm_logical
	
	var profile: DisplayProfile = null
	if tracker and tracker.display_profile:
		profile = tracker.display_profile
	else:
		profile = DisplayProfile.estimate_from_os()
	
	profile.set_logical_size_px(Vector2i(int(screen_size_lpix.x), int(screen_size_lpix.y)))
	profile.set_physical_size_mm(physical_size)
	
	if tracker:
		tracker.display_profile = profile
		print("[DimensionCalibration] Saved display profile size: %s mm" % physical_size)
	
	# Fade-out animation before completion
	var tween = create_tween()
	tween.tween_property(self, "modulate:a", 0.0, 0.3).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN)
	await tween.finished
	
	dimension_calibration_completed.emit(profile)
	queue_free()

func _on_cancel_pressed():
	# Fade-out animation before cancel
	var tween = create_tween()
	tween.tween_property(self, "modulate:a", 0.0, 0.3).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN)
	await tween.finished
	
	dimension_calibration_cancelled.emit()
	queue_free()
