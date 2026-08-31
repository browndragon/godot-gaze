@tool
class_name DebugCamFeed
extends Control

var camera_feed_texture: ImageTexture = null
var left_eye_texture: ImageTexture = null
var right_eye_texture: ImageTexture = null

var actual_cam_width: int = 0
var actual_cam_height: int = 0

var update_accumulator: float = 0.0
const UPDATE_INTERVAL: float = 0.15

var landmark_overlay: Control = null
var active_canvas: CanvasItem = null
var _active_preview_requested: bool = false

func _ready():
	var copy_btn = get_node_or_null("Panel/CopyButton")
	if copy_btn and not copy_btn.pressed.is_connected(_on_copy_button_pressed):
		copy_btn.pressed.connect(_on_copy_button_pressed)

	landmark_overlay = Control.new()
	landmark_overlay.name = "LandmarkOverlay"
	landmark_overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	landmark_overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	landmark_overlay.draw.connect(_on_overlay_draw)
	add_child(landmark_overlay)

	if not Engine.is_editor_hint():
		var gs = Engine.get_singleton("GazeServer")
		if gs:
			print("[DebugHUD] Connected to GazeServer singleton")
		else:
			print("[DebugHUD] WARNING: GazeServer singleton not found!")

func _notification(what: int) -> void:
	if what == NOTIFICATION_VISIBILITY_CHANGED or what == NOTIFICATION_ENTER_TREE:
		_update_preview_state()
	elif what == NOTIFICATION_EXIT_TREE:
		_release_preview_state()

func _update_preview_state() -> void:
	if Engine.is_editor_hint(): return
	var should_preview = is_visible_in_tree() if is_inside_tree() else visible
	if should_preview != _active_preview_requested:
		_active_preview_requested = should_preview
		var gs = Engine.get_singleton("GazeServer")
		if gs and gs.has_method("camera_set_preview_requested"):
			gs.camera_set_preview_requested(should_preview)

func _release_preview_state() -> void:
	if _active_preview_requested:
		_active_preview_requested = false
		var gs = Engine.get_singleton("GazeServer")
		if gs and gs.has_method("camera_set_preview_requested"):
			gs.camera_set_preview_requested(false)

func _exit_tree():
	_release_preview_state()

func _process(delta: float) -> void:
	if not Engine.is_editor_hint():
		var gs = Engine.get_singleton("GazeServer")
		if gs:
			var tex = gs.get_camera_texture()
			if tex:
				actual_cam_width = tex.get_width()
				actual_cam_height = tex.get_height()
				var rect = get_texture_rect("CameraFeedRect")
				if rect:
					rect.texture = tex

			var crops = gs.get_eye_crops()
			if crops and crops.size() >= 2:
				var left_img = crops[0]
				if left_img and not left_img.is_empty():
					if left_eye_texture == null or left_eye_texture.get_size() != Vector2(left_img.get_size()):
						left_eye_texture = ImageTexture.create_from_image(left_img)
					else:
						left_eye_texture.update(left_img)
					var left_rect = get_texture_rect("LeftEyeRect")
					if left_rect:
						left_rect.texture = left_eye_texture

				var right_img = crops[1]
				if right_img and not right_img.is_empty():
					if right_eye_texture == null or right_eye_texture.get_size() != Vector2(right_img.get_size()):
						right_eye_texture = ImageTexture.create_from_image(right_img)
					else:
						right_eye_texture.update(right_img)
					var right_rect = get_texture_rect("RightEyeRect")
					if right_rect:
						right_rect.texture = right_eye_texture

			if landmark_overlay:
				landmark_overlay.queue_redraw()

		update_accumulator += delta
		if update_accumulator >= UPDATE_INTERVAL:
			update_accumulator = 0.0
			update_diagnostics_ui()

func get_texture_rect(node_name: String) -> TextureRect:
	var node = get_node_or_null(node_name)
	if node and node is TextureRect:
		return node
	node = get_node_or_null("Panel/" + node_name)
	if node and node is TextureRect:
		return node
	return find_node_by_name_and_type(self, node_name, "TextureRect") as TextureRect

