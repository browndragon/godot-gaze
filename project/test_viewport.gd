extends SceneTree

func _initialize():
	print("--- SceneTree and Viewport Diagnostic ---")
	var ds = DisplayServer
	var screen_id = ds.window_get_current_screen()
	print("Screen Scale: ", ds.screen_get_scale(screen_id))
	print("Screen Size: ", ds.screen_get_size(screen_id))
	print("Window Position: ", ds.window_get_position())
	print("Window Size: ", ds.window_get_size())
	
	var vp = root
	print("Viewport Rect Size: ", vp.get_visible_rect().size)
	print("Viewport Final Transform: ", vp.get_final_transform())
	if vp.has_method("get_content_scale_factor"):
		print("Content Scale Factor (method): ", vp.get_content_scale_factor())
	elif "content_scale_factor" in vp:
		print("Content Scale Factor (prop): ", vp.content_scale_factor)
	quit()
