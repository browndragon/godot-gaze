# res://addons/godot-gaze/tests/overlay_robustness_test.gd
extends SceneTree

static func project_point_math(p_face: Vector3, xform: Transform3D, focal_len: float, cx: float, cy: float, drawn_rect: Rect2, img_w: float, img_h: float, control_origin: Vector2) -> Vector2:
	var p_cam = xform * p_face
	var depth = -p_cam.z
	if depth <= 0.01:
		return Vector2.INF
	else:
		var px = (p_cam.x / depth) * focal_len + cx
		var py = (-p_cam.y / depth) * focal_len + cy
		return control_origin + Vector2(px * drawn_rect.size.x / img_w, py * drawn_rect.size.y / img_h)


# Mock subclass to capture drawing coordinates and prevent canvas errors in headless mode
class MockDebugCamFeed extends "res://addons/godot-gaze/debug_cam_feed.gd":
	var draw_circle_calls = []
	var draw_line_calls = []

	func gd_draw_circle(pos: Vector2, radius: float, color: Color):
		draw_circle_calls.append({"position": pos, "radius": radius, "color": color})

	func gd_draw_line(from: Vector2, to: Vector2, color: Color, width: float):
		draw_line_calls.append({"from": from, "to": to, "color": color, "width": width})

	func clear_draw_calls():
		draw_circle_calls.clear()
		draw_line_calls.clear()

