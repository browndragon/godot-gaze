# Runs calibration, then immediately uses it for the testing toy.
extends Control

@export var calibration_scene_path: String = "res://addons/godot-gaze/tests/calibration.tscn"
@export var test_scene_path: String = "res://addons/godot-gaze/tests/test.tscn"

@onready var calibration_scene = preload("res://addons/godot-gaze/tests/calibration.tscn")
@onready var test_scene = preload("res://addons/godot-gaze/tests/test.tscn")

var current_scene_node: Node = null
var active_calibration_dict: Dictionary = {}

func _ready():
	load_calibration_scene()

func load_calibration_scene():
	if current_scene_node:
		current_scene_node.queue_free()
		current_scene_node = null

	var inst = calibration_scene.instantiate()
	current_scene_node = inst
	
	if inst.has_signal("calibration_completed"):
		inst.calibration_completed.connect(_on_calibration_completed)
	
	add_child(inst)

func _on_calibration_completed(res_dict):
	active_calibration_dict = res_dict
	load_test_scene()

func load_test_scene():
	if current_scene_node:
		current_scene_node.queue_free()
		current_scene_node = null

	var inst = test_scene.instantiate()
	current_scene_node = inst
	
	var gs = Engine.get_singleton("GazeServer")
	if gs:
		if active_calibration_dict.has("device_profile") and active_calibration_dict["device_profile"] != null:
			gs.set_device_profile(active_calibration_dict["device_profile"])
	
	add_child(inst)
