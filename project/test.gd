extends RMLDocument

@onready var fps_label: Label = $fps
@onready var next_test_button: Button = $next_test
@onready var toggle_vsync_button: Button = $toggle_vsync

var average_fps: float = 0.0

func _ready() -> void:
	# get_viewport().disable_3d = true
	# get_viewport().use_hdr_2d = true
	toggle_vsync_button.pressed.connect(toggle_vsync)

	var tests = ["res://tests/rendering_interface.rml"]
	
	for i in tests.size():
		if i == tests.size() - 1:
			next_test_button.text = "Finish Tests"
		load_from_path(tests[i])
		await next_test_button.pressed
	
	print("Tests finished!")
	get_tree().quit()

func _process(delta: float) -> void:
	var fps: float = 1.0 / delta
	var vsync_mode: DisplayServer.VSyncMode = DisplayServer.window_get_vsync_mode()
	average_fps = lerp(average_fps, fps, delta * 8.0)
	fps_label.text = "Average FPS (%s): %.1f" % [
		"VSync Off" if vsync_mode == DisplayServer.VSYNC_DISABLED else "VSync On",
		average_fps
	]

func toggle_vsync() -> void:
	var mode: DisplayServer.VSyncMode = DisplayServer.window_get_vsync_mode()
	if mode == DisplayServer.VSYNC_DISABLED:
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_ENABLED)
	else:
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)

