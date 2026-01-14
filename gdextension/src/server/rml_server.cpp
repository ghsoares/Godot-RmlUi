#include <iostream>
#include <sstream>

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include "../interface/render_interface_godot.h"
#include "../plugin/rml_godot_plugin.h"
#include "../rml_util.h"
#include "rml_server.h"

using namespace godot;

RMLServer *RMLServer::singleton = nullptr;

RMLServer *RMLServer::get_singleton() {
	return singleton;
}

void RMLServer::initialize() {
	ERR_FAIL_COND_MSG(initialized, "Already initialized");

	load_default_stylesheet("res://addons/rmlui/styles/default.rcss");
	load_font_face_from_path("res://addons/rmlui/fonts/OpenSans-VariableFont_wdth,wght.ttf");

	RenderInterfaceGodot *ri = RenderInterfaceGodot::get_singleton();
	ri->initialize();

	RenderingServer::get_singleton()->connect("frame_pre_draw", callable_mp(this, &RMLServer::render));

	initialized = true;
	Rml::Log::Message(Rml::Log::LT_INFO, "RMLServer initialized.");
}

void RMLServer::uninitialize() {
	ERR_FAIL_COND_MSG(!initialized, "Already uninitialized");
	initialized = false;
	
	RenderInterfaceGodot *ri = RenderInterfaceGodot::get_singleton();
	ri->uninitialize();

	Rml::Log::Message(Rml::Log::LT_INFO, "RMLServer uninitialized.");
}


RID RMLServer::initialize_document(const RID &p_canvas_item) {
	RID new_rid = document_owner.make_rid();
	std::stringstream context_str;
	context_str << "godot_context_" << new_rid.get_id();
	
	DocumentData *doc_data = document_owner.get_or_null(new_rid);
	doc_data->ctx = Rml::CreateContext(
		context_str.str(),
		Rml::Vector2i(1, 1)
	);

	if (doc_data->ctx == nullptr) {
		document_owner.free(new_rid);
		ERR_FAIL_V_MSG(RID(), "Couldn't create the context");
	}

	doc_data->render_frame = memnew(RenderFrame);

	return new_rid;
}

RID RMLServer::create_document(const RID &p_canvas_item) {
	RID new_rid = initialize_document(p_canvas_item);
	DocumentData *doc_data = document_owner.get_or_null(new_rid);

	doc_data->doc = doc_data->ctx->CreateDocument();
	if (doc_data->doc == nullptr) {
		Rml::RemoveContext(doc_data->ctx->GetName());
		document_owner.free(new_rid);
		ERR_FAIL_V_MSG(RID(), "Couldn't create the document");
		return RID();
	}

	doc_data->doc->Show();

	doc_data->canvas_item = p_canvas_item;

	return new_rid;
}

RID RMLServer::create_document_from_rml_string(const RID &p_canvas_item, const String &p_string) {
	RID new_rid = initialize_document(p_canvas_item);
	DocumentData *doc_data = document_owner.get_or_null(new_rid);

	doc_data->doc = doc_data->ctx->LoadDocumentFromMemory(godot_to_rml_string(p_string));
	if (doc_data->doc == nullptr) {
		Rml::RemoveContext(doc_data->ctx->GetName());
		document_owner.free(new_rid);
		ERR_FAIL_V_MSG(RID(), "Couldn't create the document");
		return RID();
	}

	doc_data->doc->Show();

	doc_data->canvas_item = p_canvas_item;

	return new_rid;
}

RID RMLServer::create_document_from_path(const RID &p_canvas_item, const String &p_path) {
	RID new_rid = initialize_document(p_canvas_item);
	DocumentData *doc_data = document_owner.get_or_null(new_rid);

	doc_data->doc = doc_data->ctx->LoadDocument(godot_to_rml_string(p_path));
	if (doc_data->doc == nullptr) {
		Rml::RemoveContext(doc_data->ctx->GetName());
		document_owner.free(new_rid);
		ERR_FAIL_V_MSG(RID(), "Couldn't create the document");
		return RID();
	}

	doc_data->doc->Show();

	doc_data->canvas_item = p_canvas_item;

	return new_rid;
}

Ref<RMLElement> RMLServer::get_document_root(const RID &p_document) {
	ERR_FAIL_COND_V(!document_owner.owns(p_document), nullptr);
	DocumentData *doc_data = document_owner.get_or_null(p_document);
	ERR_FAIL_NULL_V(doc_data, nullptr);

	return RMLElement::ref(doc_data->doc);
}

Ref<RMLElement> RMLServer::create_element(const RID &p_document, const String &p_tag_name) {
	ERR_FAIL_COND_V(!document_owner.owns(p_document), nullptr);
	DocumentData *doc_data = document_owner.get_or_null(p_document);
	ERR_FAIL_NULL_V(doc_data, nullptr);

	Rml::String tag_name = godot_to_rml_string(p_tag_name);

	return RMLElement::ref(doc_data->doc->CreateElement(tag_name));
}

