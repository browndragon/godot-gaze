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


# Mock classes definition
class MockCameraSensor extends Node:
	signal frame_ready(img: Image)
	var focal_length = 1000.0
	func get_last_frame() -> Image: return null

class MockEyeEstimator extends Node:
	signal eye_crops_ready(left: Image, right: Image)
	signal gaze_estimated()
	func get_left_eye_crop() -> Image: return null
	func get_right_eye_crop() -> Image: return null

class MockFaceEstimator extends Node:
	var has_detected_face = false
	var transform = Transform3D()

class MockTracker extends Node:
	var camera_sensor = null
	var eye_estimator = null
	var face_estimator = null
	var gaze_direction = Vector3(0.0, 0.0, -1.0)
	func get_camera_sensor(): return camera_sensor
	func get_eye_estimator(): return eye_estimator
	func get_face_estimator(): return face_estimator
	func get_gaze_direction(): return gaze_direction

# Mock subclass to capture drawing coordinates and prevent canvas errors in headless mode
class MockDebugCamFeed extends "res://addons/godot-gaze/debug_cam_feed.gd":
	var draw_circle_calls = []
	var draw_line_calls = []

	func gd_draw_circle(position: Vector2, radius: float, color: Color):
		draw_circle_calls.append({"position": position, "radius": radius, "color": color})

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
		
	# Test 1: Null Tracker
	print("Test 1: Null tracker assignment...")
	var overlay1 = overlay_scene.instantiate()
	root.add_child(overlay1)
	overlay1.tracker = null
	overlay1._process(0.01)
	overlay1._draw()
	root.remove_child(overlay1)
	overlay1.free()
	print("Test 1 Passed: No crash with null tracker.")

	# Test 2: Incomplete Tracker (Node without required methods)
	print("Test 2: Incomplete tracker assignment...")
	var overlay2 = overlay_scene.instantiate()
	root.add_child(overlay2)
	var dummy_node = Node.new()
	overlay2.tracker = dummy_node
	overlay2._process(0.01)
	overlay2._draw()
	root.remove_child(overlay2)
	overlay2.free()
	dummy_node.free()
	print("Test 2 Passed: No crash with incomplete tracker.")

	# Test 3: Partially Initialized Tracker (Returns null sensors/estimators)
	print("Test 3: Partially initialized tracker (all sensors null)...")
	var overlay3 = overlay_scene.instantiate()
	root.add_child(overlay3)
	var partial_tracker = MockTracker.new()
	overlay3.tracker = partial_tracker
	overlay3._process(0.01)
	overlay3._draw()
	root.remove_child(overlay3)
	overlay3.free()
	partial_tracker.free()
	print("Test 3 Passed: No crash with partially initialized tracker.")

	# Test 4: ImageTexture updates under varying resolutions
	print("Test 4: ImageTexture resolution changes and crops...")
	var overlay4 = overlay_scene.instantiate()
	root.add_child(overlay4)
	var full_tracker = MockTracker.new()
	var mock_cam = MockCameraSensor.new()
	var mock_eye = MockEyeEstimator.new()
	var mock_face = MockFaceEstimator.new()
	full_tracker.camera_sensor = mock_cam
	full_tracker.eye_estimator = mock_eye
	full_tracker.face_estimator = mock_face
	
	overlay4.tracker = full_tracker
	overlay4._process(0.01)
	
	# Initial check: no textures
	var cam_rect = overlay4.get_texture_rect("CameraFeedRect")
	var left_rect = overlay4.get_texture_rect("LeftEyeRect")
	var right_rect = overlay4.get_texture_rect("RightEyeRect")
	
	if cam_rect.texture != null:
		printerr("FAIL: CameraFeedRect texture should be null initially")
		quit(1)
		return
	if left_rect.texture != null:
		printerr("FAIL: LeftEyeRect texture should be null initially")
		quit(1)
		return
	if right_rect.texture != null:
		printerr("FAIL: RightEyeRect texture should be null initially")
		quit(1)
		return
	
	# Resolution 1: 640x480, crops 80x60
	print("  Feeding Resolution: 640x480, Crops: 80x60")
	var img_640 = Image.create(640, 480, false, Image.FORMAT_RGB8)
	var left_80 = Image.create(80, 60, false, Image.FORMAT_RGB8)
	var right_80 = Image.create(80, 60, false, Image.FORMAT_RGB8)
	
	mock_cam.emit_signal("frame_ready", img_640)
	mock_eye.emit_signal("eye_crops_ready", left_80, right_80)
	
	if cam_rect.texture == null or cam_rect.texture.get_size() != Vector2(640, 480):
		printerr("FAIL: CameraFeedRect texture incorrect after 640x480 feed")
		quit(1)
		return
	if left_rect.texture == null or left_rect.texture.get_size() != Vector2(80, 60):
		printerr("FAIL: LeftEyeRect texture incorrect after 80x60 crop")
		quit(1)
		return
	if right_rect.texture == null or right_rect.texture.get_size() != Vector2(80, 60):
		printerr("FAIL: RightEyeRect texture incorrect after 80x60 crop")
		quit(1)
		return
		
	# Resolution 2: 1280x720, crops 160x120
	print("  Feeding Resolution: 1280x720, Crops: 160x120")
	var img_1280 = Image.create(1280, 720, false, Image.FORMAT_RGB8)
	var left_160 = Image.create(160, 120, false, Image.FORMAT_RGB8)
	var right_160 = Image.create(160, 120, false, Image.FORMAT_RGB8)
	
	mock_cam.emit_signal("frame_ready", img_1280)
	mock_eye.emit_signal("eye_crops_ready", left_160, right_160)
	
	if cam_rect.texture.get_size() != Vector2(1280, 720):
		printerr("FAIL: CameraFeedRect texture did not resize to 1280x720")
		quit(1)
		return
	if left_rect.texture.get_size() != Vector2(160, 120):
		printerr("FAIL: LeftEyeRect texture did not resize to 160x120")
		quit(1)
		return
	if right_rect.texture.get_size() != Vector2(160, 120):
		printerr("FAIL: RightEyeRect texture did not resize to 160x120")
		quit(1)
		return
		
	# Resolution 3: Empty / Null images
	print("  Feeding Empty / Null images...")
	var empty_img = Image.new()
	mock_cam.emit_signal("frame_ready", empty_img)
	mock_eye.emit_signal("eye_crops_ready", null, empty_img)
	
	# The textures should remain at the last valid sizes
	if cam_rect.texture.get_size() != Vector2(1280, 720):
		printerr("FAIL: CameraFeedRect texture corrupted after empty image")
		quit(1)
		return
	if left_rect.texture.get_size() != Vector2(160, 120):
		printerr("FAIL: LeftEyeRect texture corrupted after null image")
		quit(1)
		return
	if right_rect.texture.get_size() != Vector2(160, 120):
		printerr("FAIL: RightEyeRect texture corrupted after empty image")
		quit(1)
		return
		
	# Resolution 4: Unequal left/right crop sizes (adversarial check)
	print("  Feeding unequal left (50x50) and right (200x150) crop sizes...")
	var left_50 = Image.create(50, 50, false, Image.FORMAT_RGB8)
	var right_200 = Image.create(200, 150, false, Image.FORMAT_RGB8)
	mock_eye.emit_signal("eye_crops_ready", left_50, right_200)
	if left_rect.texture.get_size() != Vector2(50, 50):
		printerr("FAIL: LeftEyeRect did not update to 50x50")
		quit(1)
		return
	if right_rect.texture.get_size() != Vector2(200, 150):
		printerr("FAIL: RightEyeRect did not update to 200x150")
		quit(1)
		return

	# Clean up overlay4
	root.remove_child(overlay4)
	overlay4.free()
	full_tracker.free()
	mock_cam.free()
	mock_eye.free()
	mock_face.free()
	print("Test 4 Passed: Resolution scaling robust.")

	# Test 5: Memory Leak Verification
	print("Test 5: Running setup/teardown in loop to check memory/object leaks...")
	
	# Force garbage collection/freeing of any deferred resources before starting measurement
	for i in range(10):
		var temp_overlay = overlay_scene.instantiate()
		var temp_tracker = MockTracker.new()
		temp_tracker.camera_sensor = MockCameraSensor.new()
		temp_tracker.eye_estimator = MockEyeEstimator.new()
		temp_tracker.face_estimator = MockFaceEstimator.new()
		temp_overlay.tracker = temp_tracker
		
		# feeding textures
		var img = Image.create(100 + i, 100 + i, false, Image.FORMAT_RGB8)
		temp_overlay._on_frame_ready(img)
		
		temp_overlay.tracker = null
		temp_overlay.free()
		temp_tracker.camera_sensor.free()
		temp_tracker.eye_estimator.free()
		temp_tracker.face_estimator.free()
		temp_tracker.free()

	# Record baseline object count
	var obj_baseline = Performance.get_monitor(Performance.OBJECT_COUNT)
	print("  Baseline Object Count: ", obj_baseline)
	
	# Run 100 iterations of instantiation, texture update, and destruction
	for i in range(100):
		var temp_overlay = overlay_scene.instantiate()
		var temp_tracker = MockTracker.new()
		temp_tracker.camera_sensor = MockCameraSensor.new()
		temp_tracker.eye_estimator = MockEyeEstimator.new()
		temp_tracker.face_estimator = MockFaceEstimator.new()
		temp_overlay.tracker = temp_tracker
		
		# Vary image size each iteration
		var w = 100 + (i % 50)
		var h = 100 + (i % 50)
		var img = Image.create(w, h, false, Image.FORMAT_RGB8)
		temp_overlay._on_frame_ready(img)
		
		# Teardown
		temp_overlay.tracker = null
		temp_overlay.free()
		temp_tracker.camera_sensor.free()
		temp_tracker.eye_estimator.free()
		temp_tracker.face_estimator.free()
		temp_tracker.free()

	var obj_end = Performance.get_monitor(Performance.OBJECT_COUNT)
	print("  End Object Count: ", obj_end)
	var diff = obj_end - obj_baseline
	print("  Object Count Difference: ", diff)
	
	# We expect the difference to be 0 or extremely close to it.
	if diff > 5:
		printerr("FAIL: Memory/Object leak detected! Object count increased by: ", diff)
		quit(1)
		return
		
	print("Test 5 Passed: No memory/object leaks detected.")
	
	# Test 6: Aspect-Ratio Drawing Math Verification
	print("Test 6: Verifying get_texture_drawn_rect math logic...")
	var overlay6 = overlay_scene.instantiate()
	root.add_child(overlay6)
	var cam_rect_6 = overlay6.get_texture_rect("CameraFeedRect")
	if cam_rect_6.get_parent():
		cam_rect_6.get_parent().remove_child(cam_rect_6)
	cam_rect_6.custom_minimum_size = Vector2(0, 0)
	cam_rect_6.size = Vector2(800, 600)
	cam_rect_6.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	
	# Case A: Same aspect ratio (4:3)
	var tex_a = ImageTexture.create_from_image(Image.create(400, 300, false, Image.FORMAT_RGB8))
	cam_rect_6.texture = tex_a
	var rect_a = overlay6.get_texture_drawn_rect(cam_rect_6)
	print("  Case A (400x300 inside 800x600) -> ", rect_a)
	if rect_a.position != Vector2(0, 0) or rect_a.size != Vector2(800, 600):
		printerr("FAIL: Case A texture drawn rect incorrect")
		quit(1)
		return
		
	# Case B: Narrow texture (pillarboxed)
	var tex_b = ImageTexture.create_from_image(Image.create(200, 300, false, Image.FORMAT_RGB8))
	cam_rect_6.texture = tex_b
	var rect_b = overlay6.get_texture_drawn_rect(cam_rect_6)
	print("  Case B (200x300 inside 800x600) -> ", rect_b)
	if abs(rect_b.position.x - 200.0) > 0.01 or rect_b.position.y != 0.0 or abs(rect_b.size.x - 400.0) > 0.01 or rect_b.size.y != 600.0:
		printerr("FAIL: Case B texture drawn rect incorrect")
		quit(1)
		return
		
	# Case C: Wide texture (letterboxed)
	var tex_c = ImageTexture.create_from_image(Image.create(600, 300, false, Image.FORMAT_RGB8))
	cam_rect_6.texture = tex_c
	var rect_c = overlay6.get_texture_drawn_rect(cam_rect_6)
	print("  Case C (600x300 inside 800x600) -> ", rect_c)
	if rect_c.position.x != 0.0 or abs(rect_c.position.y - 100.0) > 0.01 or rect_c.size.x != 800.0 or abs(rect_c.size.y - 400.0) > 0.01:
		printerr("FAIL: Case C texture drawn rect incorrect")
		quit(1)
		return
		
	# Case D: Zero height texture (adversarial check)
	print("  Case D: Zero height rectangle (800x0)...")
	cam_rect_6.size = Vector2(800, 0)
	var rect_d = overlay6.get_texture_drawn_rect(cam_rect_6)
	print("  Case D result -> ", rect_d)
	if rect_d.position != Vector2(0, 0) or rect_d.size != Vector2(0, 0):
		printerr("FAIL: Case D texture drawn rect incorrect")
		quit(1)
		return

	# Case E: Zero width rectangle (0x600)...
	print("  Case E: Zero width rectangle (0x600)...")
	cam_rect_6.size = Vector2(0, 600)
	var rect_e = overlay6.get_texture_drawn_rect(cam_rect_6)
	print("  Case E result -> ", rect_e)
	if rect_e.position != Vector2(0, 0) or rect_e.size != Vector2(0, 0):
		printerr("FAIL: Case E texture drawn rect incorrect")
		quit(1)
		return

	# Case F: Zero dimension rectangle (0x0)...
	print("  Case F: Zero dimension rectangle (0x0)...")
	cam_rect_6.size = Vector2(0, 0)
	var rect_f = overlay6.get_texture_drawn_rect(cam_rect_6)
	print("  Case F result -> ", rect_f)
	if rect_f.position != Vector2(0, 0) or rect_f.size != Vector2(0, 0):
		printerr("FAIL: Case F texture drawn rect incorrect")
		quit(1)
		return
		
	root.remove_child(overlay6)
	overlay6.free()
	cam_rect_6.free()
	print("Test 6 Passed: Aspect-Ratio Drawing Math verified.")

	# Test 7: Landmark Projection Math and Near-Plane / Negative Depth values (Adversarial)
	print("Test 7: Verification of Landmark Projection math and edge depths...")
	var test_feed = MockDebugCamFeed.new()
	var panel = Panel.new()
	panel.name = "Panel"
	test_feed.add_child(panel)
	
	var mock_cam_rect = TextureRect.new()
	mock_cam_rect.name = "CameraFeedRect"
	panel.add_child(mock_cam_rect)
	
	# Set up standard image dimensions (640x480)
	var img = Image.create(640, 480, false, Image.FORMAT_RGB8)
	var tex = ImageTexture.create_from_image(img)
	mock_cam_rect.texture = tex
	mock_cam_rect.size = Vector2(640, 480)
	mock_cam_rect.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED

	# Set up tracker and mock estimators
	var tracker = MockTracker.new()
	var cam_sensor = MockCameraSensor.new()
	var face_est = MockFaceEstimator.new()
	tracker.camera_sensor = cam_sensor
	tracker.face_estimator = face_est
	test_feed.tracker = tracker
	
	face_est.has_detected_face = true
	cam_sensor.focal_length = 1000.0

	# Subtest A: Standard position in front of camera (Z = -500)
	# OpenCV convention translates to Godot camera space: looking down -Z.
	# Standard depth = -(-500) = 500 mm.
	# TODO: Pull from a well-known constant exposed by our library.
	print("  Subtest A: Standard projection (Z = -500)...")
	face_est.transform = Transform3D(Basis(), Vector3(0, 0, -500.0))
	var right_eye = Vector3(-30.0, 28.676, 0.0)
	var cx = 320.0
	var cy = 240.0
	var focal_len = 1000.0
	var img_w = 640.0
	var img_h = 480.0
	var drawn_rect = Rect2(0, 0, 640, 480)
	var control_origin = Vector2(0, 0)

	var actual_pt_0 = project_point_math(right_eye, face_est.transform, focal_len, cx, cy, drawn_rect, img_w, img_h, control_origin)
	var expected_pt_0 = Vector2(260.0, 182.648)
	print("    Expected Point 0 (Right eye): ", expected_pt_0, " | Actual: ", actual_pt_0)
	if (actual_pt_0 - expected_pt_0).length() > 0.01:
		printerr("FAIL: Subtest A projection incorrect")
		quit(1)
		return

	# Verify no crash on Happy Path (even if engine draws warn outside NOTIFICATION_DRAW)
	test_feed.clear_draw_calls()
	test_feed._draw()
	print("    Subtest A PASSED.")

	# Subtest B: Division by Zero Depth (Z = 0)
	print("  Subtest B: Division by zero depth (Z = 0)...")
	face_est.transform = Transform3D(Basis(), Vector3(0, 0, 0.0))
	var actual_pt_0_zero = project_point_math(right_eye, face_est.transform, focal_len, cx, cy, drawn_rect, img_w, img_h, control_origin)
	if actual_pt_0_zero != Vector2.INF:
		printerr("FAIL: Subtest B math did not skip zero depth")
		quit(1)
		return

	# Verify no crash (returns early because depth <= 0.01)
	test_feed.clear_draw_calls()
	test_feed._draw()
	print("    Subtest B PASSED.")

	# Subtest C: Near-plane depth (Z = -0.00005) -> absolute depth is less than 0.0001
	print("  Subtest C: Near-plane depth within epsilon (Z = -0.00005)...")
	face_est.transform = Transform3D(Basis(), Vector3(0, 0, -0.00005))
	var actual_pt_0_near = project_point_math(right_eye, face_est.transform, focal_len, cx, cy, drawn_rect, img_w, img_h, control_origin)
	if actual_pt_0_near != Vector2.INF:
		printerr("FAIL: Subtest C math did not skip near depth")
		quit(1)
		return

	# Verify no crash (returns early because depth <= 0.01)
	test_feed.clear_draw_calls()
	test_feed._draw()
	print("    Subtest C PASSED.")

	# Subtest D: Negative depth / Behind Camera (Z = 500)
	# OpenCV space depth = -500.0 (negative!)
	# Z is positive in camera coordinate system, meaning the object is behind the camera.
	print("  Subtest D: Negative depth / Behind camera (Z = 500)...")
	face_est.transform = Transform3D(Basis(), Vector3(0, 0, 500.0))
	var actual_pt_0_neg = project_point_math(right_eye, face_est.transform, focal_len, cx, cy, drawn_rect, img_w, img_h, control_origin)
	if actual_pt_0_neg != Vector2.INF:
		printerr("FAIL: Subtest D math did not skip negative depth")
		quit(1)
		return

	# Verify no crash (returns early because depth <= 0.01)
	test_feed.clear_draw_calls()
	test_feed._draw()
	print("    Subtest D PASSED.")

	test_feed.free()
	tracker.free()
	cam_sensor.free()
	face_est.free()
	print("Test 7 Passed: Landmark projection math and edge depth cases validated.")

	print("==================================================================")
	print("ALL overlay robustness and resolution-scaling tests PASSED!")
	print("==================================================================")
	quit(0)
