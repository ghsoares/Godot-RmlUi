#pragma once

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <RmlUi/Core.h>



namespace godot {

class RMLElement;

class RMLDocument : public Control {
	GDCLASS(RMLDocument, Control)

protected:
	RID rid = RID();

	static void _bind_methods();

public:
	void _notification(int p_what);
	void _gui_input(const Ref<InputEvent> &p_event) override;

	void new_document();
	void load_from_rml_string(const String &p_rml);
	void load_from_path(const String &p_path);
	void update();

	Ref<RMLElement> as_element() const;
	Ref<RMLElement> create_element(const String &p_tag_name) const;

	RMLDocument();
	~RMLDocument();
};

}


