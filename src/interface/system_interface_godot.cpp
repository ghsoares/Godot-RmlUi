#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/translation_server.hpp>
#include <godot_cpp/classes/input.hpp>

#include "../server/rml_server.h"
#include "system_interface_godot.h"

using namespace godot;

SystemInterfaceGodot *SystemInterfaceGodot::singleton = nullptr;

SystemInterfaceGodot *SystemInterfaceGodot::get_singleton() {
	return singleton;
}

void SystemInterfaceGodot::set_context_document(const RID &p_rid) {
	current_document = p_rid;
}

double SystemInterfaceGodot::GetElapsedTime() {
	return Time::get_singleton()->get_ticks_msec() / 1000.0;
}

int SystemInterfaceGodot::TranslateString(Rml::String& translated, const Rml::String& input) {
	String text = rml_to_godot_string(input);
	String text_translated = TranslationServer::get_singleton()->translate(text);
	translated = godot_to_rml_string(text_translated);
	return 1;
}

void SystemInterfaceGodot::JoinPath(Rml::String& translated_path, const Rml::String& document_path, const Rml::String& path) {
	String doc_path = rml_to_godot_string(document_path);
	String p = rml_to_godot_string(path);

	if (p.is_absolute_path()) {
		translated_path = godot_to_rml_string(p);
		return;
	}

	String new_p = doc_path.get_base_dir().path_join(p).simplify_path();
	translated_path = godot_to_rml_string(new_p);
}

bool SystemInterfaceGodot::LogMessage(Rml::Log::Type msg_type, const Rml::String& msg) {
	switch (msg_type) {
		case Rml::Log::Type::LT_ALWAYS: {
			UtilityFunctions::print("[RmlUi] ", msg.c_str());
		} break;
		case Rml::Log::Type::LT_ERROR: {
			UtilityFunctions::push_error("[RmlUi] ", msg.c_str());
		} break;
		case Rml::Log::Type::LT_WARNING: {
			UtilityFunctions::push_warning("[RmlUi] ", msg.c_str());
		} break;
		case Rml::Log::Type::LT_INFO: {
			UtilityFunctions::print_verbose("[RmlUi] ", msg.c_str());
		} break;
		case Rml::Log::Type::LT_DEBUG: {
			if (OS::get_singleton()->is_stdout_verbose()) {
				UtilityFunctions::print("[RmlUi] ", msg.c_str());
			}
		} break;
		default: {}
	}
	return true;
}

void SystemInterfaceGodot::SetMouseCursor(const Rml::String& cursor_name) {
	Input::CursorShape shape = Input::CursorShape::CURSOR_ARROW;
	if (cursor_name == "text") {
		shape = Input::CursorShape::CURSOR_IBEAM;
	}
	else if (cursor_name == "crosshair") {
		shape = Input::CursorShape::CURSOR_CROSS;
	}
	else if (cursor_name == "pointer" || cursor_name == "hand") {
		shape = Input::CursorShape::CURSOR_POINTING_HAND;
	}
	else if (cursor_name == "col-resize" || cursor_name == "ew-resize") {
		shape = Input::CursorShape::CURSOR_HSIZE;
	}
	else if (cursor_name == "row-resize" || cursor_name == "ns-resize") {
		shape = Input::CursorShape::CURSOR_VSIZE;
	}
	else if (cursor_name == "nwse-resize") {
		shape = Input::CursorShape::CURSOR_BDIAGSIZE;
	}
	else if (cursor_name == "nesw-resize") {
		shape = Input::CursorShape::CURSOR_FDIAGSIZE;
	}
	else if (cursor_name == "all-scroll" || cursor_name == "move") {
		shape = Input::CursorShape::CURSOR_MOVE;
	}
	else if (cursor_name == "not-allowed") {
		shape = Input::CursorShape::CURSOR_FORBIDDEN;
	}
	else if (cursor_name == "help") {
		shape = Input::CursorShape::CURSOR_HELP;
	}
	else if (cursor_name == "progress") {
		shape = Input::CursorShape::CURSOR_BUSY;
	}
	else if (cursor_name == "wait") {
		shape = Input::CursorShape::CURSOR_WAIT;
	}
	if (current_document.is_valid()) {
		RMLServer::get_singleton()->document_set_cursor_shape(current_document, shape);
	}
}

void SystemInterfaceGodot::SetClipboardText(const Rml::String& text) {
	DisplayServer::get_singleton()->clipboard_set(rml_to_godot_string(text));
}

void SystemInterfaceGodot::GetClipboardText(Rml::String& text) {
	String t = DisplayServer::get_singleton()->clipboard_get();
	text = godot_to_rml_string(t);
}

void SystemInterfaceGodot::ActivateKeyboard(Rml::Vector2f caret_position, float line_height) {
	if (!DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_VIRTUAL_KEYBOARD)) {
		return;
	}
	DisplayServer::get_singleton()->virtual_keyboard_show(
		"", Rect2(caret_position.x, caret_position.y, 0, 0)
	);
}

void SystemInterfaceGodot::DeactivateKeyboard() {
	if (!DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_VIRTUAL_KEYBOARD)) {
		return;
	}
	DisplayServer::get_singleton()->virtual_keyboard_hide();
}

SystemInterfaceGodot::SystemInterfaceGodot() {
	singleton = this;
}