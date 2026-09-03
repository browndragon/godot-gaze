extends Control

## Recording Harness for Video & Benchmark Dataset Synchronization
## Features clean minimal UI, automated 3-second red-recording timers, and flicker feedback.

enum State {
	IDLE,
	READY_FOR_CUE,
	RECORDING_CUE,
	FLICKERING_COMPLETION,
	COMPLETED
}

const COLOR_GREEN: Color = Color(0.0, 0.95, 0.45, 1.0)
const COLOR_RED: Color = Color(1.0, 0.15, 0.15, 1.0)
const CUE_DURATION_SEC: float = 3.0

var current_state: State = State.IDLE
var current_cue_idx: int = 0
var recording_start_time_msec: int = 0
var cue_start_time_msec: int = 0

var recorded_cues: Array[Dictionary] = []

const CUE_DEFINITIONS: Array[Dictionary] = [
	{
		"id": "self_center",
		"title": "Screen Center",
		"instructions": "Look naturally at the square with head upright.",
		"target_pos_normalized": Vector2(0.5, 0.5)
	},
	{
		"id": "self_top_left",
		"title": "Top-Left Corner",
		"instructions": "Keep head still, look at the top-left square.",
		"target_pos_normalized": Vector2(0.06, 0.08)
	},
	{
		"id": "self_top_right",
		"title": "Top-Right Corner",
		"instructions": "Keep head still, look at the top-right square.",
		"target_pos_normalized": Vector2(0.94, 0.08)
	},
	{
		"id": "self_bottom_left",
		"title": "Bottom-Left Corner",
		"instructions": "Keep head still, look at the bottom-left square.",
		"target_pos_normalized": Vector2(0.06, 0.92)
	},
	{
		"id": "self_bottom_right",
		"title": "Bottom-Right Corner",
		"instructions": "Keep head still, look at the bottom-right square.",
		"target_pos_normalized": Vector2(0.94, 0.92)
	},
	{
		"id": "self_yaw_left",
		"title": "Head Yaw Left",
		"instructions": "Turn head ~20° left, keep eyes on square.",
		"target_pos_normalized": Vector2(0.5, 0.5)
	},
	{
		"id": "self_yaw_right",
		"title": "Head Yaw Right",
		"instructions": "Turn head ~20° right, keep eyes on square.",
		"target_pos_normalized": Vector2(0.5, 0.5)
	},
	{
		"id": "self_pitch_down",
		"title": "Head Pitch Down",
		"instructions": "Tilt chin down ~15°, keep eyes up on square.",
		"target_pos_normalized": Vector2(0.5, 0.5)
	},
	{
		"id": "self_pitch_up",
		"title": "Head Pitch Up",
		"instructions": "Tilt chin up ~15°, keep eyes down on square.",
		"target_pos_normalized": Vector2(0.5, 0.5)
	},
	{
		"id": "self_roll_left",
		"title": "Head Roll Left",
		"instructions": "Tilt head left toward shoulder, keep eyes on square.",
		"target_pos_normalized": Vector2(0.5, 0.5)
	},
	{
		"id": "self_roll_right",
		"title": "Head Roll Right",
		"instructions": "Tilt head right toward shoulder, keep eyes on square.",
		"target_pos_normalized": Vector2(0.5, 0.5)
	},
	{
		"id": "eyes_both_open",
		"title": "Both Eyes Wide Open",
		"instructions": "Both eyes open naturally, look at square.",
		"target_pos_normalized": Vector2(0.5, 0.5)
	},
	{
		"id": "eyes_both_wink",
		"title": "Both Eyes Closed (Blink)",
		"instructions": "Close both eyes gently facing screen.",
		"target_pos_normalized": Vector2(0.5, 0.5)
	},
	{
		"id": "eyes_left_wink",
		"title": "Left Eye Wink",
		"instructions": "Close left eye, keep right eye on square.",
		"target_pos_normalized": Vector2(0.5, 0.5)
	},
	{
		"id": "eyes_right_wink",
		"title": "Right Eye Wink",
		"instructions": "Close right eye, keep left eye on square.",
		"target_pos_normalized": Vector2(0.5, 0.5)
	}
]

@onready var target_marker: ColorRect = $TargetMarker
@onready var header_label: Label = $TopContainer/HeaderLabel
@onready var sub_label: Label = $TopContainer/SubLabel

func _ready() -> void:
	recording_start_time_msec = Time.get_ticks_msec()
	current_cue_idx = 0
	current_state = State.READY_FOR_CUE
	target_marker.color = COLOR_GREEN
	_update_ui()

func _input(event: InputEvent) -> void:
	if event is InputEventKey and event.is_pressed() and not event.is_echo():
		if event.keycode == KEY_SPACE:
			if current_state == State.READY_FOR_CUE:
				_start_recording_cue()
			# Intentionally reject / ignore space while RECORDING_CUE or FLICKERING
		elif event.keycode == KEY_ESCAPE:
			_handle_escape_pressed()

func _process(_delta: float) -> void:
	if current_state == State.RECORDING_CUE:
		var elapsed_cue_sec = (Time.get_ticks_msec() - cue_start_time_msec) / 1000.0
		if elapsed_cue_sec >= CUE_DURATION_SEC:
			_finish_recording_current_cue()

