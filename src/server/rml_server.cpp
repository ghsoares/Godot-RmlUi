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
#include <godot_cpp/classes/file_access.hpp>
#include "../interface/render_interface_godot.h"
#include "../interface/system_interface_godot.h"
#include "../plugin/rml_godot_plugin.h"
#include "../rml_util.h"
#include "../util.h"
#include "../project_settings.h"
#include "rml_server.h"

using namespace godot;

RMLServer *RMLServer::singleton = nullptr;

RMLServer *RMLServer::get_singleton() {
	return singleton;
}

void RMLServer::load_resources() {
	bool load_user_agent_stylesheet = GLOBAL_GET("RmlUi/load_user_agent_stylesheet");

	if (load_user_agent_stylesheet) {
		String custom_user_agent_stylesheet = GLOBAL_GET("RmlUi/custom_user_agent_stylesheet");
		bool has_custom_user_agent = custom_user_agent_stylesheet.is_valid_filename() && FileAccess::file_exists(custom_user_agent_stylesheet);
		if (has_custom_user_agent) {
			load_default_stylesheet(custom_user_agent_stylesheet);
		} else {
			load_default_stylesheet("res://addons/rmlui/styles/default.rcss");
		}
	}
	load_font_face_from_path("res://addons/rmlui/fonts/OpenSans-VariableFont_wdth,wght.ttf");
}

void RMLServer::initialize() {
	RenderInterfaceGodot *ri = dynamic_cast<RenderInterfaceGodot *>(Rml::GetRenderInterface());
	ERR_FAIL_NULL_MSG(ri, "Render interface configured is not of type RenderInterfaceGodot");
	ri->initialize();

	Rml::Log::Message(Rml::Log::LT_INFO, "RMLServer initialized.");
}

void RMLServer::uninitialize() {
	RenderInterfaceGodot *ri = dynamic_cast<RenderInterfaceGodot *>(Rml::GetRenderInterface());
	ERR_FAIL_NULL_MSG(ri, "Render interface configured is not of type RenderInterfaceGodot");
	ri->finalize();

	Rml::Log::Message(Rml::Log::LT_INFO, "RMLServer uninitialized.");
}

RID RMLServer::initialize_document() {
	RenderInterfaceGodot *ri = dynamic_cast<RenderInterfaceGodot *>(Rml::GetRenderInterface());
	ERR_FAIL_NULL_V_MSG(ri, RID(), "Render interface configured is not of type RenderInterfaceGodot");

	RID new_rid = document_owner.make_rid();
	String context_id = vformat("godot_context_%d", new_rid.get_id());
	DocumentData *doc_data = document_owner.get_or_null(new_rid);
	doc_data->ctx = Rml::CreateContext(
		godot_to_rml_string(context_id),
		Rml::Vector2i(1, 1)
	);
	
	if (doc_data->ctx == nullptr) {
		document_owner.free(new_rid);
		ERR_FAIL_V_MSG(RID(), "Couldn't create the context");
	}

	return new_rid;
}

RID RMLServer::create_document() {
	RID new_rid = initialize_document();
	ERR_FAIL_COND_V(!new_rid.is_valid(), RID());
	DocumentData *doc_data = document_owner.get_or_null(new_rid);

	doc_data->doc = doc_data->ctx->CreateDocument();
	if (doc_data->doc == nullptr) {
		Rml::RemoveContext(doc_data->ctx->GetName());
		document_owner.free(new_rid);
		ERR_FAIL_V_MSG(RID(), "Couldn't create the document");
		return RID();
	}

	doc_data->doc->Show();

	return new_rid;
}

RID RMLServer::create_document_from_rml_string(const String &p_string) {
	RID new_rid = initialize_document();
	ERR_FAIL_COND_V(!new_rid.is_valid(), RID());
	DocumentData *doc_data = document_owner.get_or_null(new_rid);

	doc_data->doc = doc_data->ctx->LoadDocumentFromMemory(godot_to_rml_string(p_string));
	if (doc_data->doc == nullptr) {
		Rml::RemoveContext(doc_data->ctx->GetName());
		document_owner.free(new_rid);
		ERR_FAIL_V_MSG(RID(), "Couldn't create the document");
		return RID();
	}

	doc_data->doc->Show();

	return new_rid;
}

