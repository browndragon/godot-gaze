# A specific live-camera debug test to capture a few interesting states with user interaction.
extends Control

@onready var instruction_label = $InstructionLabel
@onready var console_output = $ConsoleOutput
@onready var tracker = $GazeTracker

var current_step = 0
var steps = [
	{
		"instruction": "Step 1: Align your head straight in front of the webcam and stare directly at the CENTER of the screen.",
		"name": "Center Gaze"
	},
	{
		"instruction": "Step 2: Keep your head facing straight at the webcam, and move only your eyes to stare at the LEFT edge of the screen.",
		"name": "Left Gaze (Static Head)"
	},
	{
		"instruction": "Step 3: Keep your head facing straight at the webcam, and move only your eyes to stare at the RIGHT edge of the screen.",
		"name": "Right Gaze (Static Head)"
	},
	{
		"instruction": "Step 4: Rotate your head slightly to the LEFT (approx 10 degrees), but keep your eyes focused on the CENTER of the screen.",
		"name": "Head Left (Gaze Center)"
	},
	{
		"instruction": "Step 5: Rotate your head slightly to the RIGHT (approx 10 degrees), but keep your eyes focused on the CENTER of the screen.",
		"name": "Head Right (Gaze Center)"
	}
]

var collected_data = []

func _ready():
	# Center the window on start
	var screen_id = DisplayServer.window_get_current_screen()
	var screen_size = DisplayServer.screen_get_size(screen_id)
	var window_size = DisplayServer.window_get_size()
	DisplayServer.window_set_position((screen_size - window_size) / 2)

	tracker.initialize_tracker()
	show_step()

func _input(event):
	if event.is_action_pressed("ui_accept"): # Spacebar or Enter
		if current_step < steps.size():
			if tracker.is_face_detected():
				collect_data()
				current_step += 1
				if current_step < steps.size():
					show_step()
				else:
					finish_test()
			else:
				instruction_label.text = steps[current_step]["instruction"] + "\n\n[WARNING: FACE NOT DETECTED! Make sure your face is visible to the camera, then press Spacebar again...]"

func show_step():
	var step_info = steps[current_step]
	instruction_label.text = step_info["instruction"] + "\n\nPress Spacebar when you are in position..."

func collect_data():
	var step_name = steps[current_step]["name"]
	var data = {
		"step": step_name,
		"gaze_origin_cam": tracker.get_gaze_origin(),
		"gaze_dir_cam": tracker.get_gaze_direction(false),
		"gaze_dir_opencv": tracker.get_gaze_direction_opencv_space(),
		"head_rot_opencv": tracker.get_head_rotation_opencv_space(),
		"head_trans_opencv": tracker.get_head_translation_opencv_space(),
		"projected_gaze": tracker.project_gaze_ray_to_viewport(tracker.get_gaze_origin(), tracker.get_gaze_direction(false))
	}
	collected_data.append(data)
	
	# Also print it to stdout immediately
	print("[DIAGNOSTIC DATA] Step: ", step_name)
	print("  Gaze Origin Cam: ", data.gaze_origin_cam)
	print("  Gaze Dir Cam: ", data.gaze_dir_cam)
	print("  Gaze Dir OpenCV: ", data.gaze_dir_opencv)
	print("  Head Rot OpenCV: ", data.head_rot_opencv)
	print("  Head Trans OpenCV: ", data.head_trans_opencv)
	print("  Projected Gaze: ", data.projected_gaze)

func finish_test():
	tracker.stop_tracker()
	instruction_label.text = "TEST COMPLETED!\n\nPlease copy the log output below and paste it in the chat."
	
	var out_text = "=== HEAD TEST RESULTS ===\n"
	for d in collected_data:
		out_text += "Step: %s\n" % d.step
		out_text += "  Gaze Origin Cam: %s\n" % str(d.gaze_origin_cam)
		out_text += "  Gaze Dir Cam: %s\n" % str(d.gaze_dir_cam)
		out_text += "  Gaze Dir OpenCV: %s\n" % str(d.gaze_dir_opencv)
		out_text += "  Head Rot OpenCV: %s\n" % str(d.head_rot_opencv)
		out_text += "  Head Trans OpenCV: %s\n" % str(d.head_trans_opencv)
		out_text += "  Projected Gaze: %s\n\n" % str(d.projected_gaze)
	out_text += "========================="
	
	console_output.text = out_text
	print(out_text)