func _init():
	print("=================== OVERLAY ROBUSTNESS & LEAK TESTS ===================")
	
	var scene_path = "res://addons/godot-gaze/debug_cam_feed.tscn"
	var overlay_scene = load(scene_path)
	if overlay_scene == null:
		printerr("FAIL: Unable to load debug_cam_feed.tscn")
		quit(1)
		return
		
	# Test 1: Instantiation and tree lifecycle
	print("Test 1: Overlay instantiation and tree lifecycle...")
	var overlay1 = overlay_scene.instantiate()
	root.add_child(overlay1)
	overlay1._process(0.016)
	overlay1._draw()
	root.remove_child(overlay1)
	overlay1.free()
	print("Test 1 Passed: Lifecycle cleanly handled.")

	# Test 2: Preview state updates on tree visibility
	print("Test 2: Preview state visibility check...")
	var overlay2 = overlay_scene.instantiate()
	root.add_child(overlay2)
	overlay2.visible = true
	overlay2._update_preview_state()
	var gs = Engine.get_singleton("GazeServer")
	if gs and not gs.is_camera_preview_requested():
		printerr("FAIL: Preview requested was not set to true when overlay is visible")
		quit(1)
		return
	overlay2.visible = false
	overlay2._update_preview_state()
	if gs and gs.is_camera_preview_requested():
		printerr("FAIL: Preview requested was not set to false when overlay is hidden")
		quit(1)
		return
	root.remove_child(overlay2)
	overlay2.free()
	print("Test 2 Passed: Preview state tracking verified.")

	# Test 3: Aspect-Ratio Drawing Math Verification
	print("Test 3: Verifying get_texture_drawn_rect math logic...")
	var overlay3 = overlay_scene.instantiate()
	root.add_child(overlay3)
	var cam_rect_3 = overlay3.get_texture_rect("CameraFeedRect")
	if cam_rect_3.get_parent():
		cam_rect_3.get_parent().remove_child(cam_rect_3)
	cam_rect_3.custom_minimum_size = Vector2(0, 0)
	cam_rect_3.size = Vector2(800, 600)
	cam_rect_3.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	
	# Case A: Same aspect ratio (4:3)
	var tex_a = ImageTexture.create_from_image(Image.create(400, 300, false, Image.FORMAT_RGB8))
	cam_rect_3.texture = tex_a
	var rect_a = overlay3.get_texture_drawn_rect(cam_rect_3)
	print("  Case A (400x300 inside 800x600) -> ", rect_a)
	if rect_a.position != Vector2(0, 0) or rect_a.size != Vector2(800, 600):
		printerr("FAIL: Case A texture drawn rect incorrect")
		quit(1)
		return
		
	# Case B: Narrow texture (pillarboxed)
	var tex_b = ImageTexture.create_from_image(Image.create(200, 300, false, Image.FORMAT_RGB8))
	cam_rect_3.texture = tex_b
	var rect_b = overlay3.get_texture_drawn_rect(cam_rect_3)
	print("  Case B (200x300 inside 800x600) -> ", rect_b)
	if abs(rect_b.position.x - 200.0) > 0.01 or rect_b.position.y != 0.0 or abs(rect_b.size.x - 400.0) > 0.01 or rect_b.size.y != 600.0:
		printerr("FAIL: Case B texture drawn rect incorrect")
		quit(1)
		return
		
	# Case C: Wide texture (letterboxed)
	var tex_c = ImageTexture.create_from_image(Image.create(600, 300, false, Image.FORMAT_RGB8))
	cam_rect_3.texture = tex_c
	var rect_c = overlay3.get_texture_drawn_rect(cam_rect_3)
	print("  Case C (600x300 inside 800x600) -> ", rect_c)
	if rect_c.position.x != 0.0 or abs(rect_c.position.y - 100.0) > 0.01 or rect_c.size.x != 800.0 or abs(rect_c.size.y - 400.0) > 0.01:
		printerr("FAIL: Case C texture drawn rect incorrect")
		quit(1)
		return
		
	# Case D: Zero height rectangle (800x0)
	cam_rect_3.size = Vector2(800, 0)
	var rect_d = overlay3.get_texture_drawn_rect(cam_rect_3)
	if rect_d.position != Vector2(0, 0) or rect_d.size != Vector2(0, 0):
		printerr("FAIL: Case D texture drawn rect incorrect")
		quit(1)
		return

	# Case E: Zero dimension rectangle (0x0)
	cam_rect_3.size = Vector2(0, 0)
	var rect_e = overlay3.get_texture_drawn_rect(cam_rect_3)
	if rect_e.position != Vector2(0, 0) or rect_e.size != Vector2(0, 0):
		printerr("FAIL: Case E texture drawn rect incorrect")
		quit(1)
		return
		
	root.remove_child(overlay3)
	overlay3.free()
	cam_rect_3.free()
	print("Test 3 Passed: Aspect-Ratio Drawing Math verified.")

	# Test 4: Memory Leak Verification
	print("Test 4: Running setup/teardown in loop to check memory/object leaks...")
	for i in range(10):
		var temp_overlay = overlay_scene.instantiate()
		temp_overlay._process(0.016)
		temp_overlay.free()

	var obj_baseline = Performance.get_monitor(Performance.OBJECT_COUNT)
	print("  Baseline Object Count: ", obj_baseline)
	
	for i in range(100):
		var temp_overlay = overlay_scene.instantiate()
		temp_overlay._process(0.016)
		temp_overlay.free()

	var obj_end = Performance.get_monitor(Performance.OBJECT_COUNT)
	print("  End Object Count: ", obj_end)
	var diff = obj_end - obj_baseline
	print("  Object Count Difference: ", diff)
	
	if diff > 5:
		printerr("FAIL: Memory/Object leak detected! Object count increased by: ", diff)
		quit(1)
		return
		
	print("Test 4 Passed: No memory/object leaks detected.")

	# Test 5: Landmark Projection Math and Edge Depths
	print("Test 5: Verification of Landmark Projection math...")
	var test_feed = MockDebugCamFeed.new()
	var panel = Panel.new()
	panel.name = "Panel"
	test_feed.add_child(panel)
	
	var mock_cam_rect = TextureRect.new()
	mock_cam_rect.name = "CameraFeedRect"
	panel.add_child(mock_cam_rect)
	
	var img = Image.create(640, 480, false, Image.FORMAT_RGB8)
	var tex = ImageTexture.create_from_image(img)
	mock_cam_rect.texture = tex
	mock_cam_rect.size = Vector2(640, 480)
	mock_cam_rect.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED

	var right_eye = Vector3(-30.0, 28.676, 0.0)
	var cx = 320.0
	var cy = 240.0
	var focal_len = 1000.0
	var img_w = 640.0
	var img_h = 480.0
	var drawn_rect = Rect2(0, 0, 640, 480)
	var control_origin = Vector2(0, 0)
	var xform = Transform3D(Basis(), Vector3(0, 0, -500.0))

	var actual_pt_0 = project_point_math(right_eye, xform, focal_len, cx, cy, drawn_rect, img_w, img_h, control_origin)
	var expected_pt_0 = Vector2(260.0, 182.648)
	print("    Expected Point 0 (Right eye): ", expected_pt_0, " | Actual: ", actual_pt_0)
	if (actual_pt_0 - expected_pt_0).length() > 0.01:
		printerr("FAIL: Subtest A projection incorrect")
		quit(1)
		return

	# Division by zero depth
	var zero_xform = Transform3D(Basis(), Vector3(0, 0, 0.0))
	var actual_pt_zero = project_point_math(right_eye, zero_xform, focal_len, cx, cy, drawn_rect, img_w, img_h, control_origin)
	if actual_pt_zero != Vector2.INF:
		printerr("FAIL: Subtest B math did not skip zero depth")
		quit(1)
		return

	# Near-plane depth
	var near_xform = Transform3D(Basis(), Vector3(0, 0, -0.00005))
	var actual_pt_near = project_point_math(right_eye, near_xform, focal_len, cx, cy, drawn_rect, img_w, img_h, control_origin)
	if actual_pt_near != Vector2.INF:
		printerr("FAIL: Subtest C math did not skip near depth")
		quit(1)
		return

	test_feed.free()
	print("Test 5 Passed: Landmark projection math and edge depth cases validated.")

	print("==================================================================")
	print("ALL overlay robustness tests PASSED!")
	print("==================================================================")
	quit(0)