RID RMLServer::create_document_from_path(const String &p_path) {
	RID new_rid = initialize_document();
	ERR_FAIL_COND_V(!new_rid.is_valid(), RID());
	DocumentData *doc_data = document_owner.get_or_null(new_rid);

	doc_data->doc = doc_data->ctx->LoadDocument(godot_to_rml_string(p_path));
	if (doc_data->doc == nullptr) {
		Rml::RemoveContext(doc_data->ctx->GetName());
		document_owner.free(new_rid);
		ERR_FAIL_V_MSG(RID(), "Couldn't create the document");
		return RID();
	}

	doc_data->doc->Show();

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

	SystemInterfaceGodot::get_singleton()->set_context_document(p_document);
	bool propagated = true;
	
	Ref<InputEventKey> k = p_event;
	if (k.is_valid()) {
		if (k->is_pressed()) {
			Rml::Input::KeyIdentifier key_identifier = godot_to_rml_key(k->get_keycode());
			propagated = doc_data->ctx->ProcessKeyDown(
				key_identifier, 
				godot_to_rml_key_modifiers(k->get_modifiers_mask())
			);
			char32_t c = k->get_unicode();
			if (((c >= 32 || c == '\n') && c != 127) && !(k->get_modifiers_mask() & KeyModifierMask::KEY_MASK_CTRL)) {
				propagated &= doc_data->ctx->ProcessTextInput(c);
			}
		} else {
			propagated = doc_data->ctx->ProcessKeyUp(
				godot_to_rml_key(k->get_keycode()), 
				godot_to_rml_key_modifiers(k->get_modifiers_mask())
			);
		}
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->is_pressed()) {
			switch (mb->get_button_index()) {
				case MouseButton::MOUSE_BUTTON_LEFT: {
					propagated = doc_data->ctx->ProcessMouseButtonDown(
						0,
						godot_to_rml_key_modifiers(mb->get_modifiers_mask())
					);
				} break;
				case MouseButton::MOUSE_BUTTON_RIGHT: {
					propagated = doc_data->ctx->ProcessMouseButtonDown(
						1,
						godot_to_rml_key_modifiers(mb->get_modifiers_mask())
					);
				} break;
				case MouseButton::MOUSE_BUTTON_MIDDLE: {
					propagated = doc_data->ctx->ProcessMouseButtonDown(
						2,
						godot_to_rml_key_modifiers(mb->get_modifiers_mask())
					);
				} break;
				case MouseButton::MOUSE_BUTTON_WHEEL_UP: {
					Rml::Vector2f delta = Rml::Vector2f(0.0, -1.0);
					if (mb->get_modifiers_mask() && KeyModifierMask::KEY_MASK_SHIFT) {
						delta = Rml::Vector2f(-1.0, 0.0);
					}
					propagated = doc_data->ctx->ProcessMouseWheel(
						delta,
						godot_to_rml_key_modifiers(mb->get_modifiers_mask())
					);
				} break;
				case MouseButton::MOUSE_BUTTON_WHEEL_DOWN: {
					Rml::Vector2f delta = Rml::Vector2f(0.0, 1.0);
					if (mb->get_modifiers_mask() && KeyModifierMask::KEY_MASK_SHIFT) {
						delta = Rml::Vector2f(1.0, 0.0);
					}
					propagated = doc_data->ctx->ProcessMouseWheel(
						delta,
						godot_to_rml_key_modifiers(mb->get_modifiers_mask())
					);
				} break;
				default: {}
			}
		} else {
			switch (mb->get_button_index()) {
				case MouseButton::MOUSE_BUTTON_LEFT: {
					propagated = doc_data->ctx->ProcessMouseButtonUp(
						0,
						godot_to_rml_key_modifiers(mb->get_modifiers_mask())
					);
				} break;
				case MouseButton::MOUSE_BUTTON_RIGHT: {
					propagated = doc_data->ctx->ProcessMouseButtonUp(
						1,
						godot_to_rml_key_modifiers(mb->get_modifiers_mask())
					);
				} break;
				case MouseButton::MOUSE_BUTTON_MIDDLE: {
					propagated = doc_data->ctx->ProcessMouseButtonUp(
						2,
						godot_to_rml_key_modifiers(mb->get_modifiers_mask())
					);
				} break;
				default: {}
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		Vector2 mpos = mm->get_position();
		Rml::Vector2i ctx_size = doc_data->ctx->GetDimensions();
		if (mpos.x < 0 || mpos.x >= ctx_size.x || mpos.y < 0 || mpos.y >= ctx_size.y) {
			propagated = doc_data->ctx->ProcessMouseLeave();
		} else {
			propagated = doc_data->ctx->ProcessMouseMove(
				mpos.x,
				mpos.y,
				godot_to_rml_key_modifiers(mm->get_modifiers_mask())
			);
		}
	}

	Ref<InputEventScreenTouch> touch = p_event;
	if (touch.is_valid()) {
		Rml::Touch touch_info;
		touch_info.identifier = (unsigned int)touch->get_index();
		touch_info.position = Rml::Vector2f(
			touch->get_position().x,
			touch->get_position().y
		);
		Rml::TouchList list;
		list.push_back(touch_info);
		if (touch->is_canceled()){
			propagated = doc_data->ctx->ProcessTouchCancel(list);
		} else if (touch->is_pressed()) {
			propagated = doc_data->ctx->ProcessTouchStart(list, 0);
		} else {
			propagated = doc_data->ctx->ProcessTouchEnd(list, 0);
		}
	}

	Ref<InputEventScreenDrag> drag = p_event;
	if (drag.is_valid()) {
		Rml::Touch touch_info;
		touch_info.identifier = (unsigned int)drag->get_index();
		touch_info.position = Rml::Vector2f(
			drag->get_position().x,
			drag->get_position().y
		);
		Rml::TouchList list;
		list.push_back(touch_info);
		propagated = doc_data->ctx->ProcessTouchMove(list, 0);
	}

	SystemInterfaceGodot::get_singleton()->set_context_document(RID());

	return !propagated;
}

void RMLServer::document_set_cursor_shape(const RID &p_document, const Input::CursorShape &p_shape) {
	ERR_FAIL_COND(!document_owner.owns(p_document));
	DocumentData *doc_data = document_owner.get_or_null(p_document);
	ERR_FAIL_NULL(doc_data);

	doc_data->cursor_shape = p_shape;
}

Input::CursorShape RMLServer::document_get_cursor_shape(const RID &p_document) {
	ERR_FAIL_COND_V(!document_owner.owns(p_document), Input::CURSOR_ARROW);
	DocumentData *doc_data = document_owner.get_or_null(p_document);
	ERR_FAIL_NULL_V(doc_data, Input::CURSOR_ARROW);

	return doc_data->cursor_shape;
}

void RMLServer::document_draw(const RID &p_document, const RID &p_canvas_item) {
	RenderInterfaceGodot *ri = dynamic_cast<RenderInterfaceGodot *>(Rml::GetRenderInterface());
	ERR_FAIL_NULL_MSG(ri, "Render interface configured is not of type RenderInterfaceGodot");

	ERR_FAIL_COND(!document_owner.owns(p_document));
	DocumentData *doc_data = document_owner.get_or_null(p_document);
	ERR_FAIL_NULL(doc_data);

	Vector2i size = Vector2i(
		doc_data->ctx->GetDimensions().x,
		doc_data->ctx->GetDimensions().y
	);
	if (size.x <= 0 || size.y <= 0) {
		return;
	}

	ri->push_context(doc_data->draw_context, size);
	doc_data->ctx->Render();
	ri->pop_context();
	ri->draw_context(doc_data->draw_context, p_canvas_item);
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

void RMLServer::free_rid(const RID &p_rid) {
	RenderInterfaceGodot *ri = dynamic_cast<RenderInterfaceGodot *>(Rml::GetRenderInterface());
	ERR_FAIL_NULL_MSG(ri, "Render interface configured is not of type RenderInterfaceGodot");

	if (document_owner.owns(p_rid)) {
		DocumentData *doc_data = document_owner.get_or_null(p_rid);
		Rml::RemoveContext(doc_data->ctx->GetName());
		
		ri->free_context(doc_data->draw_context);
		
		document_owner.free(p_rid);
	}
}

void RMLServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_document"), &RMLServer::create_document);
	ClassDB::bind_method(D_METHOD("create_document_from_rml_string", "rml"), &RMLServer::create_document_from_rml_string);
	ClassDB::bind_method(D_METHOD("create_document_from_path", "path"), &RMLServer::create_document_from_path);
	ClassDB::bind_method(D_METHOD("get_document_root", "document"), &RMLServer::get_document_root);
	ClassDB::bind_method(D_METHOD("create_element", "document", "tag_name"), &RMLServer::create_element);
	
	ClassDB::bind_method(D_METHOD("document_set_size", "document", "size"), &RMLServer::document_set_size);
	ClassDB::bind_method(D_METHOD("document_process_event", "document", "event"), &RMLServer::document_process_event);

	ClassDB::bind_method(D_METHOD("document_update", "document"), &RMLServer::document_update);
	ClassDB::bind_method(D_METHOD("document_draw", "document", "canvas_item"), &RMLServer::document_draw);

	ClassDB::bind_method(D_METHOD("load_font_face_from_path", "path", "fallback_face"), &RMLServer::load_font_face_from_path, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("load_font_face_from_buffer", "buffer", "family", "fallback_face", "is_italic"), &RMLServer::load_font_face_from_buffer, DEFVAL(false), DEFVAL(false));

	ClassDB::bind_method(D_METHOD("free_rid", "rid"), &RMLServer::free_rid);
}

RMLServer::RMLServer() {
	singleton = this;
}

