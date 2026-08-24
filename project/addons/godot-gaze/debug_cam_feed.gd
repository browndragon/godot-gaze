@tool
extends Control

@export var tracker: Node = null:
	set(val):
		if tracker != val:
			disconnect_tracker_signals()
			tracker = val
			if tracker:
				connect_tracker_signals()

const DEFAULT_FOCAL_TO_WIDTH_RATIO = 1.5625

var camera_sensor: Node = null
var eye_estimator: Node = null
var face_estimator: Node = null

var camera_feed_texture: ImageTexture = null
var left_eye_texture: ImageTexture = null
var right_eye_texture: ImageTexture = null

var actual_cam_width: int = 0
var actual_cam_height: int = 0

var update_accumulator: float = 0.0
const UPDATE_INTERVAL: float = 0.15

var landmark_overlay: Control = null
var active_canvas: CanvasItem = null

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
		if is_inside_tree() and not is_instance_valid(tracker):
			tracker = find_gaze_tracker(get_tree().root)
		if is_instance_valid(tracker):
			connect_tracker_signals()
			print("[DebugHUD] GazeTracker connected: ", tracker)
			print("[DebugHUD] Camera sensor: ", camera_sensor)
			print("[DebugHUD] Face estimator: ", face_estimator)
		else:
			print("[DebugHUD] WARNING: GazeTracker NOT found in scene tree!")

var preview_requested: bool = false
var crop_requested: bool = false

func _process(delta):
	if not Engine.is_editor_hint():
		if not is_instance_valid(tracker) and is_inside_tree():
			disconnect_tracker_signals()
			tracker = find_gaze_tracker(get_tree().root)
		if is_instance_valid(tracker):
			var t_camera_sensor = tracker.call("get_camera_sensor") if tracker.has_method("get_camera_sensor") else null
			var t_eye_estimator = tracker.call("get_eye_estimator") if tracker.has_method("get_eye_estimator") else null
			var t_face_estimator = tracker.call("get_face_estimator") if tracker.has_method("get_face_estimator") else null
			
			var current_cam_valid = is_instance_valid(camera_sensor)
			var current_eye_valid = is_instance_valid(eye_estimator)
			var current_face_valid = is_instance_valid(face_estimator)
			
			var target_cam_valid = is_instance_valid(t_camera_sensor)
			var target_eye_valid = is_instance_valid(t_eye_estimator)
			var target_face_valid = is_instance_valid(t_face_estimator)
			
			var is_different = false
			if current_cam_valid != target_cam_valid or (current_cam_valid and camera_sensor != t_camera_sensor):
				is_different = true
			elif current_eye_valid != target_eye_valid or (current_eye_valid and eye_estimator != t_eye_estimator):
				is_different = true
			elif current_face_valid != target_face_valid or (current_face_valid and face_estimator != t_face_estimator):
				is_different = true
			elif current_cam_valid and not preview_requested:
				is_different = true
			elif current_eye_valid and not crop_requested:
				is_different = true
				
			if is_different:
				disconnect_tracker_signals()
				connect_tracker_signals()

			update_accumulator += delta
			if update_accumulator >= UPDATE_INTERVAL:
				update_accumulator = 0.0
				update_diagnostics_ui()

func _exit_tree():
	disconnect_tracker_signals()

func find_gaze_tracker(node: Node) -> Node:
	if node == null:
		return null
	if node.get_class() == "GazeTracker" or node.has_method("get_camera_sensor"):
		return node
	for child in node.get_children():
		var found = find_gaze_tracker(child)
		if found:
			return found
	return null

