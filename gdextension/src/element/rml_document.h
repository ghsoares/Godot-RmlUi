#pragma once

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <RmlUi/Core.h>

namespace godot {

Rml::String godot_to_rml_string(const String &p_str);
String rml_to_godot_string(const Rml::String &p_str);

class RMLDocument : public Control {
	GDCLASS(RMLDocument, Control)

protected:
	Rml::Context *ctx = nullptr;
	Rml::ElementDocument *doc = nullptr;

	static void _bind_methods();

public:
	void _notification(int p_what);
	void _gui_input(const Ref<InputEvent> &p_event);

	RMLDocument();
	~RMLDocument();
};

}