func find_node_by_name_and_type(parent: Node, node_name: String, type_name: String) -> Node:
	if parent.name == node_name and parent.is_class(type_name):
		return parent
	for child in parent.get_children():
		var found = find_node_by_name_and_type(child, node_name, type_name)
		if found:
			return found
	return null

func get_texture_drawn_rect(rect: TextureRect) -> Rect2:
	if not rect or not rect.texture:
		return Rect2()
	var tex_size = rect.texture.get_size()
	var rect_size = rect.size
	if tex_size.x == 0 or tex_size.y == 0 or rect_size.x == 0 or rect_size.y == 0:
		return Rect2()
	if rect.stretch_mode == TextureRect.STRETCH_KEEP_ASPECT_CENTERED:
		var tex_ratio = tex_size.x / tex_size.y
		var rect_ratio = rect_size.x / rect_size.y
		var drawn_w = 0.0
		var drawn_h = 0.0
		var offset_x = 0.0
		var offset_y = 0.0
		if rect_ratio > tex_ratio:
			drawn_h = rect_size.y
			drawn_w = drawn_h * tex_ratio
			offset_x = (rect_size.x - drawn_w) / 2.0
		else:
			drawn_w = rect_size.x
			drawn_h = drawn_w / tex_ratio
			offset_y = (rect_size.y - drawn_h) / 2.0
		return Rect2(offset_x, offset_y, drawn_w, drawn_h)
	elif rect.stretch_mode == TextureRect.STRETCH_KEEP_ASPECT_COVERED:
		var tex_ratio = tex_size.x / tex_size.y
		var rect_ratio = rect_size.x / rect_size.y
		var drawn_w = 0.0
		var drawn_h = 0.0
		var offset_x = 0.0
		var offset_y = 0.0
		if rect_ratio > tex_ratio:
			drawn_w = rect_size.x
			drawn_h = drawn_w / tex_ratio
			offset_y = (rect_size.y - drawn_h) / 2.0
		else:
			drawn_h = rect_size.y
			drawn_w = drawn_h * tex_ratio
			offset_x = (rect_size.x - drawn_w) / 2.0
		return Rect2(offset_x, offset_y, drawn_w, drawn_h)
	return Rect2(0, 0, rect_size.x, rect_size.y)

