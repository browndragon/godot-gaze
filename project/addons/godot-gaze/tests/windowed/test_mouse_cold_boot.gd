extends SceneTree

func _init():
	var gds = Engine.get_singleton("GazeDisplayServer")
	var gs = Engine.get_singleton("GazeServer")
	print("[DIAG_START] ds_pos=", DisplayServer.mouse_get_position(), 
		  " gds_pos=", gds.mouse_get_position(), 
		  " gds_mask=", gds.mouse_get_button_state(), 
		  " still=", gs.is_physical_mouse_still(), 
		  " active=", gs.is_physical_mouse_active())

	# Test 1: Pump synthetic InputEventMouseMotion
	var mm = InputEventMouseMotion.new()
	mm.device = GazeServer.DEVICE_ID_GAZE_SYNTHETIC
	mm.set_meta("synthetic_gaze", true)
	mm.position = Vector2(999, 999)
	mm.global_position = Vector2(999, 999)
	Input.parse_input_event(mm)
	await process_frame

	print("[DIAG_AFTER_MM] ds_pos=", DisplayServer.mouse_get_position(), 
		  " gds_pos=", gds.mouse_get_position(), 
		  " gds_mask=", gds.mouse_get_button_state(), 
		  " still=", gs.is_physical_mouse_still(), 
		  " active=", gs.is_physical_mouse_active())

	# Test 2: Pump synthetic InputEventMouseButton (pressed = true, simulating blink)
	var mb = InputEventMouseButton.new()
	mb.device = GazeServer.DEVICE_ID_GAZE_SYNTHETIC
	mb.set_meta("synthetic_gaze", true)
	mb.button_index = MOUSE_BUTTON_LEFT
	mb.pressed = true
	mb.position = Vector2(999, 999)
	Input.parse_input_event(mb)
	await process_frame

	print("[DIAG_AFTER_MB_PRESS] ds_pos=", DisplayServer.mouse_get_position(), 
		  " gds_pos=", gds.mouse_get_position(), 
		  " ds_btn_state=", DisplayServer.mouse_get_button_state(),
		  " input_mask=", Input.get_mouse_button_mask(), 
		  " gds_mask=", gds.mouse_get_button_state(), 
		  " still=", gs.is_physical_mouse_still(), 
		  " active=", gs.is_physical_mouse_active())

	# Assert that synthetic press did NOT pollute GazeDisplayServer hardware button query
	if gds.mouse_get_button_state() != DisplayServer.mouse_get_button_state():
		printerr("FAIL: GazeDisplayServer.mouse_get_button_state() diverged from DisplayServer")
		quit(1)
		return

	# Assert that synthetic mouse button did NOT activate physical mouse stillness state machine
	if not gs.is_physical_mouse_still() or gs.is_physical_mouse_active():
		printerr("FAIL: Synthetic mouse press erroneously triggered physical mouse active state")
		quit(1)
		return

	# Test 3: Release button
	var mb_up = InputEventMouseButton.new()
	mb_up.device = GazeServer.DEVICE_ID_GAZE_SYNTHETIC
	mb_up.set_meta("synthetic_gaze", true)
	mb_up.button_index = MOUSE_BUTTON_LEFT
	mb_up.pressed = false
	mb_up.position = Vector2(999, 999)
	Input.parse_input_event(mb_up)
	await process_frame

	print("[DIAG_AFTER_MB_RELEASE] ds_pos=", DisplayServer.mouse_get_position(), 
		  " gds_pos=", gds.mouse_get_position(), 
		  " input_mask=", Input.get_mouse_button_mask(), 
		  " gds_mask=", gds.mouse_get_button_state(), 
		  " still=", gs.is_physical_mouse_still(), 
		  " active=", gs.is_physical_mouse_active())

	if not gs.is_physical_mouse_still() or gs.is_physical_mouse_active():
		printerr("FAIL: Physical mouse is not still after synthetic release")
		quit(1)
		return

	print("PASS: test_mouse_cold_boot passed with zero physical mouse feedback loops.")
	quit(0)
