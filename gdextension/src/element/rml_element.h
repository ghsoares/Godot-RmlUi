#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <RmlUi/Core.h>

namespace godot {

class RMLServer;
class RMLDocument;

using element_ptr = Rml::ElementPtr;

struct ElementRef {
	element_ptr owned;
	Rml::Element *attached = nullptr;

	Rml::Element *get() const {
		return attached ? attached : owned.get();
	}

	bool is_attached() const {
		return attached != nullptr;
	}

	bool is_valid() const {
		return get() != nullptr;
	}

	element_ptr pop_owned() {
		element_ptr ptr = std::move(owned);
		owned = nullptr;
		return ptr;
	}

	Rml::Element *pop_attached() {
		Rml::Element *ptr = attached;
		attached = nullptr;
		return ptr;
	}

	_FORCE_INLINE_ void operator=(ElementRef &p_ref) {
		owned = p_ref.pop_owned();
		attached = p_ref.attached;
	}

	_FORCE_INLINE_ bool operator==(const Rml::Element *p_ptr) const {
		return get() == p_ptr;
	}

	_FORCE_INLINE_ Rml::Element *operator*() const {
		return get();
	}

	_FORCE_INLINE_ Rml::Element *operator->() const {
		return get();
	}

	_FORCE_INLINE_ Rml::Element *ptr() const {
		return get();
	}

	_FORCE_INLINE_ ElementRef() {}
	_FORCE_INLINE_ ElementRef(ElementRef &ref) {
		owned = ref.pop_owned();
		attached = ref.attached;
	}
	_FORCE_INLINE_ ElementRef(Rml::Element *el): attached(el) {}
	_FORCE_INLINE_ ElementRef(Rml::ElementPtr el): owned(std::move(el)) {}
};

class RMLElement: public RefCounted {
	GDCLASS(RMLElement, RefCounted);

	friend class RMLServer;
	friend class RMLDocument;

	ElementRef element = nullptr;

protected:
	static void _bind_methods();

public:
	void append_child(const Ref<RMLElement> &p_child);
	void remove_child(const Ref<RMLElement> &p_child);
	Ref<RMLElement> query_selector(const String &p_selector) const;
	TypedArray<Ref<RMLElement>> query_selector_all(const String &p_selector) const;
	int get_child_count() const;
	Ref<RMLElement> get_parent() const;
	Ref<RMLElement> get_child(int p_idx) const;
	TypedArray<Ref<RMLElement>> get_children() const;
	void clear_children();

	Rect2 get_rect() const;

	void set_attribute(const String &p_name, const Variant &p_val);
	Variant get_attribute(const String &p_name, const Variant &p_default = Variant()) const;
	void remove_attribute(const String &p_name);

	void set_property(const String &p_name, const String &p_val);
	String get_property(const String &p_name, const String &p_default = String()) const;
	void remove_property(const String &p_name);

	void add_event_listener(const String &p_event_id, const Callable &p_listener);
	void remove_event_listener(const String &p_event_id, const Callable &p_listener);

	void set_id(const String &p_id);
	String get_id() const;

	void set_class(const String &p_class);
	void toggle_class(const String &p_class);
	void set_class_names(const String &p_class_names);
	void remove_class(const String &p_class);
	String get_class_names() const;
	bool has_class(const String &p_class) const;

	String get_tag_name() const;

	void set_text_content(const String &p_text);
	void set_inner_rml(const String &p_rml);

	PackedStringArray get_text_content() const;
	String get_inner_rml() const;

	Rml::Element *get_element() const;

	static Ref<RMLElement> ref(ElementRef &ref);
	static Ref<RMLElement> ref(ElementRef &&ref);
	static Ref<RMLElement> empty();

	RMLElement(): element(nullptr) {}
	RMLElement(ElementRef el): element(el) {}
};

}
