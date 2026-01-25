#include "rml_element.h"
#include "rml_event_listener.h"
#include "../server/rml_server.h"
#include "../rml_util.h"

using namespace godot;

#ifdef ELEMENT_REFERENCE_STRICT
#define ENSURE_VALID(ref) ERR_FAIL_NULL_MSG(ref, #ref " is invalid"); ERR_FAIL_NULL_MSG(ref->element, #ref " is invalid");
#define ENSURE_VALID_V(ref, val) ERR_FAIL_NULL_V_MSG(ref, val, #ref " is invalid"); ERR_FAIL_NULL_V_MSG(ref->element, val, #ref " is invalid");
#else
#define ENSURE_VALID(ref) if (ref == nullptr || !ref->element.is_valid()) { return; }
#define ENSURE_VALID_V(ref, val) if (ref == nullptr || !ref->element.is_valid()) { return val; }
#endif

bool RMLElement::is_valid() const {
	return this->element.is_valid();
}

void RMLElement::append_child(const Ref<RMLElement> &p_child) {
	ENSURE_VALID(this);
	ENSURE_VALID(p_child);
	ERR_FAIL_COND(p_child->element->GetParentNode() != nullptr);
	
	element->AppendChild(p_child->element.pop_owned());
}

void RMLElement::remove_child(const Ref<RMLElement> &p_child) {
	ENSURE_VALID(this);
	ENSURE_VALID(p_child);
	ERR_FAIL_COND(p_child->element->GetParentNode() != element.get());

	Rml::ElementPtr el = element->RemoveChild(p_child->element.get());
	p_child->element.push_owner(std::move(el));
}

Ref<RMLElement> RMLElement::query_selector(const String &p_selector) const {
	ENSURE_VALID_V(this, RMLElement::empty());
	return RMLElement::ref(element->QuerySelector(godot_to_rml_string(p_selector)));
}

TypedArray<RMLElement> RMLElement::query_selector_all(const String &p_selector) const {
	ENSURE_VALID_V(this, TypedArray<RMLElement>());
	TypedArray<RMLElement> ret;

	Rml::ElementList list;
	element->QuerySelectorAll(list, godot_to_rml_string(p_selector));

	for (Rml::Element *el : list) {
		ret.append(RMLElement::ref(el));
	}

	return ret;
}

int RMLElement::get_child_count() const {
	ENSURE_VALID_V(this, 0);
	return element->GetNumChildren();
}

Ref<RMLElement> RMLElement::get_parent() const {
	ENSURE_VALID_V(this, RMLElement::empty());
	return RMLElement::ref(element->GetParentNode());
}

Ref<RMLElement> RMLElement::get_child(int p_idx) const {
	ENSURE_VALID_V(this, RMLElement::empty());
	int count = element->GetNumChildren();
	ERR_FAIL_INDEX_V(p_idx, count, RMLElement::empty());
	return RMLElement::ref(element->GetChild(p_idx));
}

TypedArray<RMLElement> RMLElement::get_children() const {
	ENSURE_VALID_V(this, TypedArray<RMLElement>());
	int count = element->GetNumChildren();
	TypedArray<RMLElement> ret;
	ret.resize(count);

	for (int i = 0; i < count; i++) {
		ret[i] = RMLElement::ref(element->GetChild(i));
	}

	return ret;
}

void RMLElement::clear_children() {
	ENSURE_VALID(this);
	int count = element->GetNumChildren();
	for (int i = count - 1; i >= 0; i--) {
		element->RemoveChild(element->GetChild(i));
	}
}

Rect2 RMLElement::get_rect() const {
	ENSURE_VALID_V(this, Rect2());
	Vector2 r_min = Vector2(INFINITY, INFINITY);
	Vector2 r_max = Vector2(-INFINITY, -INFINITY);

	int box_count = element->GetNumBoxes();
	for (int i = 0; i < box_count; i++) {
		Rml::Vector2f off;
		const Rml::Box &b = element->GetBox(i, off);
		Rml::Vector2f pos = b.GetPosition();
		Rml::Vector2f s = b.GetSize();
		r_min = r_min.min(Vector2(pos.x, pos.y));
		r_max = r_max.max(Vector2(pos.x + s.x, pos.y + s.y));
	}

	return Rect2(r_min, r_max-r_min);
}

