import os

def configure_pages(gen):
	index_page = gen.get_template('generic.html')
	gen.add_page({
		"name": "Godot RmlUi",
		"href": "$BASE_URL/",
		"location": "/",
		"filename": gen.dist_path("index.html"),
		"content": index_page
	})