func update_diagnostics_ui() -> void:
	var metrics_lbl = get_node_or_null("Panel/ScrollContainer/VBoxContainer/MetricsBox/MetricsLabel")
	if not metrics_lbl:
		return

	var lines = []
	var gs = Engine.get_singleton("GazeServer")
	if gs:
		var ev = gs.get_most_recent_event()
		var is_face_detected = (ev is InputEventGaze and ev.is_face_tracked())
		var is_running = gs.is_tracking_active()

		var build_info = GazeServer.get_build_info() if ClassDB.class_has_method("GazeServer", "get_build_info") else ""
		if build_info != "":
			lines.append("Build: [color=aqua]%s[/color]" % build_info)
		lines.append("Tracker State: [color=%s]%s[/color]" % ["green" if is_running else "gray", "Running" if is_running else "Idle"])
		lines.append("Face Tracked: %s" % ("[color=green]YES[/color]" if is_face_detected else "[color=red]NO[/color]"))

		if is_face_detected:
			var head_pos = ev.head_transform.origin
			var head_rot = ev.head_transform.basis.get_euler() * (180.0 / PI)
			var head_fwd = -ev.head_transform.basis.z.normalized()
			var eye_orig = ev.gaze_transform.origin
			var gaze_dir = -ev.gaze_transform.basis.z.normalized()
			lines.append("Head Trans (mm): [color=yellow](%.1f, %.1f, %.1f)[/color]" % [head_pos.x, head_pos.y, head_pos.z])
			lines.append("Head Rot (deg): [color=yellow](P:%.1f, Y:%.1f, R:%.1f)[/color]" % [head_rot.x, head_rot.y, -head_rot.z])
			lines.append("Head Forward: [color=yellow](%.3f, %.3f, %.3f)[/color]" % [head_fwd.x, head_fwd.y, head_fwd.z])
			lines.append("Eye Origin (mm): [color=yellow](%.1f, %.1f, %.1f)[/color]" % [eye_orig.x, eye_orig.y, eye_orig.z])
			lines.append("Gaze Direction: [color=yellow](%.3f, %.3f, %.3f)[/color]" % [gaze_dir.x, gaze_dir.y, gaze_dir.z])
			lines.append("Eye Openness: [color=yellow]L: %.2f, R: %.2f[/color]" % [ev.left_eye_openness, ev.right_eye_openness])
		else:
			lines.append("Head Trans (mm): [color=gray]N/A[/color]")
			lines.append("Head Rot (deg): [color=gray]N/A[/color]")
			lines.append("Gaze Direction: [color=gray]N/A[/color]")

		if ev:
			var domain_lines = []
			if "biorhythm_index" in ev:
				domain_lines.append("Biorhythm: [color=yellow]%.2f[/color]" % ev.biorhythm_index)
			if "saccade_meter" in ev:
				domain_lines.append("Saccade: [color=yellow]%.2f[/color]" % ev.saccade_meter)
			if "distance_meter" in ev:
				domain_lines.append("Distance: [color=yellow]%.2f[/color]" % ev.distance_meter)
			if "eye_state_flags" in ev:
				domain_lines.append("Eye Flags: [color=yellow]0x%X[/color]" % ev.eye_state_flags)
			if domain_lines.size() > 0:
				lines.append("\n[b]Event Domain Telemetry:[/b]")
				lines.append("  " + " | ".join(domain_lines))

		var dev_cal = gs.get_device_calibration()
		var bio_cal = gs.get_bio_calibration()
		lines.append("\n[b]Calibration & Geometry:[/b]")
		lines.append("  Device Cal: [color=aqua]%s[/color]" % (dev_cal.get_class() if dev_cal else "Guess (Fallback)"))
		if dev_cal:
			var phys_mm = dev_cal.get_physical_size_mm()
			var log_px = dev_cal.get_logical_size_px()
			var w_pos = dev_cal.get_window_position()
			var c_off = dev_cal.get_camera_offset()
			lines.append("  Screen Size: [color=yellow]%.1fx%.1f mm ( %dx%d px )[/color]" % [phys_mm.x, phys_mm.y, log_px.x, log_px.y])
			lines.append("  Window Pos: [color=yellow](%.1f, %.1f)[/color]" % [w_pos.x, w_pos.y])
			lines.append("  Cam Offset: [color=yellow](%.1f, %.1f, %.1f) mm[/color]" % [c_off.x, c_off.y, c_off.z])
		lines.append("  User Bio Cal: [color=aqua]%s[/color]" % (bio_cal.get_class() if bio_cal else "Default (No adjustment)"))

		lines.append("\n[b]Camera Feed:[/b]")
		lines.append("  Resolution: [color=yellow]%dx%d[/color]" % [actual_cam_width, actual_cam_height])
	else:
		metrics_lbl.text = "[color=red]GazeServer not found.[/color]"
		return

	lines.append("\n[b]Environment Details:[/b]")
	var dpi_val = DisplayServer.screen_get_dpi() if Engine.has_singleton("DisplayServer") else 96
	var scr_scale = DisplayServer.screen_get_scale() if Engine.has_singleton("DisplayServer") else 1.0
	lines.append("  Screen DPI: [color=yellow]%d[/color]" % dpi_val)
	lines.append("  Device Scale: [color=yellow]%.2f[/color]" % scr_scale)

	var new_text = "\n".join(lines)
	if metrics_lbl.text != new_text:
		metrics_lbl.text = new_text