void RMLElement::set_attribute(const String &p_name, const Variant &p_val) {
	ENSURE_VALID(this);
	element->SetAttribute(
		godot_to_rml_string(p_name), 
		godot_to_rml_variant(p_val)
	);
}

Variant RMLElement::get_attribute(const String &p_name, const Variant &p_default) const {
	ENSURE_VALID_V(this, Variant());
	return rml_to_godot_variant(element->GetAttribute(
		godot_to_rml_string(p_name),
		godot_to_rml_variant(p_default)
	));
}

void RMLElement::remove_attribute(const String &p_name) {
	ENSURE_VALID(this);
	element->RemoveAttribute(godot_to_rml_string(p_name));
}

void RMLElement::set_property(const String &p_name, const String &p_val) {
	ENSURE_VALID(this);
	element->SetProperty(
		godot_to_rml_string(p_name), 
		godot_to_rml_string(p_val)
	);
}

String RMLElement::get_property(const String &p_name, const String &p_default) const {
	ENSURE_VALID_V(this, p_default);
	const Rml::Property *prop = element->GetProperty(godot_to_rml_string(p_name));
	if (prop == nullptr) {
		return p_default;
	}
	return rml_to_godot_string(prop->ToString());
}

void RMLElement::remove_property(const String &p_name) {
	ENSURE_VALID(this);
	element->RemoveProperty(godot_to_rml_string(p_name));
}

void RMLElement::add_event_listener(const String &p_event_id, const Callable &p_listener) {
	ENSURE_VALID(this);
	RMLEventListener *listener = RMLEventListener::get_listener(element.get(), p_event_id, p_listener);
	ERR_FAIL_COND_MSG(listener != nullptr, "Already listening to this event");
	listener = RMLEventListener::create_listener(element.get(), p_event_id, p_listener);
}

void RMLElement::remove_event_listener(const String &p_event_id, const Callable &p_listener) {
	ENSURE_VALID(this);
	RMLEventListener *listener = RMLEventListener::get_listener(element.get(), p_event_id, p_listener);
	ERR_FAIL_COND_MSG(listener == nullptr, "Not listening to this event");

	element->RemoveEventListener(godot_to_rml_string(listener->get_event_id()), listener);
}

void RMLElement::set_id(const String &p_id) {
	ENSURE_VALID(this);
	element->SetId(godot_to_rml_string(p_id));
}

String RMLElement::get_id() const {
	ENSURE_VALID_V(this, "");
	return rml_to_godot_string(element->GetId());
}

void RMLElement::toggle_class(const String &p_class) {
	ENSURE_VALID(this);
	element->SetClass(godot_to_rml_string(p_class), !element->IsClassSet(godot_to_rml_string(p_class)));
}

void RMLElement::set_class(const String &p_class) {
	ENSURE_VALID(this);
	element->SetClass(godot_to_rml_string(p_class), true);
}

void RMLElement::set_class_names(const String &p_class_names) {
	ENSURE_VALID(this);
	element->SetClassNames(godot_to_rml_string(p_class_names));
}

void RMLElement::remove_class(const String &p_class) {
	ENSURE_VALID(this);
	element->SetClass(godot_to_rml_string(p_class), false);
}

String RMLElement::get_class_names() const {
	ENSURE_VALID_V(this, "");
	return rml_to_godot_string(element->GetClassNames());
}

bool RMLElement::has_class(const String &p_class) const {
	ENSURE_VALID_V(this, false);
	return element->IsClassSet(godot_to_rml_string(p_class));
}

String RMLElement::get_tag_name() const {
	ENSURE_VALID_V(this, String());
	return rml_to_godot_string(element->GetTagName());
}

void RMLElement::set_text_content(const String &p_text) {
	ENSURE_VALID(this);
	clear_children();

	element->AppendChild(
		element->GetOwnerDocument()->CreateTextNode(godot_to_rml_string(p_text))
	);
}

void RMLElement::set_inner_rml(const String &p_rml) {
	ENSURE_VALID(this);

	element->SetInnerRML(godot_to_rml_string(p_rml));
}

PackedStringArray RMLElement::get_text_content() const {
	ENSURE_VALID_V(this, PackedStringArray());
	Rml::ElementList el_list;
	element->GetElementsByTagName(el_list, "#text");
	PackedStringArray texts;
	for (auto it : el_list) {
		Rml::ElementText *text = (Rml::ElementText *)it;
		texts.append(rml_to_godot_string(text->GetText()));
	}
	return texts;
}

