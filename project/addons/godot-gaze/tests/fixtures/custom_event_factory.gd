extends GazeEventFactory

func create_gaze_event() -> InputEventGazeBase:
	var gs = Engine.get_singleton("GazeServer")
	var ev = gs.create_default_event() if gs else InputEventGaze.new()
	ev.set_meta("custom_factory_applied", true)
	return ev

func create_missing_event(reason: int) -> InputEventGazeBase:
	var gs = Engine.get_singleton("GazeServer")
	var ev = gs.create_default_missing_event(reason) if gs else InputEventGazeMissing.new()
	ev.set_meta("custom_factory_applied", true)
	return ev
