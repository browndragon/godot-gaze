# Reusable continuous random dwell calibration controller and HUD.
class_name GazeContinuousCalibrator extends Control

signal point_calibrated(step: int, target_pos: Vector2, bio: GazeBioProfile, delta_deg: float)
signal calibration_converged(bio: GazeBioProfile)
signal calibration_finished(bio: GazeBioProfile)
signal calibration_canceled()

@export var indicator_scene: PackedScene = null
@export var margin_px: float = 70.0
@export var convergence_threshold_deg: float = 0.20
@export var convergence_stable_steps: int = 2
@export var target_radius_px: float = 60.0

var is_calibrating: bool = false
var step_index: int = 0
var current_target_pos: Vector2 = Vector2.ZERO
var active_calib: GazeCalibration = null
var consecutive_converged_steps: int = 0
var is_converged: bool = false

var prev_bias_yaw_deg: float = 0.0
var prev_bias_pitch_deg: float = 0.0
var latest_delta_deg: float = 0.0

var calib_history: Array[Dictionary] = []
var custom_indicator: Node = null
var latest_gaze_event: InputEventGaze = null

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	anchors_preset = Control.PRESET_FULL_RECT

	if indicator_scene != null:
		custom_indicator = indicator_scene.instantiate()
		add_child(custom_indicator)
		if custom_indicator is CanvasItem:
			custom_indicator.visible = false

func start_calibration() -> void:
	active_calib = GazeCalibration.new()
	step_index = 0
	consecutive_converged_steps = 0
	is_converged = false
	calib_history.clear()
	prev_bias_yaw_deg = 0.0
	prev_bias_pitch_deg = 0.0
	latest_delta_deg = 0.0
	is_calibrating = true
	visible = true

	if custom_indicator is CanvasItem:
		custom_indicator.visible = true

	_advance_to_next_point()
	queue_redraw()

func _advance_to_next_point() -> void:
	var vp_rect = get_viewport_rect()
	var center = vp_rect.size * 0.5

	if step_index == 0:
		current_target_pos = center
	else:
		# Randomly generate two points within the screen rectangle and pick the one further from center
		var min_x = margin_px
		var max_x = max(min_x + 10.0, vp_rect.size.x - margin_px)
		var min_y = margin_px
		var max_y = max(min_y + 10.0, vp_rect.size.y - margin_px)

		var p1 = Vector2(randf_range(min_x, max_x), randf_range(min_y, max_y))
		var p2 = Vector2(randf_range(min_x, max_x), randf_range(min_y, max_y))

		var d1 = (p1 - center).length_squared()
		var d2 = (p2 - center).length_squared()
		current_target_pos = p1 if d1 >= d2 else p2

	if active_calib:
		active_calib.set_target(current_target_pos, target_radius_px)

	if custom_indicator is Node2D:
		custom_indicator.position = current_target_pos
	elif custom_indicator is Control:
		custom_indicator.position = current_target_pos - custom_indicator.size * 0.5

func _unhandled_input(event: InputEvent) -> void:
	if not is_calibrating or active_calib == null:
		return

	# If physical mouse is active, cancel active target dwell immediately
	var gs = Engine.get_singleton("GazeServer")
	if gs and gs.is_physical_mouse_active():
		active_calib.cancel_target()
		active_calib.set_target(current_target_pos, target_radius_px)
		queue_redraw()
		return

	if event is InputEventGaze:
		latest_gaze_event = event
		var delta = get_process_delta_time()
		if delta <= 0.0:
			delta = 0.0166
		var completed = active_calib.add_sample(event, delta)
		queue_redraw()
		if completed:
			_on_point_completed()

func _process(delta: float) -> void:
	if not is_calibrating or active_calib == null:
		return

	var gs = Engine.get_singleton("GazeServer")
	if gs and gs.is_physical_mouse_active():
		active_calib.cancel_target()
		active_calib.set_target(current_target_pos, target_radius_px)
		queue_redraw()
		return

	if latest_gaze_event != null:
		var completed = active_calib.add_sample(latest_gaze_event, delta)
		queue_redraw()
		if completed:
			_on_point_completed()

func force_capture() -> void:
	if not is_calibrating or latest_gaze_event == null or active_calib == null:
		return
	active_calib.add_event(latest_gaze_event, current_target_pos)
	_on_point_completed()

func finish_calibration() -> RefCounted:
	if not is_calibrating:
		return null
	is_calibrating = false
	visible = false
	if custom_indicator is CanvasItem:
		custom_indicator.visible = false

	if active_calib and active_calib.get_sample_count() > 0:
		var bio = active_calib.install()
		calibration_finished.emit(bio)
		queue_redraw()
		return bio

	calibration_canceled.emit()
	queue_redraw()
	return null

func cancel_calibration() -> void:
	if not is_calibrating:
		return
	is_calibrating = false
	visible = false
	if custom_indicator is CanvasItem:
		custom_indicator.visible = false
	calibration_canceled.emit()
	queue_redraw()

