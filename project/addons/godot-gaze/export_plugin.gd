# export_plugin.gd
@tool
extends EditorExportPlugin

func _get_name() -> String:
	return "GodotGazeExportPlugin"

func _export_begin(features: PackedStringArray, is_debug: bool, path: String, flags: int):
	if features.has("web") or features.has("HTML5"):
		var export_dir = path.get_base_dir()
		var dir = DirAccess.open("res://")
		if not dir:
			printerr("[GodotGaze] Failed to open DirAccess")
			return

		var files_to_copy = {
			"res://addons/godot-gaze/bin/gaze_sidecar.js": export_dir.path_join("gaze_sidecar.js")
		}

		for src_file in files_to_copy:
			var dest_file = files_to_copy[src_file]
			if FileAccess.file_exists(src_file):
				var err = dir.copy(src_file, dest_file)
				if err == OK:
					print("[GodotGaze] Successfully copied ", src_file.get_file(), " to export folder: ", dest_file)
				else:
					printerr("[GodotGaze] Failed to copy ", src_file.get_file(), " to export folder: ", err)
			else:
				printerr("[GodotGaze] Source file not found at: ", src_file)
