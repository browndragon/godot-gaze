extends "res://addons/gut/test.gd"

class CustomTestEvent extends InputEventGaze:
	var custom_metric: float = 0.0
	var custom_tag: String = ""

class CustomEventFactory extends GazeEventFactory:
	var base_factory: GazeEventFactory = null

	func create_gaze_event() -> InputEventGazeBase:
		var base = base_factory.create_gaze_event() if base_factory else GazeServer.create_default_event()
		var custom = CustomTestEvent.new()
		custom.copy_from(base)
		custom.custom_metric = 0.99
		custom.custom_tag = "augmented"
		return custom

func test_copy_from_gaze_event():
	var base = InputEventGaze.new()
	base.window_id = 2
	base.frame_id = 100
	base.timestamp_usec = 987654321
	base.left_eye_openness = 0.92
	base.right_eye_openness = 0.88
	base.position = Vector2(500, 300)
	base.velocity = Vector2(100, -50)
	base.head_transform = Transform3D(Basis(), Vector3(10, 20, 550))
	base.gaze_transform = Transform3D(Basis(), Vector3(5, 10, 0))

	var custom = CustomTestEvent.new()
	custom.copy_from(base)
	custom.custom_metric = 42.0

	assert_eq(custom.window_id, 2, "Window ID should match")
	assert_eq(custom.frame_id, 100, "Frame ID should match")
	assert_eq(custom.timestamp_usec, 987654321, "Timestamp should match")
	assert_almost_eq(custom.left_eye_openness, 0.92, 0.001, "Left eye openness should match")
	assert_almost_eq(custom.right_eye_openness, 0.88, 0.001, "Right eye openness should match")
	assert_eq(custom.position, Vector2(500, 300), "Position should match")
	assert_eq(custom.velocity, Vector2(100, -50), "Velocity should match")
	assert_eq(custom.head_transform.origin, Vector3(10, 20, 550), "Head transform origin should match")
	assert_eq(custom.custom_metric, 42.0, "Custom metric should be set")

func test_gaze_server_event_factory_composition():
	var gs = Engine.get_singleton("GazeServer")
	assert_not_null(gs, "GazeServer should exist")

	var def_factory = GazeServerEventFactory.new()
	var custom_factory = CustomEventFactory.new()
	custom_factory.base_factory = def_factory

	var event = custom_factory.create_gaze_event()
	assert_not_null(event, "Created event should not be null")
	assert_true(event is CustomTestEvent, "Event should be instance of CustomTestEvent")
	var cast_event = event as CustomTestEvent
	assert_eq(cast_event.custom_metric, 0.99, "Custom metric should be augmented")
	assert_eq(cast_event.custom_tag, "augmented", "Custom tag should be augmented")
