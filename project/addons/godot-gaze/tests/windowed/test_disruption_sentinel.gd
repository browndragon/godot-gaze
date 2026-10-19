extends SceneTree

const DisruptionSentinel = preload("res://addons/godot-gaze/tests/utils/disruption_sentinel.gd")

func _init():
	print("--- TEST DISRUPTION SENTINEL ---")
	var disruption_str = DisruptionSentinel.check_disruption()
	print("check_disruption() returned: '", disruption_str, "'")
	if typeof(disruption_str) != TYPE_STRING:
		printerr("FAIL: check_disruption() did not return a String")
		quit(1)
		return
	print("PASS: DisruptionSentinel type and contract verified.")
	quit(0)