func _on_copy_button_pressed():
	var copy_btn = get_node_or_null("Panel/CopyButton")
	var data = {
		"timestamp": Time.get_datetime_string_from_system(true),
		"gaze_tracker_active": false,
		"face_detected": false,
		"gaze_origin_mm": null,
		"gaze_direction": null,
		"head_translation_mm": null,
		"head_rotation_deg": null,
		"head_forward": null,
		"device_calibration": null,
		"screen_physical_mm": null,
		"screen_logical_px": null,
		"window_position_px": null,
		"camera_offset_mm": null,
		"bio_calibration": null,
		"screen_dpi": DisplayServer.screen_get_dpi() if Engine.has_singleton("DisplayServer") else 96,
		"device_scale": DisplayServer.screen_get_scale() if Engine.has_singleton("DisplayServer") else 1.0,
		"camera_width_height": "%dx%d" % [actual_cam_width, actual_cam_height]
	}

	var gs = Engine.get_singleton("GazeServer")
	if gs:
		data["gaze_tracker_active"] = gs.is_tracking_active()
		var dev_cal = gs.get_device_calibration()
		var bio_cal = gs.get_bio_calibration()
		data["device_calibration"] = dev_cal.get_class() if dev_cal else "Null"
		if dev_cal:
			data["screen_physical_mm"] = [dev_cal.get_physical_size_mm().x, dev_cal.get_physical_size_mm().y]
			data["screen_logical_px"] = [dev_cal.get_logical_size_px().x, dev_cal.get_logical_size_px().y]
			data["window_position_px"] = [dev_cal.get_window_position().x, dev_cal.get_window_position().y]
			data["camera_offset_mm"] = [dev_cal.get_camera_offset().x, dev_cal.get_camera_offset().y, dev_cal.get_camera_offset().z]
		data["bio_calibration"] = bio_cal.get_class() if bio_cal else "Null"
		var ev = gs.get_most_recent_event()
		if ev is InputEventGaze and ev.is_face_tracked():
			data["face_detected"] = true
			data["head_translation_mm"] = [ev.head_transform.origin.x, ev.head_transform.origin.y, ev.head_transform.origin.z]
			var rot = ev.head_transform.basis.get_euler() * (180.0 / PI)
			data["head_rotation_deg"] = [rot.x, rot.y, rot.z]
			var head_fwd = -ev.head_transform.basis.z.normalized()
			data["head_forward"] = [head_fwd.x, head_fwd.y, head_fwd.z]
			var eye_orig = ev.gaze_transform.origin
			data["gaze_origin_mm"] = [eye_orig.x, eye_orig.y, eye_orig.z]
			var gaze_dir = -ev.gaze_transform.basis.z.normalized()
			data["gaze_direction"] = [gaze_dir.x, gaze_dir.y, gaze_dir.z]

	if Engine.has_singleton("DisplayServer"):
		DisplayServer.clipboard_set(JSON.stringify(data, "  "))
	if copy_btn:
		copy_btn.text = "✅ Copied!"
		await get_tree().create_timer(2.0).timeout
		if is_instance_valid(copy_btn):
			copy_btn.text = "📋 Copy Diagnostics"

func _draw():
	active_canvas = self
	_perform_drawing()
	active_canvas = null

func _on_overlay_draw():
	if landmark_overlay:
		active_canvas = landmark_overlay
		_perform_drawing()
		active_canvas = null