func _start_recording_cue() -> void:
	cue_start_time_msec = Time.get_ticks_msec()
	current_state = State.RECORDING_CUE
	target_marker.color = COLOR_RED
	var cue = CUE_DEFINITIONS[current_cue_idx]
	header_label.text = "[%d/%d] %s" % [current_cue_idx + 1, CUE_DEFINITIONS.size(), cue["title"]]
	sub_label.text = "RECORDING (Hold gaze on RED square)..."
	sub_label.modulate = COLOR_RED

func _finish_recording_current_cue() -> void:
	var cue_end_time_msec = Time.get_ticks_msec()
	var cue_def = CUE_DEFINITIONS[current_cue_idx]
	var viewport_sz = get_viewport_rect().size
	var target_px = cue_def["target_pos_normalized"] * viewport_sz

	var record_entry = {
		"cue_index": current_cue_idx,
		"id": cue_def["id"],
		"title": cue_def["title"],
		"start_time_msec": cue_start_time_msec,
		"end_time_msec": cue_end_time_msec,
		"duration_msec": cue_end_time_msec - cue_start_time_msec,
		"relative_start_sec": (cue_start_time_msec - recording_start_time_msec) / 1000.0,
		"relative_end_sec": (cue_end_time_msec - recording_start_time_msec) / 1000.0,
		"target_normalized": [cue_def["target_pos_normalized"].x, cue_def["target_pos_normalized"].y],
		"target_px": [target_px.x, target_px.y],
		"viewport_size": [viewport_sz.x, viewport_sz.y]
	}
	recorded_cues.append(record_entry)
	print("[RecordingHarness] Logged Cue '%s' (%.2fs - %.2fs, duration: %.2fs)" % [
		cue_def["id"], record_entry["relative_start_sec"], record_entry["relative_end_sec"], record_entry["duration_msec"] / 1000.0
	])

	current_cue_idx += 1
	if current_cue_idx >= CUE_DEFINITIONS.size():
		_finish_session()
		return

	current_state = State.FLICKERING_COMPLETION
	_update_ui()
	
	# Flicker to Green: flash Green -> Dark -> Green -> Dark -> Solid Green (0.24s)
	var tween = create_tween()
	tween.tween_property(target_marker, "color", Color.BLACK, 0.06)
	tween.tween_property(target_marker, "color", COLOR_GREEN, 0.06)
	tween.tween_property(target_marker, "color", Color.BLACK, 0.06)
	tween.tween_property(target_marker, "color", COLOR_GREEN, 0.06)
	await tween.finished

	current_state = State.READY_FOR_CUE

func _handle_escape_pressed() -> void:
	if current_state == State.RECORDING_CUE:
		# Cancel current recording and reset back to ready
		current_state = State.READY_FOR_CUE
		target_marker.color = COLOR_GREEN
		_update_ui()
	elif current_state == State.READY_FOR_CUE and current_cue_idx > 0:
		# Step back to redo previous cue
		current_cue_idx -= 1
		if recorded_cues.size() > current_cue_idx:
			recorded_cues.remove_at(current_cue_idx)
		target_marker.color = COLOR_GREEN
		_update_ui()

func _update_ui() -> void:
	if current_cue_idx >= CUE_DEFINITIONS.size():
		return

	var cue = CUE_DEFINITIONS[current_cue_idx]
	var total_cues = CUE_DEFINITIONS.size()
	
	header_label.text = "[%d/%d] %s" % [current_cue_idx + 1, total_cues, cue["title"]]
	sub_label.text = "%s  (Press [SPACE] when ready)" % [cue["instructions"]]
	sub_label.modulate = Color(0.8, 0.85, 0.9)

	var viewport_sz = get_viewport_rect().size
	var marker_norm: Vector2 = cue["target_pos_normalized"]
	var marker_pos = (marker_norm * viewport_sz) - (target_marker.size * 0.5)
	target_marker.position = marker_pos
	target_marker.color = COLOR_GREEN

func _finish_session() -> void:
	current_state = State.COMPLETED
	target_marker.visible = false
	header_label.text = "Session Completed! All Cues Recorded."
	sub_label.text = "Manifest saved to disk and printed below."
	sub_label.modulate = COLOR_GREEN

	var total_duration = (Time.get_ticks_msec() - recording_start_time_msec) / 1000.0
	var out_dict = {
		"session_id": "recording_%d" % [Time.get_unix_time_from_system()],
		"total_duration_sec": total_duration,
		"cues_count": recorded_cues.size(),
		"cues": recorded_cues
	}

	var json_str = JSON.stringify(out_dict, "  ")
	var file_path = "user://recording_manifest_%d.json" % [Time.get_unix_time_from_system()]
	var fa = FileAccess.open(file_path, FileAccess.WRITE)
	if fa:
		fa.store_string(json_str)
		fa.close()
		print("[RecordingHarness] Manifest saved to: ", ProjectSettings.globalize_path(file_path))
	
	print("\n================ RECORDING MANIFEST JSON ================")
	print(json_str)
	print("=========================================================\n")
