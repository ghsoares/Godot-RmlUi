
def configure_pages(gen):
	index_page = gen.get_template('generic.html')
	gen.add_page({
		"name": "Godot RmlUi",
		"href": "/index.html",
		"location": "/",
		"content": index_page
	})