func _perform_drawing():
	var rect = get_texture_rect("CameraFeedRect")
	if not rect or rect.texture == null:
		return

	var drawn_rect = get_texture_drawn_rect(rect)
	var tex_size = rect.texture.get_size()
	var img_w = tex_size.x
	var img_h = tex_size.y
	if img_w <= 0 or img_h <= 0:
		return

	var focal_len = img_w / (2.0 * tan(deg_to_rad(65.0) * 0.5))
	var cx = img_w / 2.0
	var cy = img_h / 2.0

	var gs = Engine.get_singleton("GazeServer")
	if not gs: return
	var ev = gs.get_most_recent_event()
	if not (ev is InputEventGaze) or not ev.is_face_tracked(): return
	var xform = ev.head_transform
	var landmarks_2d = gs.get_face_landmarks()
	var raw_gaze = -ev.gaze_transform.basis.z

	if abs(xform.origin.z) <= 0.01:
		return

	var drawn_pts = []
	if not landmarks_2d.is_empty() and landmarks_2d.size() == 35:
		for pt_px in landmarks_2d:
			var local_pt = Vector2(pt_px.x * drawn_rect.size.x / img_w, pt_px.y * drawn_rect.size.y / img_h) + drawn_rect.position
			var screen_pt = rect.global_position + local_pt - active_canvas.global_position
			drawn_pts.append(screen_pt)

	# Draws landmark points (Cyan)
	for pt in drawn_pts:
		if pt != Vector2.INF:
			gd_draw_circle(pt, 3.5, Color(0.0, 0.85, 1.0, 0.95))

	# Connect 35-point facial landmark wireframe
	var connections = []
	if drawn_pts.size() >= 35:
		for i in range(18, 34):
			connections.append([i, i + 1])
		connections.append_array([[12, 13], [13, 14], [15, 16], [16, 17]])
		connections.append_array([[4, 5], [6, 7], [4, 6], [4, 7]])
		connections.append_array([[8, 10], [10, 9], [9, 11], [11, 8]])
		connections.append_array([[0, 1], [2, 3]])

	for conn in connections:
		if conn[0] < drawn_pts.size() and conn[1] < drawn_pts.size():
			var p1 = drawn_pts[conn[0]]
			var p2 = drawn_pts[conn[1]]
			if p1 != Vector2.INF and p2 != Vector2.INF:
				gd_draw_line(p1, p2, Color(0.0, 0.85, 1.0, 0.75), 2.0)

	var cam_to_screen = func(p_cam: Vector3) -> Vector2:
		var depth = -p_cam.z
		if depth <= 0.01:
			return Vector2.INF
		var px = (p_cam.x / depth) * focal_len + cx
		var py = cy - (p_cam.y / depth) * focal_len
		var local_pt = Vector2(px * drawn_rect.size.x / img_w, py * drawn_rect.size.y / img_h) + drawn_rect.position
		return rect.global_position + local_pt - active_canvas.global_position

	# Nose 3-axis indicator
	var nose_origin_3d = xform.origin
	var head_axis_len = 100.0
	var nose_x_3d = nose_origin_3d + xform.basis.x * head_axis_len
	var nose_y_3d = nose_origin_3d + xform.basis.y * head_axis_len
	var nose_fwd_3d = nose_origin_3d - xform.basis.z * head_axis_len

	var pt_nose_org = cam_to_screen.call(nose_origin_3d)
	var pt_nose_x = cam_to_screen.call(nose_x_3d)
	var pt_nose_y = cam_to_screen.call(nose_y_3d)
	var pt_nose_fwd = cam_to_screen.call(nose_fwd_3d)

	if pt_nose_org != Vector2.INF:
		if pt_nose_x != Vector2.INF: gd_draw_line(pt_nose_org, pt_nose_x, Color(1.0, 0.2, 0.2, 0.35), 2.0)
		if pt_nose_y != Vector2.INF: gd_draw_line(pt_nose_org, pt_nose_y, Color(0.2, 1.0, 0.2, 0.35), 2.0)
		if pt_nose_fwd != Vector2.INF:
			gd_draw_line(pt_nose_org, pt_nose_fwd, Color(0.0, 0.95, 1.0, 1.0), 3.5)

	# Eye Gaze indicator
	var eye_origin_3d = ev.gaze_transform.origin
	if raw_gaze.length_squared() < 0.001:
		raw_gaze = Vector3(0.0, 0.0, 1.0)
	var eye_fwd = raw_gaze.normalized()

	var eye_axis_len = 100.0
	var eye_fwd_3d = eye_origin_3d + eye_fwd * eye_axis_len
	var pt_eye_org = cam_to_screen.call(eye_origin_3d)
	var pt_eye_fwd = cam_to_screen.call(eye_fwd_3d)

	if pt_eye_org != Vector2.INF and pt_eye_fwd != Vector2.INF:
		gd_draw_line(pt_eye_org, pt_eye_fwd, Color(0.2, 1.0, 0.1, 1.0), 3.5)

func gd_draw_circle(pos: Vector2, radius: float, color: Color):
	if active_canvas:
		active_canvas.draw_circle(pos, radius, color)

func gd_draw_line(from: Vector2, to: Vector2, color: Color, width: float):
	if active_canvas:
		active_canvas.draw_line(from, to, color, width)
