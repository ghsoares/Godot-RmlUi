#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/templates/rid_owner.hpp>
#include <RmlUi/Core.h>

#include "../element/rml_element.h"
#include "../rendering/rendering_resources.h"

namespace godot {

class RMLElement;
class RMLDocument;
class RMLEventListener;

class RMLServer: public Object {
	GDCLASS(RMLServer, Object);

	static RMLServer *singleton;

	bool initialized = false;

	struct DocumentData;

	struct DocumentData {
		Rml::Context *ctx;
		Rml::ElementDocument *doc;
		RID canvas_item;
		RenderFrame *render_frame;
	};

	RID_Owner<DocumentData> document_owner;

	void render();

	RID initialize_document(const RID &p_canvas_item);
	void free_render_frame(uint64_t p_frame);
protected:
	static void _bind_methods();
	
public:
	static RMLServer *get_singleton();

	bool is_initialized() const { return initialized; }

	void initialize();
	void uninitialize();

	RID create_document(const RID &p_canvas_item);
	RID create_document_from_rml_string(const RID &p_canvas_item, const String &p_string);
	RID create_document_from_path(const RID &p_canvas_item, const String &p_path);
	Ref<RMLElement> get_document_root(const RID &p_document);
	Ref<RMLElement> create_element(const RID &p_document, const String &p_tag_name);

	void document_update(const RID &p_document);
	void document_set_size(const RID &p_document, const Vector2i &p_size);
	bool document_process_event(const RID &p_document, const Ref<InputEvent> &p_event);

	bool load_default_stylesheet(const String &p_path);

	bool load_font_face_from_path(const String &p_path, bool p_fallback_face = false);
	bool load_font_face_from_buffer(const PackedByteArray &p_buffer, const String &p_family, bool p_fallback_face = false, bool p_is_italic = false);

	void free_rid(const RID &p_rid);

	RMLServer();
};

}