void RMLServer::document_update(const RID &p_document) {
	ERR_FAIL_COND(!document_owner.owns(p_document));
	DocumentData *doc_data = document_owner.get_or_null(p_document);
	ERR_FAIL_NULL(doc_data);
	doc_data->ctx->Update();
}

void RMLServer::document_set_size(const RID &p_document, const Vector2i &p_size) {
	ERR_FAIL_COND(!document_owner.owns(p_document));
	DocumentData *doc_data = document_owner.get_or_null(p_document);
	ERR_FAIL_NULL(doc_data);
	doc_data->ctx->SetDimensions(Rml::Vector2i(
		p_size.x,
		p_size.y
	));
}

bool RMLServer::document_process_event(const RID &p_document, const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND_V(!document_owner.owns(p_document), false);
	DocumentData *doc_data = document_owner.get_or_null(p_document);
	ERR_FAIL_NULL_V(doc_data, false);
	
	Ref<InputEventKey> k = p_event;
	if (k.is_valid()) {
		if (k->is_pressed()) {
			Rml::Input::KeyIdentifier key_identifier = godot_to_rml_key(k->get_keycode());
			bool propagated = doc_data->ctx->ProcessKeyDown(
				key_identifier, 
				godot_to_rml_key_modifiers(k->get_modifiers_mask())
			);
			char32_t c = k->get_unicode();
			if (((c >= 32 || c == '\n') && c != 127) && !(k->get_modifiers_mask() & KeyModifierMask::KEY_MASK_CTRL)) {
				propagated &= doc_data->ctx->ProcessTextInput(c);
			}
			return !propagated;
		}
		return !doc_data->ctx->ProcessKeyUp(
			godot_to_rml_key(k->get_keycode()), 
			godot_to_rml_key_modifiers(k->get_modifiers_mask())
		);
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->is_pressed()) {
			switch (mb->get_button_index()) {
				case MouseButton::MOUSE_BUTTON_LEFT: return !doc_data->ctx->ProcessMouseButtonDown(
					0,
					godot_to_rml_key_modifiers(mb->get_modifiers_mask())
				);
				case MouseButton::MOUSE_BUTTON_RIGHT: return !doc_data->ctx->ProcessMouseButtonDown(
					1,
					godot_to_rml_key_modifiers(mb->get_modifiers_mask())
				);
				case MouseButton::MOUSE_BUTTON_MIDDLE: return !doc_data->ctx->ProcessMouseButtonDown(
					2,
					godot_to_rml_key_modifiers(mb->get_modifiers_mask())
				);
				case MouseButton::MOUSE_BUTTON_WHEEL_UP: return !doc_data->ctx->ProcessMouseWheel(
					1.0,
					godot_to_rml_key_modifiers(mb->get_modifiers_mask())
				);
				case MouseButton::MOUSE_BUTTON_WHEEL_DOWN: return !doc_data->ctx->ProcessMouseWheel(
					-1.0,
					godot_to_rml_key_modifiers(mb->get_modifiers_mask())
				);
			}
		}
		switch (mb->get_button_index()) {
			case MouseButton::MOUSE_BUTTON_LEFT: return !doc_data->ctx->ProcessMouseButtonUp(
				0,
				godot_to_rml_key_modifiers(mb->get_modifiers_mask())
			);
			case MouseButton::MOUSE_BUTTON_RIGHT: return !doc_data->ctx->ProcessMouseButtonUp(
				1,
				godot_to_rml_key_modifiers(mb->get_modifiers_mask())
			);
			case MouseButton::MOUSE_BUTTON_MIDDLE: return !doc_data->ctx->ProcessMouseButtonUp(
				2,
				godot_to_rml_key_modifiers(mb->get_modifiers_mask())
			);
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		Vector2 mpos = mm->get_position();
		Rml::Vector2i ctx_size = doc_data->ctx->GetDimensions();
		if (mpos.x < 0 || mpos.x >= ctx_size.x || mpos.y < 0 || mpos.y >= ctx_size.y) {
			return !doc_data->ctx->ProcessMouseLeave();
		}
		return !doc_data->ctx->ProcessMouseMove(
			mpos.x,
            mpos.y,
            godot_to_rml_key_modifiers(mm->get_modifiers_mask())
		);
	}

	Ref<InputEventScreenTouch> touch = p_event;
	if (touch.is_valid()) {
		Rml::TouchList list;
		list.push_back(Rml::Touch {
			.identifier = (unsigned int)touch->get_index(),
			.position = Rml::Vector2f(
				touch->get_position().x,
				touch->get_position().y
			)
		});
		if (touch->is_canceled()){
			return !doc_data->ctx->ProcessTouchCancel(list);
		}
		if (touch->is_pressed()) {
			return !doc_data->ctx->ProcessTouchStart(list, 0);
		} else {
			return !doc_data->ctx->ProcessTouchEnd(list, 0);
		}
	}

	Ref<InputEventScreenDrag> drag = p_event;
	if (drag.is_valid()) {
		Rml::TouchList list;
		list.push_back(Rml::Touch {
			.identifier = (unsigned int)drag->get_index(),
			.position = Rml::Vector2f(
				drag->get_position().x,
				drag->get_position().y
			)
		});
		return !doc_data->ctx->ProcessTouchMove(list, 0);
	}

	return false;
}

