#include <iostream>
#include <sstream>

#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_key.hpp>

#include "../server/rml_server.h"
#include "rml_element.h"
#include "rml_document.h"

#include "../util.h"

using namespace godot;

void RMLDocument::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			RMLServer::get_singleton()->document_set_size(rid, Vector2i(
				get_size().x,
				get_size().y
			));
			set_process(true);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			set_process(false);
		} break;
		case NOTIFICATION_PROCESS: {
			update();
			queue_redraw();
		} break;
		case NOTIFICATION_DRAW: {
			RMLServer::get_singleton()->document_draw(rid, get_canvas_item(), get_global_rect().position);
		} break;
		case NOTIFICATION_RESIZED: {
			RMLServer::get_singleton()->document_set_size(rid, Vector2i(
				get_size().x,
				get_size().y
			));
		} break;
		case NOTIFICATION_MOUSE_EXIT: {
			Ref<InputEventMouseMotion> mm;
			mm.instantiate();
			mm->set_position(Vector2(INFINITY, INFINITY));
			RMLServer::get_singleton()->document_process_event(rid, mm);
		} break;
		case NOTIFICATION_FOCUS_EXIT: {

		} break;
	}
}

void RMLDocument::_gui_input(const Ref<InputEvent> &p_event) {
	if (RMLServer::get_singleton()->document_process_event(rid, p_event)) {
		accept_event();
		Control::CursorShape new_shape = (Control::CursorShape)RMLServer::get_singleton()->document_get_cursor_shape(rid);
		set_default_cursor_shape(new_shape);
	}
}

void RMLDocument::new_document() {
	if (rid.is_valid()) {
		RMLServer::get_singleton()->free_rid(rid);
	}
	rid = RMLServer::get_singleton()->create_document();
	RMLServer::get_singleton()->document_set_size(rid, get_size());
}

void RMLDocument::load_from_rml_string(const String &p_rml) {
	if (rid.is_valid()) {
		RMLServer::get_singleton()->free_rid(rid);
	}
	rid = RMLServer::get_singleton()->create_document_from_rml_string(p_rml);
	RMLServer::get_singleton()->document_set_size(rid, get_size());
}

void RMLDocument::load_from_path(const String &p_path) {
	if (rid.is_valid()) {
		RMLServer::get_singleton()->free_rid(rid);
	}
	rid = RMLServer::get_singleton()->create_document_from_path(p_path);
	RMLServer::get_singleton()->document_set_size(rid, get_size());
}

void RMLDocument::update() {
	RMLServer::get_singleton()->document_update(rid);
}

Ref<RMLElement> RMLDocument::as_element() const {
	return RMLServer::get_singleton()->get_document_root(rid);
}

Ref<RMLElement> RMLDocument::create_element(const String &p_tag_name) const {
	return RMLServer::get_singleton()->create_element(rid, p_tag_name);
}

void RMLDocument::_bind_methods() {
	ClassDB::bind_method(D_METHOD("new_document"), &RMLDocument::new_document);
	ClassDB::bind_method(D_METHOD("load_from_rml_string", "rml_string"), &RMLDocument::load_from_rml_string);
	ClassDB::bind_method(D_METHOD("load_from_path", "path"), &RMLDocument::load_from_path);
	ClassDB::bind_method(D_METHOD("update"), &RMLDocument::update);

	ClassDB::bind_method(D_METHOD("as_element"), &RMLDocument::as_element);
	ClassDB::bind_method(D_METHOD("create_element", "tag_name"), &RMLDocument::create_element);
}

RMLDocument::RMLDocument() { 
	set_focus_mode(FocusMode::FOCUS_ALL);
	new_document();
}

RMLDocument::~RMLDocument() {
	if (rid.is_valid()) {
		RMLServer::get_singleton()->free_rid(rid);
	}
}