extends RMLDocument

@onready var fps_label: Label = $fps
@onready var next_test_button: Button = $next_test

var average_fps: float = 0.0

func _ready() -> void:
	var tests = ["res://tests/rendering_interface.rml"]
	
	for test in tests:
		load_from_path(test)
		await next_test_button.pressed
	
	print("Tests finished!")
	get_tree().quit()

func _process(delta: float) -> void:
	var fps: float = 1.0 / delta
	average_fps = lerp(average_fps, fps, delta * 8.0)
	fps_label.text = "Average FPS: %.1f" % average_fps