func disconnect_tracker_signals():
	if is_instance_valid(camera_sensor):
		if camera_sensor.is_connected("frame_ready", _on_frame_ready):
			camera_sensor.disconnect("frame_ready", _on_frame_ready)
		var cam_rid = camera_sensor.call("get_camera_rid") if camera_sensor.has_method("get_camera_rid") else RID()
		if cam_rid.is_valid():
			var vs = Engine.get_singleton("VisionServer")
			if vs:
				vs.camera_set_preview_requested(cam_rid, false)
	if is_instance_valid(eye_estimator):
		if eye_estimator.is_connected("eye_crops_ready", _on_eye_crops_ready):
			eye_estimator.disconnect("eye_crops_ready", _on_eye_crops_ready)
		if eye_estimator.is_connected("gaze_estimated", _on_gaze_estimated):
			eye_estimator.disconnect("gaze_estimated", _on_gaze_estimated)
		var eye_rid = eye_estimator.call("get_eye_rid") if eye_estimator.has_method("get_eye_rid") else RID()
		if eye_rid.is_valid():
			var gs = Engine.get_singleton("GazeServer")
			if gs:
				gs.eye_tracker_set_crop_requested(eye_rid, false)
	
	preview_requested = false
	crop_requested = false
	camera_sensor = null
	eye_estimator = null
	face_estimator = null

func connect_tracker_signals():
	if not is_instance_valid(tracker):
		return
	camera_sensor = tracker.call("get_camera_sensor") if tracker.has_method("get_camera_sensor") else null
	eye_estimator = tracker.call("get_eye_estimator") if tracker.has_method("get_eye_estimator") else null
	face_estimator = tracker.call("get_face_estimator") if tracker.has_method("get_face_estimator") else null
	
	if is_instance_valid(camera_sensor):
		if not camera_sensor.is_connected("frame_ready", _on_frame_ready):
			camera_sensor.connect("frame_ready", _on_frame_ready)
		var cam_rid = camera_sensor.call("get_camera_rid") if camera_sensor.has_method("get_camera_rid") else RID()
		if cam_rid.is_valid():
			var vs = Engine.get_singleton("VisionServer")
			if vs:
				vs.camera_set_preview_requested(cam_rid, true)
				preview_requested = true
	if is_instance_valid(eye_estimator):
		if not eye_estimator.is_connected("eye_crops_ready", _on_eye_crops_ready):
			eye_estimator.connect("eye_crops_ready", _on_eye_crops_ready)
		if not eye_estimator.is_connected("gaze_estimated", _on_gaze_estimated):
			eye_estimator.connect("gaze_estimated", _on_gaze_estimated)
		var eye_rid = eye_estimator.call("get_eye_rid") if eye_estimator.has_method("get_eye_rid") else RID()
		if eye_rid.is_valid():
			var gs = Engine.get_singleton("GazeServer")
			if gs:
				gs.eye_tracker_set_crop_requested(eye_rid, true)
				crop_requested = true

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

func _on_frame_ready(img: Image):
	if not img or img.is_empty():
		return
	actual_cam_width = img.get_width()
	actual_cam_height = img.get_height()
	
	if camera_feed_texture == null or camera_feed_texture.get_size() != Vector2(img.get_size()):
		camera_feed_texture = ImageTexture.create_from_image(img)
	else:
		camera_feed_texture.update(img)
	
	var rect = get_texture_rect("CameraFeedRect")
	if rect:
		rect.texture = camera_feed_texture
	
	if landmark_overlay:
		landmark_overlay.queue_redraw()

func _on_eye_crops_ready(left: Image, right: Image):
	if left and not left.is_empty():
		if left_eye_texture == null or left_eye_texture.get_size() != Vector2(left.get_size()):
			left_eye_texture = ImageTexture.create_from_image(left)
		else:
			left_eye_texture.update(left)
		var rect = get_texture_rect("LeftEyeRect")
		if rect:
			rect.texture = left_eye_texture

	if right and not right.is_empty():
		if right_eye_texture == null or right_eye_texture.get_size() != Vector2(right.get_size()):
			right_eye_texture = ImageTexture.create_from_image(right)
		else:
			right_eye_texture.update(right)
		var rect = get_texture_rect("RightEyeRect")
		if rect:
			rect.texture = right_eye_texture