String RMLElement::get_inner_rml() const {
	ENSURE_VALID_V(this, "");
	return rml_to_godot_string(element->GetInnerRML());
}

Rml::Element *RMLElement::get_element() const {
	return element.get();
}

Ref<RMLElement> RMLElement::ref(ElementRef &ref) {
	Ref<RMLElement> ret;
	ret.instantiate();
	ret->element = ref;
	return ret;
}

Ref<RMLElement> RMLElement::ref(Rml::ElementPtr &&el) {
	Ref<RMLElement> ret;
	ret.instantiate();
	ret->element = ElementRef(std::move(el));
	return ret;
}

Ref<RMLElement> RMLElement::ref(Rml::Element *el) {
	Ref<RMLElement> ret;
	ret.instantiate();
	ret->element = ElementRef(el);
	return ret;
}

Ref<RMLElement> RMLElement::empty() {
	Ref<RMLElement> ret;
	ret.instantiate();
	return ret;
}

void RMLElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &RMLElement::is_valid);
	ClassDB::bind_method(D_METHOD("append_child", "child"), &RMLElement::append_child);
	ClassDB::bind_method(D_METHOD("remove_child", "child"), &RMLElement::remove_child);

	ClassDB::bind_method(D_METHOD("query_selector", "selector"), &RMLElement::query_selector);
	ClassDB::bind_method(D_METHOD("query_selector_all", "selector"), &RMLElement::query_selector_all);
	ClassDB::bind_method(D_METHOD("get_child_count"), &RMLElement::get_child_count);
	ClassDB::bind_method(D_METHOD("get_child", "index"), &RMLElement::get_child);
	ClassDB::bind_method(D_METHOD("get_parent"), &RMLElement::get_parent);
	ClassDB::bind_method(D_METHOD("get_children"), &RMLElement::get_children);
	ClassDB::bind_method(D_METHOD("clear_children"), &RMLElement::clear_children);

	ClassDB::bind_method(D_METHOD("get_rect"), &RMLElement::get_rect);

	ClassDB::bind_method(D_METHOD("set_attribute", "name", "value"), &RMLElement::set_attribute);
	ClassDB::bind_method(D_METHOD("get_attribute", "name", "default_value"), &RMLElement::get_attribute, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("remove_attribute", "name"), &RMLElement::remove_attribute);

	ClassDB::bind_method(D_METHOD("add_event_listener", "name", "listener"), &RMLElement::add_event_listener);
	ClassDB::bind_method(D_METHOD("remove_event_listener", "name", "listener"), &RMLElement::remove_event_listener);

	ClassDB::bind_method(D_METHOD("set_id", "id"), &RMLElement::set_id);
	ClassDB::bind_method(D_METHOD("get_id"), &RMLElement::get_id);

	ClassDB::bind_method(D_METHOD("set_class", "name"), &RMLElement::set_class);
	ClassDB::bind_method(D_METHOD("toggle_class", "name"), &RMLElement::toggle_class);
	ClassDB::bind_method(D_METHOD("set_class_names", "names"), &RMLElement::set_class_names);
	ClassDB::bind_method(D_METHOD("remove_class", "name"), &RMLElement::remove_class);
	ClassDB::bind_method(D_METHOD("get_class_names"), &RMLElement::get_class_names);
	ClassDB::bind_method(D_METHOD("has_class", "name"), &RMLElement::has_class);

	ClassDB::bind_method(D_METHOD("set_property", "name", "value"), &RMLElement::set_property);
	ClassDB::bind_method(D_METHOD("get_property", "name", "default_value"), &RMLElement::get_property, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("remove_property", "name"), &RMLElement::remove_property);

	ClassDB::bind_method(D_METHOD("get_tag_name"), &RMLElement::get_tag_name);

	ClassDB::bind_method(D_METHOD("set_text_content", "text"), &RMLElement::set_text_content);
	ClassDB::bind_method(D_METHOD("set_inner_rml", "rml"), &RMLElement::set_inner_rml);

	ClassDB::bind_method(D_METHOD("get_text_content"), &RMLElement::get_text_content);
	ClassDB::bind_method(D_METHOD("get_inner_rml"), &RMLElement::get_inner_rml);
}

