extends SceneTree

func _init():
	print("=================== CAMERA PROBE TEST ===================")
	if not Engine.has_singleton("CameraServer"):
		printerr("FAIL: CameraServer singleton missing from Engine")
		quit(1)
		return

	var cs = Engine.get_singleton("CameraServer")
	if not is_instance_valid(cs):
		printerr("FAIL: CameraServer singleton instance is invalid")
		quit(1)
		return

	cs.set_monitoring_feeds(true)
	var feeds = cs.feeds()
	print("Camera feeds count: ", feeds.size())
	for i in range(feeds.size()):
		var f = feeds[i]
		if f:
			print("Feed ", i, ": name=", f.get_name(), " active=", f.is_active(), " position=", f.get_position())

	var vs = Engine.get_singleton("VisionServer")
	if not is_instance_valid(vs):
		printerr("FAIL: VisionServer singleton missing or invalid")
		quit(1)
		return

	var cam_rid = vs.camera_create()
	vs.camera_set_device_id(cam_rid, 0)
	var started = vs.camera_start(cam_rid)
	print("VisionServer.camera_start(0) returned: ", started)
	if not started:
		printerr("FAIL: VisionServer.camera_start(0) failed")
		quit(1)
		return

	var is_active = vs.camera_is_active(cam_rid)
	if not is_active:
		printerr("FAIL: VisionServer.camera_is_active(0) returned false after start")
		quit(1)
		return

	vs.camera_stop(cam_rid)
	if vs.camera_is_active(cam_rid):
		printerr("FAIL: VisionServer.camera_is_active(0) returned true after stop")
		quit(1)
		return

	vs.camera_free(cam_rid)
	print("PASS: Low-level CameraServer and VisionServer probe verified successfully.")
	print("=========================================================")
	quit(0)
