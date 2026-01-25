extends RMLDocument

func _ready() -> void:
	load_from_path("res://test.rml")
	
	var doc: RMLElement = as_element()
	var el: RMLElement = create_element("div")
	doc.append_child(el)
	
	await get_tree().create_timer(1.0).timeout
	
	el.set_text_content("Test reference")
	
	await get_tree().create_timer(1.0).timeout
	
	doc.remove_child(el)