func _on_point_completed() -> void:
	if not is_calibrating or active_calib == null:
		return

	var bio = active_calib.install()
	if bio == null:
		return

	if step_index > 0:
		var dy = bio.bias_yaw_deg - prev_bias_yaw_deg
		var dp = bio.bias_pitch_deg - prev_bias_pitch_deg
		latest_delta_deg = sqrt(dy * dy + dp * dp)
		if latest_delta_deg < convergence_threshold_deg:
			consecutive_converged_steps += 1
			if consecutive_converged_steps >= convergence_stable_steps:
				is_converged = true
				calibration_converged.emit(bio)
		else:
			consecutive_converged_steps = 0
			is_converged = false
	else:
		latest_delta_deg = 0.0

	prev_bias_yaw_deg = bio.bias_yaw_deg
	prev_bias_pitch_deg = bio.bias_pitch_deg

	calib_history.append({
		"step": step_index + 1,
		"yaw": bio.bias_yaw_deg,
		"pitch": bio.bias_pitch_deg,
		"delta": latest_delta_deg
	})
	if calib_history.size() > 5:
		calib_history.pop_front()

	point_calibrated.emit(step_index, current_target_pos, bio, latest_delta_deg)
	step_index += 1
	_advance_to_next_point()
	queue_redraw()

func _draw() -> void:
	if not is_calibrating:
		return

	var fill_ratio = active_calib.get_display_fill() if active_calib != null else 0.0
	var stage = active_calib.get_stage() if active_calib != null else 0

	# 1. Draw Default Target Indicator if no custom indicator is assigned
	if custom_indicator == null:
		var target_col = Color(0.2, 0.9, 1.0) if stage <= 1 else Color(1.0, 0.65, 0.1)

		# Outer pulsating halo
		var pulse = 1.0 + 0.15 * sin(Time.get_ticks_msec() * 0.008)
		draw_circle(current_target_pos, 26.0 * pulse, Color(target_col.r, target_col.g, target_col.b, 0.18))
		draw_arc(current_target_pos, target_radius_px * 0.5, 0, TAU, 32, Color(target_col.r, target_col.g, target_col.b, 0.35), 1.5)

		# Dynamic Dwell Progress Arc (0 -> fill_ratio * TAU)
		if fill_ratio > 0.0:
			var arc_col = Color(0.3, 0.9, 1.0) if fill_ratio <= 0.5 else Color.WHITE
			draw_arc(current_target_pos, 24.0, -PI * 0.5, -PI * 0.5 + fill_ratio * TAU, 48, arc_col, 4.0)

		# Center bullseye
		draw_circle(current_target_pos, 5.0, Color.WHITE)
		draw_line(current_target_pos - Vector2(8, 0), current_target_pos + Vector2(8, 0), target_col, 1.5)
		draw_line(current_target_pos - Vector2(0, 8), current_target_pos + Vector2(0, 8), target_col, 1.5)

	# 2. Draw Convergence Telemetry HUD Panel
	var vp_size = get_viewport_rect().size
	var hud_w: float = 380.0
	var hud_h: float = 160.0
	var hud_pos = Vector2(vp_size.x - hud_w - 20.0, 20.0)

	draw_rect(Rect2(hud_pos, Vector2(hud_w, hud_h)), Color(0.05, 0.07, 0.11, 0.82), true)
	draw_rect(Rect2(hud_pos, Vector2(hud_w, hud_h)), Color(0.3, 0.4, 0.55, 0.6), false, 1.5)

	var title_col = Color(0.4, 1.0, 0.5) if is_converged else Color(1.0, 0.8, 0.2)
	var title_text = "GAZE CALIBRATION [CONVERGED]" if is_converged else "GAZE CALIBRATION [STEP %d]" % [step_index + 1]
	draw_string(ThemeDB.fallback_font, hud_pos + Vector2(16, 24), title_text, HORIZONTAL_ALIGNMENT_LEFT, -1, 14, title_col)

	var stage_name = "Settling" if stage <= 1 else "Capturing"
	var mode_text = "Target %d | Stage: %s (Fill: %d%%)" % [step_index + 1, stage_name, int(fill_ratio * 100)]
	draw_string(ThemeDB.fallback_font, hud_pos + Vector2(16, 44), mode_text, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color(0.7, 0.75, 0.85))

	var kappa_text = "Angle Kappa: Yaw: %+.2f° | Pitch: %+.2f°" % [prev_bias_yaw_deg, prev_bias_pitch_deg]
	draw_string(ThemeDB.fallback_font, hud_pos + Vector2(16, 68), kappa_text, HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color.WHITE)

	var delta_col = Color(0.3, 1.0, 0.4) if latest_delta_deg < convergence_threshold_deg and step_index > 0 else Color(1.0, 0.6, 0.2)
	var delta_text = "Step Delta Δ: %.3f° (Threshold: %.2f°)" % [latest_delta_deg, convergence_threshold_deg] if step_index > 0 else "Step Delta Δ: Initializing..."
	draw_string(ThemeDB.fallback_font, hud_pos + Vector2(16, 88), delta_text, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, delta_col)

	var hist_str = "History: "
	for item in calib_history:
		hist_str += "#%d(Δ%.2f°) " % [item["step"], item["delta"]]
	draw_string(ThemeDB.fallback_font, hud_pos + Vector2(16, 114), hist_str, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color(0.65, 0.75, 0.9))

	var instr_text = "[ESC] Finish & Apply | [SPACE] Force Point"
	draw_string(ThemeDB.fallback_font, hud_pos + Vector2(16, 142), instr_text, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color(0.55, 0.6, 0.7))
