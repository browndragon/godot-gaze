extends Control

## Interactive Landscape Fixture Capture Tool
## Captures high-resolution camera frames from physical webcam rotated 90° sideways.
## Supports full UI rotation compensation, animated bullseye targets, and real-time pose telemetry.

const CUES = [
	{
		"id": "landscape_center",
		"title": "1/9: Center Baseline",
		"head_inst": "Head upright, facing forward.",
		"eyes_inst": "Gaze directly at the CENTER bullseye.",
		"target_uv": Vector2(0.5, 0.5)
	},
	{
		"id": "landscape_left_left",
		"title": "2/9: Head Left, Eyes Left",
		"head_inst": "Turn head LEFT (~25°) towards keyboard side.",
		"eyes_inst": "Gaze at the FAR LEFT bullseye (opposite camera).",
		"target_uv": Vector2(0.04, 0.5)
	},
	{
		"id": "landscape_right_right",
		"title": "3/9: Head Right, Eyes Right",
		"head_inst": "Turn head RIGHT (~25°) towards camera bezel.",
		"eyes_inst": "Gaze at the FAR RIGHT bullseye (near camera).",
		"target_uv": Vector2(0.96, 0.5)
	},
	{
		"id": "landscape_top_top",
		"title": "4/9: Head Up, Eyes Up",
		"head_inst": "Tilt head UP (~20°) towards ceiling.",
		"eyes_inst": "Gaze at the TOP bullseye (ceiling edge).",
		"target_uv": Vector2(0.5, 0.04)
	},
	{
		"id": "landscape_down_down",
		"title": "5/9: Head Down, Eyes Down",
		"head_inst": "Tilt head DOWN (~20°) towards desk.",
		"eyes_inst": "Gaze at the BOTTOM bullseye (desk edge).",
		"target_uv": Vector2(0.5, 0.96)
	},
	{
		"id": "landscape_noseleft_eyesright",
		"title": "6/9: Dissociated: Head LEFT, Eyes RIGHT",
		"head_inst": "Turn head LEFT (~20°) towards keyboard...",
		"eyes_inst": "...while gazing RIGHT at the bullseye near camera!",
		"target_uv": Vector2(0.94, 0.5)
	},
	{
		"id": "landscape_noseright_eyesleft",
		"title": "7/9: Dissociated: Head RIGHT, Eyes LEFT",
		"head_inst": "Turn head RIGHT (~20°) towards camera...",
		"eyes_inst": "...while gazing LEFT at the bullseye near keyboard!",
		"target_uv": Vector2(0.06, 0.5)
	},
	{
		"id": "landscape_nosetop_eyesdown",
		"title": "8/9: Dissociated: Head UP, Eyes DOWN",
		"head_inst": "Tilt head UP (~18°) towards ceiling...",
		"eyes_inst": "...while gazing DOWN at the bullseye near desk!",
		"target_uv": Vector2(0.5, 0.94)
	},
	{
		"id": "landscape_nosedown_eyesup",
		"title": "9/9: Dissociated: Head DOWN, Eyes UP",
		"head_inst": "Tilt head DOWN (~18°) towards desk...",
		"eyes_inst": "...while gazing UP at the bullseye near ceiling!",
		"target_uv": Vector2(0.5, 0.06)
	}
]

# Orientation angle: 90 = Laptop rotated 90° CW (Camera on Right Bezel)
var current_orientation_deg: int = 90
var current_cue_idx: int = 0
var is_capturing: bool = false
var pulse_time: float = 0.0

var vs = null
var gs = null
var cam_rid: RID

# UI References
@onready var root_rotator: Control = $RootRotator
@onready var preview_rect: TextureRect = $RootRotator/CenterLayout/PreviewContainer/PreviewRect
@onready var silhouette_overlay: Control = $RootRotator/CenterLayout/PreviewContainer/SilhouetteOverlay
@onready var target_marker: Control = $RootRotator/TargetMarker
@onready var header_label: Label = $RootRotator/TopContainer/HeaderLabel
@onready var head_label: Label = $RootRotator/TopContainer/HeadLabel
@onready var eyes_label: Label = $RootRotator/TopContainer/EyesLabel
@onready var status_label: Label = $RootRotator/TopContainer/StatusLabel
@onready var telemetry_label: Label = $RootRotator/BottomContainer/TelemetryLabel
@onready var controls_label: Label = $RootRotator/BottomContainer/ControlsLabel

func _ready() -> void:
	vs = Engine.get_singleton("VisionServer")
	gs = Engine.get_singleton("GazeServer")
	if not vs or not gs:
		printerr("FATAL: VisionServer or GazeServer singleton missing!")
		get_tree().quit(1)
		return

	cam_rid = vs.camera_create()
	vs.camera_set_device_id(cam_rid, 0)
	vs.camera_set_preview_requested(cam_rid, true)
	var started = vs.camera_start(cam_rid)
	print("Camera started: ", started)

	gs.set_camera_vision_rid(cam_rid)
	gs.start_processing()

	# Custom draw callbacks
	target_marker.draw.connect(_on_target_marker_draw)
	silhouette_overlay.draw.connect(_on_silhouette_draw)

	# Start maximized (use [F] to toggle fullscreen)
	DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_MAXIMIZED)

	get_tree().root.size_changed.connect(_update_layout)
	_update_layout()
	_update_ui()

