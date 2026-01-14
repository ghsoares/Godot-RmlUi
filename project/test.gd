extends RMLDocument

var current_text: String

func _ready() -> void:
	load_from_path("res://test.rml")
	
	as_element().query_selector("button").add_event_listener("click", func (_ev):
		as_element().toggle_class("trippy")
	)
	as_element().query_selector("input").add_event_listener("change", func (ev):
		current_text = ev.value
		update_name_text()
	)
	update_name_text()

func update_name_text() -> void:
	if current_text.strip_edges().is_empty():
		as_element().query_selector("#name").set_text_content("Enter your name above")
	else:
		as_element().query_selector("#name").set_text_content("Your name is: %s!" % current_text.strip_edges())
