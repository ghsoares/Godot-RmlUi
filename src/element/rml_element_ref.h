#pragma once

#include <godot_cpp/core/error_macros.hpp>
#include <RmlUi/Core.h>

namespace godot {

// Owns a unique pointer or a raw pointer to a Rml::Element
// Must always point to a valid element
class ElementOwner {
private:
	static std::map<Rml::Element *, std::weak_ptr<ElementOwner>> owners;

	Rml::ElementPtr unique_ptr = nullptr;
	Rml::Element *ptr = nullptr;

public:
	Rml::Element *get() const;
	bool is_unique() const;
	
	Rml::ElementPtr pop_owned();

	void push_owned(Rml::ElementPtr &&p_ptr);
	void push_raw(Rml::Element *p_ptr);

	static std::shared_ptr<ElementOwner> map(Rml::ElementPtr &&p_ptr);
	static std::shared_ptr<ElementOwner> map(Rml::Element *p_ptr);

	ElementOwner(Rml::ElementPtr &&ptr);
	ElementOwner(Rml::Element *ptr);
	~ElementOwner();
};

class ElementRef {
private:
	std::shared_ptr<ElementOwner> owner = nullptr;

public:
	Rml::Element *get() const;
	bool is_valid() const;

	Rml::ElementPtr pop_owned();
	void push_owner(Rml::ElementPtr &&p_ptr);

	bool operator==(const Rml::Element *p_ptr) const;
	bool operator==(const ElementRef &p_ref) const;

	Rml::Element *operator*() const;
	Rml::Element *operator->() const;

	ElementRef();
	ElementRef(ElementRef &ref);
	ElementRef(Rml::Element *el);
	ElementRef(Rml::ElementPtr &&el);
};

};