func _exit_tree() -> void:
	if gs:
		gs.stop_processing()
	if vs and cam_rid.is_valid():
		vs.camera_stop(cam_rid)
		vs.camera_free(cam_rid)
		cam_rid = RID()

func _process(delta: float) -> void:
	pulse_time += delta * 4.0
	target_marker.queue_redraw()
	silhouette_overlay.queue_redraw()

	if vs and cam_rid.is_valid():
		var tex = vs.get_camera_current_texture(cam_rid)
		if tex:
			preview_rect.texture = tex

	# Update live telemetry
	if gs:
		var detected = gs.is_face_detected()
		if detected:
			var rot = gs.get_head_pose_euler_deg()
			telemetry_label.text = "Tracking: [FACE DETECTED] | Pose: Yaw: %+.1f°  Pitch: %+.1f°  Roll: %+.1f°" % [
				rot.y, rot.x, rot.z
			]
			telemetry_label.modulate = Color(0.2, 1.0, 0.4)
		else:
			telemetry_label.text = "Tracking: [NO FACE DETECTED - Look at camera to align]"
			telemetry_label.modulate = Color(1.0, 0.4, 0.3)

func _input(event: InputEvent) -> void:
	if not (event is InputEventKey and event.is_pressed() and not event.is_echo()):
		return

	match event.keycode:
		KEY_SPACE:
			if not is_capturing:
				_capture_current_cue()
		KEY_ESCAPE, KEY_BACKSPACE:
			if current_cue_idx > 0 and not is_capturing:
				current_cue_idx -= 1
				_update_ui()
		KEY_F, KEY_F11:
			var curr = DisplayServer.window_get_mode()
			if curr == DisplayServer.WINDOW_MODE_FULLSCREEN:
				DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
			else:
				DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
		KEY_1:
			_set_cue_or_orientation(0, event.is_command_or_control_pressed())
		KEY_2:
			_set_cue_or_orientation(1, event.is_command_or_control_pressed())
		KEY_3:
			_set_cue_or_orientation(2, event.is_command_or_control_pressed())
		KEY_4:
			_set_cue_or_orientation(3, event.is_command_or_control_pressed())
		KEY_5:
			_set_cue_or_orientation(4, event.is_command_or_control_pressed())
		KEY_6:
			_set_cue_or_orientation(5, event.is_command_or_control_pressed())
		KEY_7:
			_set_cue_or_orientation(6, event.is_command_or_control_pressed())
		KEY_8:
			_set_cue_or_orientation(7, event.is_command_or_control_pressed())
		KEY_9:
			_set_cue_or_orientation(8, event.is_command_or_control_pressed())
		KEY_R:
			# Cycle orientation: 0 -> 90 -> 180 -> 270
			current_orientation_deg = (current_orientation_deg + 90) % 360
			_update_layout()
			_update_ui()

func _set_cue_or_orientation(idx: int, is_ctrl: bool) -> void:
	if is_ctrl:
		# Ctrl+1..4 sets orientation
		var angles = [0, 90, 180, 270]
		if idx < angles.size():
			current_orientation_deg = angles[idx]
			_update_layout()
			_update_ui()
	else:
		if idx < CUES.size() and not is_capturing:
			current_cue_idx = idx
			_update_ui()

func _update_layout() -> void:
	var vp_sz = get_viewport_rect().size
	var center = vp_sz * 0.5

	root_rotator.set_anchors_preset(Control.PRESET_TOP_LEFT)

	match current_orientation_deg:
		0:
			root_rotator.rotation_degrees = 0.0
			root_rotator.size = vp_sz
			root_rotator.pivot_offset = Vector2.ZERO
			root_rotator.position = Vector2.ZERO
		90:
			# Laptop rotated 90° CW (Camera on Right) -> Rotate UI -90° CCW
			root_rotator.rotation_degrees = -90.0
			root_rotator.size = Vector2(vp_sz.y, vp_sz.x)
			root_rotator.pivot_offset = root_rotator.size * 0.5
			root_rotator.position = center - root_rotator.pivot_offset
		180:
			root_rotator.rotation_degrees = 180.0
			root_rotator.size = vp_sz
			root_rotator.pivot_offset = vp_sz * 0.5
			root_rotator.position = center - root_rotator.pivot_offset
		270:
			# Laptop rotated 90° CCW (Camera on Left) -> Rotate UI +90° CW
			root_rotator.rotation_degrees = 90.0
			root_rotator.size = Vector2(vp_sz.y, vp_sz.x)
			root_rotator.pivot_offset = root_rotator.size * 0.5
			root_rotator.position = center - root_rotator.pivot_offset

	_position_target_marker()

