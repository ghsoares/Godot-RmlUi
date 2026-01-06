#include <iostream>
#include <sstream>

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>

#include "../interface/render_interface_godot.h"
#include "rml_document.h"

using namespace godot;

Rml::String godot::godot_to_rml_string(const String &p_str) {
	return Rml::String(p_str.utf8().get_data());
}

String godot::rml_to_godot_string(const Rml::String &p_str) {
	return String(p_str.c_str());
}

void RMLDocument::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			set_process(true);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			set_process(false);
		} break;
		case NOTIFICATION_PROCESS: {
			if (!ctx) { return; }
			ctx->Update();
			queue_redraw();
		} break;
		case NOTIFICATION_DRAW: {
			if (!ctx) { return; }
			RenderInterfaceGodot::get_singleton()->set_drawing_canvas_item(get_canvas_item());
			ctx->Render();
			RenderInterfaceGodot::get_singleton()->set_drawing_canvas_item(RID());
		} break;
		case NOTIFICATION_RESIZED: {
			if (!ctx) { return; }
			ctx->SetDimensions(Rml::Vector2i(get_size().x, get_size().y));
		} break;
	}
}

void RMLDocument::_gui_input(const Ref<InputEvent> &p_event) {
	if (!ctx) { return; }

	Ref<InputEventMouseButton> mb = p_event;
	Ref<InputEventMouseMotion> mm = p_event;

	if (mb.is_valid()) {
		bool interacted = !ctx->ProcessMouseButtonDown(
			mb->get_button_index() - 1,
			mb->is_pressed()
		);
		if (interacted) {
			accept_event();
		}
	}

	if (mm.is_valid()) {
		bool interacted = !ctx->ProcessMouseMove(
			mm->get_position().x,
            mm->get_position().y,
            0
		);
		if (interacted) {
			accept_event();
		}
	}
}

void RMLDocument::_bind_methods() {
	
}

RMLDocument::RMLDocument() { 
	std::stringstream ss;
	ss << "godot_context_" << get_instance_id();
	ctx = Rml::CreateContext(
		ss.str(),
		Rml::Vector2i(get_size().x, get_size().y)
	);

	doc = ctx->CreateDocument();
	doc->Show();
}

RMLDocument::~RMLDocument() {
	
}