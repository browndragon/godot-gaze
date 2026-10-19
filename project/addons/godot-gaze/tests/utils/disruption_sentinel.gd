# project/addons/godot-gaze/tests/utils/disruption_sentinel.gd
class_name DisruptionSentinel
extends RefCounted

## Utility to detect whether a windowed test failure was caused or influenced
## by environmental desktop activity (user mouse movement, clicking, or window focus loss).

static func check_disruption() -> String:
	var reasons: Array[String] = []
	
	# 1. Did the Godot window lose OS key / application focus?
	if not DisplayServer.window_is_focused(0):
		reasons.append("Window lost OS focus (Cmd+Tab, app switch, or clicked outside)")
	
	# 2. Was the physical mouse active during test execution?
	var gs = Engine.get_singleton("GazeServer")
	if gs and gs.is_physical_mouse_active():
		reasons.append("Physical mouse motion was detected")
	
	# 3. Was any physical mouse button pressed?
	if DisplayServer.mouse_get_button_state() != 0:
		reasons.append("Physical mouse button click detected")
	
	if reasons.is_empty():
		return ""
	return " | ".join(reasons)

static func fail(tree: SceneTree, msg: String, exit_code: int = 1) -> void:
	printerr("FAIL: ", msg)
	var disruption = check_disruption()
	if not disruption.is_empty():
		printerr("[ENVIRONMENTAL_DISRUPTION] Desktop activity detected during failure: ", disruption)
	tree.quit(exit_code)