func _position_target_marker() -> void:
	if current_cue_idx >= CUES.size():
		target_marker.visible = false
		return

	target_marker.visible = true
	var cue = CUES[current_cue_idx]
	var rot_sz = root_rotator.size
	var uv: Vector2 = cue["target_uv"]
	target_marker.position = (uv * rot_sz) - (target_marker.size * 0.5)

func _update_ui() -> void:
	if current_cue_idx >= CUES.size():
		_finish_capture()
		return

	var cue = CUES[current_cue_idx]
	header_label.text = "[%d/%d] %s" % [current_cue_idx + 1, CUES.size(), cue["title"]]
	head_label.text = "1. HEAD: %s" % cue["head_inst"]
	eyes_label.text = "2. EYES: %s" % cue["eyes_inst"]
	status_label.text = "READY — Press [SPACE] to capture"
	status_label.modulate = Color(0.2, 0.9, 0.4)

	var orient_str = "90° CW (Camera on Right)"
	match current_orientation_deg:
		0: orient_str = "0° (Normal - Camera on Top)"
		180: orient_str = "180° (Inverted - Camera on Bottom)"
		270: orient_str = "270° CCW (Camera on Left)"

	controls_label.text = "Orientation: %s | Keys: [SPACE] Capture  [ESC/Bksp] Prev  [1-9] Jump  [F] Fullscreen  [R] Rotate UI" % orient_str

	_position_target_marker()

func _capture_current_cue() -> void:
	is_capturing = true
	var cue = CUES[current_cue_idx]
	status_label.text = "CAPTURING FRAME..."
	status_label.modulate = Color(1.0, 0.8, 0.2)
	target_marker.queue_redraw()

	# Wait a frame for clean video buffer
	await get_tree().process_frame

	var raw_img: Image = null
	if vs and cam_rid.is_valid():
		raw_img = vs.camera_get_current_image(cam_rid)

	if raw_img and not raw_img.is_empty():
		var res_dir = ProjectSettings.globalize_path("res://../tests/resources")
		var out_path = res_dir + "/" + cue["id"] + ".jpg"
		var err = raw_img.save_jpg(out_path, 0.95)
		if err == OK:
			print("[Capture] Successfully saved: ", out_path, " (%dx%d)" % [raw_img.get_width(), raw_img.get_height()])
			status_label.text = "SAVED: %s.jpg" % cue["id"]
			status_label.modulate = Color(0.2, 1.0, 0.4)
		else:
			printerr("[Capture] Failed to save image: ", err)
	else:
		printerr("[Capture] No frame available from camera!")

	await get_tree().create_timer(0.35).timeout

	current_cue_idx += 1
	is_capturing = false

	if current_cue_idx >= CUES.size():
		_finish_capture()
	else:
		_update_ui()

func _finish_capture() -> void:
	target_marker.visible = false
	header_label.text = "ALL 9 FIXTURES CAPTURED SUCCESSFULLY!"
	head_label.text = "Saved to tests/resources/landscape_*.jpg"
	eyes_label.text = "Press [1-9] to redo any specific fixture, or close this window."
	status_label.text = "COMPLETE"
	status_label.modulate = Color(0.2, 1.0, 0.4)

func _on_target_marker_draw() -> void:
	var sz = target_marker.size
	var center = sz * 0.5
	var base_radius = 24.0

	# Outer breathing ring
	var pulse_radius = base_radius + sin(pulse_time) * 4.0
	var ring_col = Color(0.1, 0.95, 0.45, 0.6) if not is_capturing else Color(1.0, 0.3, 0.2, 0.8)
	target_marker.draw_arc(center, pulse_radius, 0, TAU, 32, ring_col, 3.0, true)

	# Main bullseye ring
	var main_col = Color(0.0, 1.0, 0.5, 1.0) if not is_capturing else Color(1.0, 0.2, 0.1, 1.0)
	target_marker.draw_circle(center, 12.0, main_col)
	target_marker.draw_circle(center, 4.0, Color.BLACK)

	# Crosshairs
	target_marker.draw_line(center + Vector2(-18, 0), center + Vector2(18, 0), Color.WHITE, 1.5)
	target_marker.draw_line(center + Vector2(0, -18), center + Vector2(0, 18), Color.WHITE, 1.5)

func _on_silhouette_draw() -> void:
	var sz = silhouette_overlay.size
	var center = sz * 0.5

	# Head oval guide
	var head_rect = Rect2(center.x - 70, center.y - 95, 140, 190)
	silhouette_overlay.draw_rect(head_rect, Color(0.3, 0.7, 1.0, 0.25), false, 2.0)

	# Eye line guide
	silhouette_overlay.draw_line(Vector2(center.x - 60, center.y - 20), Vector2(center.x + 60, center.y - 20), Color(0.3, 1.0, 0.8, 0.4), 1.5)
	# Center vertical guide
	silhouette_overlay.draw_line(Vector2(center.x, center.y - 85), Vector2(center.x, center.y + 85), Color(0.3, 1.0, 0.8, 0.3), 1.0)