bool RMLServer::load_default_stylesheet(const String &p_path) {
	Rml::SharedPtr<Rml::StyleSheetContainer> ss = Rml::Factory::InstanceStyleSheetFile(godot_to_rml_string(p_path));
	ERR_FAIL_NULL_V(ss, false);

	RmlPluginGodot::get_singleton()->set_default_stylesheet(ss);

	return true;
}

bool RMLServer::load_font_face_from_path(const String &p_path, bool p_fallback_face) {
	return Rml::LoadFontFace(godot_to_rml_string(p_path), p_fallback_face);
}

bool RMLServer::load_font_face_from_buffer(const PackedByteArray &p_buffer, const String &p_family, bool p_fallback_face, bool p_is_italic) {
	return Rml::LoadFontFace(
		Rml::Span(p_buffer.ptr(), p_buffer.size()), 
		godot_to_rml_string(p_family), 
		p_is_italic ? Rml::Style::FontStyle::Italic : Rml::Style::FontStyle::Normal,
		Rml::Style::FontWeight::Auto,
		p_fallback_face
	);
}

void RMLServer::free_render_frame(uint64_t p_frame) {
	RenderFrame *frame = reinterpret_cast<RenderFrame *>(p_frame);

	RenderInterfaceGodot *ri = RenderInterfaceGodot::get_singleton();
	ri->free_render_frame(frame);

	memdelete(frame);
}

void RMLServer::free_rid(const RID &p_rid) {
	if (document_owner.owns(p_rid)) {
		DocumentData *doc_data = document_owner.get_or_null(p_rid);
		Rml::RemoveContext(doc_data->ctx->GetName());

		RenderingServer *rs = RenderingServer::get_singleton();
		if (doc_data->render_frame->is_valid()) {
			rs->call_on_render_thread(callable_mp(this, &RMLServer::free_render_frame).bind(reinterpret_cast<uint64_t>(doc_data->render_frame)));
		}

		document_owner.free(p_rid);
	}
}

void RMLServer::render() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs->is_on_render_thread()) {
		rs->call_on_render_thread(callable_mp(this, &RMLServer::render));
		return;
	}
	
	RenderInterfaceGodot *ri = RenderInterfaceGodot::get_singleton();

	List<RID> contexts;
	document_owner.get_owned_list(&contexts);
	for (RID doc_rid : contexts) {
		DocumentData *doc = document_owner.get_or_null(doc_rid);
		doc->ctx->Update();

		rs->canvas_item_clear(doc->canvas_item);

		Vector2i size = Vector2i(
			doc->ctx->GetDimensions().x,
			doc->ctx->GetDimensions().y
		);
		if (size.x <= 0 || size.y <= 0) {
			continue;
		}

		doc->render_frame->desired_size = size;

		ri->set_render_frame(doc->render_frame);
		doc->ctx->Render();
		ri->set_render_frame(nullptr);
		rs->canvas_item_add_texture_rect(
			doc->canvas_item, 
			Rect2(0, 0, size.x, size.y),
			doc->render_frame->get_texture()
		);
	}
}

void RMLServer::_bind_methods() {;
	ClassDB::bind_method(D_METHOD("is_initialized"), &RMLServer::is_initialized);

	ClassDB::bind_method(D_METHOD("initialize"), &RMLServer::initialize);
	ClassDB::bind_method(D_METHOD("uninitialize"), &RMLServer::uninitialize);

	ClassDB::bind_method(D_METHOD("create_document", "canvas_item"), &RMLServer::create_document);
	ClassDB::bind_method(D_METHOD("create_document_from_rml_string", "canvas_item", "rml"), &RMLServer::create_document_from_rml_string);
	ClassDB::bind_method(D_METHOD("create_document_from_path", "canvas_item", "path"), &RMLServer::create_document_from_path);
	ClassDB::bind_method(D_METHOD("get_document_root", "document"), &RMLServer::get_document_root);
	ClassDB::bind_method(D_METHOD("create_element", "document", "tag_name"), &RMLServer::create_element);
	
	ClassDB::bind_method(D_METHOD("document_set_size", "document", "size"), &RMLServer::document_set_size);
	ClassDB::bind_method(D_METHOD("document_process_event", "document", "event"), &RMLServer::document_process_event);

	ClassDB::bind_method(D_METHOD("load_font_face_from_path", "path", "fallback_face"), &RMLServer::load_font_face_from_path, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("load_font_face_from_buffer", "buffer", "family", "fallback_face", "is_italic"), &RMLServer::load_font_face_from_buffer, DEFVAL(false), DEFVAL(false));

	ClassDB::bind_method(D_METHOD("free_rid", "rid"), &RMLServer::free_rid);
}

RMLServer::RMLServer() {
	singleton = this;
}