func _on_gaze_estimated():
	if landmark_overlay:
		landmark_overlay.queue_redraw()

func update_diagnostics_ui():
	var metrics_lbl = get_node_or_null("Panel/ScrollContainer/VBoxContainer/MetricsBox/MetricsLabel")
	if not metrics_lbl:
		return
	
	var lines = []
	if not is_instance_valid(tracker):
		metrics_lbl.text = "[color=red]GazeTracker not found in scene tree.[/color]"
		return
	
	var is_face_detected = false
	var head_pos = Vector3.ZERO
	var head_rot = Vector3.ZERO
	if is_instance_valid(face_estimator):
		is_face_detected = face_estimator.get("has_detected_face") == true
		if is_face_detected:
			var xform = face_estimator.get("transform")
			if xform:
				head_pos = xform.origin
				head_rot = xform.basis.get_euler() * (180.0 / PI)

	var gaze_dir = tracker.call("get_gaze_direction") if tracker.has_method("get_gaze_direction") else Vector3.ZERO
	var dev_cal = tracker.get("device_calibration")
	var bio_cal = tracker.get("bio_calibration")

	# Formatting
	var state_names = {
		0: "Unknown",
		1: "Permission Required",
		2: "Initializing",
		3: "Running",
		4: "Error"
	}
	var state_colors = {
		0: "gray",
		1: "orange",
		2: "yellow",
		3: "green",
		4: "red"
	}
	var lifecycle_val = tracker.call("get_lifecycle_state") if tracker.has_method("get_lifecycle_state") else 0
	var state_name = state_names.get(lifecycle_val, "Unknown")
	var state_color = state_colors.get(lifecycle_val, "gray")
	
	var build_info = tracker.call("get_build_info") if tracker.has_method("get_build_info") else ""
	if build_info != "":
		lines.append("Build: [color=aqua]%s[/color]" % build_info)
	lines.append("Tracker State: [color=%s]%s[/color]" % [state_color, state_name])
	lines.append("Face Tracked: %s" % ("[color=green]YES[/color]" if is_face_detected else "[color=red]NO[/color]"))
	
	if is_face_detected:
		lines.append("Head Trans (mm): [color=yellow](%.1f, %.1f, %.1f)[/color]" % [head_pos.x, head_pos.y, head_pos.z])
		lines.append("Head Rot (deg): [color=yellow](P:%.1f, Y:%.1f, R:%.1f)[/color]" % [head_rot.x, head_rot.y, -head_rot.z])
	else:
		lines.append("Head Trans (mm): [color=gray]N/A[/color]")
		lines.append("Head Rot (deg): [color=gray]N/A[/color]")

	lines.append("Gaze Direction: [color=yellow](%.3f, %.3f, %.3f)[/color]" % [gaze_dir.x, gaze_dir.y, gaze_dir.z])
	
	lines.append("\n[b]Calibration Status:[/b]")
	lines.append("  Device Cal: [color=aqua]%s[/color]" % (dev_cal.get_class() if dev_cal else "Guess (Fallback)"))
	lines.append("  User Bio Cal: [color=aqua]%s[/color]" % (bio_cal.get_class() if bio_cal else "Default (No adjustment)"))

	if is_instance_valid(camera_sensor):
		var focal = camera_sensor.get("focal_length")
		var fov = camera_sensor.get("camera_fov")
		if fov == null:
			fov = 35.488537576579634
		var display_focal = focal
		if focal == null or focal <= 0.0:
			var w = actual_cam_width if actual_cam_width > 0 else 640
			display_focal = w / (2.0 * tan(fov * PI / 180.0 * 0.5))
		lines.append("\n[b]Camera Feed:[/b]")
		lines.append("  Resolution: [color=yellow]%dx%d[/color]" % [actual_cam_width, actual_cam_height])
		lines.append("  Focal Length: [color=yellow]%.1f px%s[/color]" % [display_focal, " (Auto)" if focal == null or focal <= 0.0 else ""])

	lines.append("\n[b]Environment Details:[/b]")
	lines.append("  Screen DPI: [color=yellow]%d[/color]" % DisplayServer.screen_get_dpi())
	lines.append("  Device Scale: [color=yellow]%.2f[/color]" % (1.0 if OS.get_name() != "macOS" else 2.0))

	var new_text = "\n".join(lines)
	if metrics_lbl.text != new_text:
		metrics_lbl.text = new_text

