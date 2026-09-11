# A specific live-camera debug test to capture a few interesting states with user interaction.
extends Control

@onready var instruction_label = $InstructionLabel
@onready var console_output = $ConsoleOutput

var current_step = 0
var latest_gaze_event: InputEventGaze = null
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

	var gs = Engine.get_singleton("GazeServer")
	if gs:
		gs.start_tracking()
	show_step()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventGaze:
		latest_gaze_event = event

func _input(event):
	if event.is_action_pressed("ui_accept"): # Spacebar or Enter
		if current_step < steps.size():
			if latest_gaze_event != null and latest_gaze_event.is_face_tracked():
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
		"head_pos_mm": latest_gaze_event.head_pose.origin,
		"head_basis": latest_gaze_event.head_pose.basis,
		"gaze_origin_mm": latest_gaze_event.eye_origin,
		"gaze_dir": latest_gaze_event.eye_direction,
		"eye_gaze_2d": latest_gaze_event.get_eye_gaze(),
		"nose_gaze_2d": latest_gaze_event.get_nose_gaze()
	}
	collected_data.append(data)
	
	# Also print it to stdout immediately
	print("[DIAGNOSTIC DATA] Step: ", step_name)
	print("  Head Pos: ", data.head_pos_mm)
	print("  Gaze Origin: ", data.gaze_origin_mm)
	print("  Gaze Dir: ", data.gaze_dir)
	print("  Eye Gaze 2D: ", data.eye_gaze_2d)
	print("  Nose Gaze 2D: ", data.nose_gaze_2d)

func finish_test():
	var gs = Engine.get_singleton("GazeServer")
	if gs:
		gs.stop_tracking(false)
	instruction_label.text = "TEST COMPLETED!\n\nPlease copy the log output below and paste it in the chat."
	
	var out_text = "=== HEAD TEST RESULTS ===\n"
	for d in collected_data:
		out_text += "Step: %s\n" % d.step
		out_text += "  Head Pos: %s\n" % str(d.head_pos_mm)
		out_text += "  Gaze Origin: %s\n" % str(d.gaze_origin_mm)
		out_text += "  Gaze Dir: %s\n" % str(d.gaze_dir)
		out_text += "  Eye Gaze 2D: %s\n" % str(d.eye_gaze_2d)
		out_text += "  Nose Gaze 2D: %s\n\n" % str(d.nose_gaze_2d)
	out_text += "========================="
	
	console_output.text = out_text
	print(out_text)