func _on_copy_button_pressed():
	var copy_btn = get_node_or_null("Panel/CopyButton")
	var data = {
		"timestamp": Time.get_datetime_string_from_system(true),
		"gaze_tracker_active": is_instance_valid(tracker),
		"face_detected": false,
		"gaze_direction": null,
		"head_translation_mm": null,
		"head_rotation_deg": null,
		"device_calibration": null,
		"bio_calibration": null,
		"screen_dpi": DisplayServer.screen_get_dpi(),
		"camera_width_height": null,
		"camera_focal_length": null
	}
	
	if is_instance_valid(tracker):
		data["device_calibration"] = tracker.get("device_calibration").get_class() if tracker.get("device_calibration") else "Null"
		data["bio_calibration"] = tracker.get("bio_calibration").get_class() if tracker.get("bio_calibration") else "Null"
		
		if is_instance_valid(camera_sensor):
			data["camera_width_height"] = "%dx%d" % [actual_cam_width, actual_cam_height]
			data["camera_focal_length"] = camera_sensor.get("focal_length")
		
		if is_instance_valid(face_estimator):
			var detected = face_estimator.get("has_detected_face") == true
			data["face_detected"] = detected
			if detected:
				var xform = face_estimator.get("transform")
				if xform:
					data["head_translation_mm"] = [xform.origin.x, xform.origin.y, xform.origin.z]
					var rot = xform.basis.get_euler() * (180.0 / PI)
					data["head_rotation_deg"] = [rot.x, rot.y, rot.z]
		
		var gaze_dir = tracker.call("get_gaze_direction") if tracker.has_method("get_gaze_direction") else null
		if gaze_dir:
			data["gaze_direction"] = [gaze_dir.x, gaze_dir.y, gaze_dir.z]

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
	if not is_instance_valid(tracker):
		return
	if not is_instance_valid(face_estimator) or not is_instance_valid(camera_sensor):
		face_estimator = tracker.call("get_face_estimator") if tracker.has_method("get_face_estimator") else null
		camera_sensor = tracker.call("get_camera_sensor") if tracker.has_method("get_camera_sensor") else null
	
	if not is_instance_valid(face_estimator) or not is_instance_valid(camera_sensor):
		return
		
	var has_face = face_estimator.get("has_detected_face")
	if has_face == null or not has_face:
		return

	var rect = get_texture_rect("CameraFeedRect")
	if not rect or rect.texture == null:
		return

	var drawn_rect = get_texture_drawn_rect(rect)

	var tex_size = rect.texture.get_size()
	var img_w = tex_size.x
	var img_h = tex_size.y
	if img_w <= 0 or img_h <= 0:
		return

	var focal_len = camera_sensor.get("focal_length") if is_instance_valid(camera_sensor) else -1.0
	if focal_len == null or focal_len <= 0.0:
		focal_len = img_w
		
	var cx = img_w / 2.0
	var cy = img_h / 2.0

	var xform = face_estimator.get("transform") if is_instance_valid(face_estimator) else null
	if xform == null:
		xform = Transform3D()

	if abs(xform.origin.z) <= 0.01:
		return

	# Authoritative canonical 3D Face Model Points exposed from C++ FaceModelGeometry / GazeServer
	var model_points = tracker.call("get_face_model_points") if tracker.has_method("get_face_model_points") else []
	if model_points.is_empty():
		return

	var landmarks_2d = tracker.call("get_face_landmarks_2d") if tracker.has_method("get_face_landmarks_2d") else []
	var drawn_pts = []

	if not landmarks_2d.is_empty() and landmarks_2d.size() == 35:
		for pt_px in landmarks_2d:
			var local_pt = Vector2(pt_px.x * drawn_rect.size.x / img_w, pt_px.y * drawn_rect.size.y / img_h) + drawn_rect.position
			var screen_pt = rect.global_position + local_pt - active_canvas.global_position
			drawn_pts.append(screen_pt)
	else:
		# Fallback to 3D projection if 2D landmarks not available
		for p_face in model_points:
			var p_cam = xform * p_face
			var depth = -p_cam.z
			if depth <= 0.01:
				drawn_pts.append(Vector2.INF)
			else:
				var px = (p_cam.x / depth) * focal_len + cx
				var py = cy - (p_cam.y / depth) * focal_len
				var local_pt = Vector2(px * drawn_rect.size.x / img_w, py * drawn_rect.size.y / img_h) + drawn_rect.position
				var screen_pt = rect.global_position + local_pt - active_canvas.global_position
				drawn_pts.append(screen_pt)

	# Draws high-fidelity landmark points (Cyan)
	for pt in drawn_pts:
		if pt != Vector2.INF:
			gd_draw_circle(pt, 3.5, Color(0.0, 0.85, 1.0, 0.95))

	# Connect 35-point facial landmark wireframe (jawline, eyebrows, nose, mouth, eyes)
	var connections = []
	if drawn_pts.size() >= 35:
		# Jawline contour (18..34)
		for i in range(18, 34):
			connections.append([i, i + 1])
		# Eyebrows (12..14, 15..17)
		connections.append_array([[12, 13], [13, 14], [15, 16], [16, 17]])
		# Nose ridge and wings (4..7)
		connections.append_array([[4, 5], [6, 7], [4, 6], [4, 7]])
		# Mouth contours (8..11)
		connections.append_array([[8, 10], [10, 9], [9, 11], [11, 8]])
		# Eye canthi (0..3)
		connections.append_array([[0, 1], [2, 3]])

	for conn in connections:
		if conn[0] < drawn_pts.size() and conn[1] < drawn_pts.size():
			var p1 = drawn_pts[conn[0]]
			var p2 = drawn_pts[conn[1]]
			if p1 != Vector2.INF and p2 != Vector2.INF:
				gd_draw_line(p1, p2, Color(0.0, 0.85, 1.0, 0.75), 2.0)

	# Helper to convert camera-space 3D point to overlay 2D screen coordinate
	var cam_to_screen = func(p_cam: Vector3) -> Vector2:
		if p_cam.z <= 0.01:
			return Vector2.INF
		var px = (p_cam.x / p_cam.z) * focal_len + cx
		var py = cy - (p_cam.y / p_cam.z) * focal_len
		var local_pt = Vector2(px * drawn_rect.size.x / img_w, py * drawn_rect.size.y / img_h) + drawn_rect.position
		return rect.global_position + local_pt - active_canvas.global_position

	# =========================================================================
	# 1. NOSE / HEAD POSE 3-AXIS ORIENTATION INDICATOR (~10cm in camera basis)
	# =========================================================================
	var nose_origin_3d = xform.origin
	var head_axis_len = 100.0 # 100 mm = 10 cm

	var nose_x_3d = nose_origin_3d + xform.basis.x * head_axis_len
	var nose_y_3d = nose_origin_3d + xform.basis.y * head_axis_len
	var nose_fwd_3d = nose_origin_3d - xform.basis.z * head_axis_len # Local -Z is head forward

	var pt_nose_org = cam_to_screen.call(nose_origin_3d)
	var pt_nose_x = cam_to_screen.call(nose_x_3d)
	var pt_nose_y = cam_to_screen.call(nose_y_3d)
	var pt_nose_fwd = cam_to_screen.call(nose_fwd_3d)

	if pt_nose_org != Vector2.INF:
		# Transparent +X axis (Red, alpha ~0.35)
		if pt_nose_x != Vector2.INF:
			gd_draw_line(pt_nose_org, pt_nose_x, Color(1.0, 0.2, 0.2, 0.35), 2.0)
		# Transparent +Y axis (Green, alpha ~0.35)
		if pt_nose_y != Vector2.INF:
			gd_draw_line(pt_nose_org, pt_nose_y, Color(0.2, 1.0, 0.2, 0.35), 2.0)
		# Saturated -Z Head Forward vector (Cyan, alpha 1.0, thick line + tip circle)
		if pt_nose_fwd != Vector2.INF:
			gd_draw_line(pt_nose_org, pt_nose_fwd, Color(0.0, 0.95, 1.0, 1.0), 3.5)
			gd_draw_circle(pt_nose_fwd, 4.5, Color(0.0, 0.95, 1.0, 1.0))

	# =========================================================================
	# 2. EYE GAZE 3-AXIS ORIENTATION INDICATOR (~10cm in camera basis)
	# =========================================================================
	var eye_mid_local = (model_points[0] + model_points[2]) * 0.5
	var eye_origin_3d = xform * eye_mid_local
	var raw_gaze = tracker.call("get_gaze_direction") if tracker.has_method("get_gaze_direction") else Vector3(0.0, 0.0, 1.0)
	if raw_gaze.length_squared() < 0.001:
		raw_gaze = Vector3(0.0, 0.0, 1.0)
	var eye_fwd = raw_gaze.normalized()

	# Construct orthonormal orientation basis for eye gaze vector
	var eye_up_ref = Vector3(0.0, 1.0, 0.0)
	if abs(eye_fwd.dot(eye_up_ref)) > 0.95:
		eye_up_ref = Vector3(1.0, 0.0, 0.0)
	var eye_right_vec = eye_up_ref.cross(eye_fwd).normalized()
	var eye_up_vec = eye_fwd.cross(eye_right_vec).normalized()

	var eye_axis_len = 100.0 # 100 mm = 10 cm
	var eye_x_3d = eye_origin_3d + eye_right_vec * (eye_axis_len * 0.6)
	var eye_y_3d = eye_origin_3d + eye_up_vec * (eye_axis_len * 0.6)
	var eye_fwd_3d = eye_origin_3d + eye_fwd * eye_axis_len

	var pt_eye_org = cam_to_screen.call(eye_origin_3d)
	var pt_eye_x = cam_to_screen.call(eye_x_3d)
	var pt_eye_y = cam_to_screen.call(eye_y_3d)
	var pt_eye_fwd = cam_to_screen.call(eye_fwd_3d)

	if pt_eye_org != Vector2.INF:
		# Transparent +X coordinate (Red, alpha ~0.35)
		if pt_eye_x != Vector2.INF:
			gd_draw_line(pt_eye_org, pt_eye_x, Color(1.0, 0.3, 0.3, 0.35), 2.0)
		# Transparent +Y coordinate (Green, alpha ~0.35)
		if pt_eye_y != Vector2.INF:
			gd_draw_line(pt_eye_org, pt_eye_y, Color(0.3, 1.0, 0.3, 0.35), 2.0)
		# Saturated forward Eye Gaze vector (Lime Green, alpha 1.0, thick line + diamond)
		if pt_eye_fwd != Vector2.INF:
			gd_draw_line(pt_eye_org, pt_eye_fwd, Color(0.2, 1.0, 0.1, 1.0), 3.5)
			gd_draw_circle(pt_eye_fwd, 4.5, Color(0.2, 1.0, 0.1, 1.0))

func gd_draw_circle(pos: Vector2, radius: float, color: Color):
	if active_canvas:
		active_canvas.draw_circle(pos, radius, color)

func gd_draw_line(from: Vector2, to: Vector2, color: Color, width: float):
	if active_canvas:
		active_canvas.draw_line(from, to, color, width)
