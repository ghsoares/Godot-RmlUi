#include "rml_element_ref.h"

using namespace godot;

std::map<Rml::Element *, std::weak_ptr<ElementOwner>> ElementOwner::owners = {};

Rml::Element *ElementOwner::get() const {
	return ptr;
}

bool ElementOwner::is_unique() const {
	return unique_ptr != nullptr;
}

Rml::ElementPtr ElementOwner::pop_owned() {
	ERR_FAIL_NULL_V(unique_ptr, nullptr);
	Rml::ElementPtr ret = std::move(unique_ptr);
	unique_ptr = nullptr;
	return ret;
}

void ElementOwner::push_owned(Rml::ElementPtr &&p_ptr) {
	unique_ptr = std::move(p_ptr);
	ptr = unique_ptr.get();
}

void ElementOwner::push_raw(Rml::Element *p_ptr) {
	unique_ptr = nullptr;
	ptr = p_ptr;
}

ElementOwner::ElementOwner(Rml::ElementPtr &&ptr) {
	push_owned(std::move(ptr));
}

ElementOwner::ElementOwner(Rml::Element *ptr) {
	push_raw(ptr);
}

ElementOwner::~ElementOwner() { }

std::shared_ptr<ElementOwner> ElementOwner::map(Rml::ElementPtr &&p_ptr) {
	std::shared_ptr<ElementOwner> owner = map(p_ptr.get());
	if (owner == nullptr) return nullptr;
	owner->push_owned(std::move(p_ptr));
	return owner;
}

std::shared_ptr<ElementOwner> ElementOwner::map(Rml::Element *p_ptr) {
	if (p_ptr == nullptr) return nullptr;
	auto it = owners.find(p_ptr);
	if (it != owners.end()) {
		std::shared_ptr<ElementOwner> owner = it->second.lock();
		if (owner) {
			return owner;
		}
	}
	std::shared_ptr<ElementOwner> owner = std::make_unique<ElementOwner>(p_ptr);
	owners.insert({ p_ptr, owner });
	return owner;
}

Rml::Element *ElementRef::get() const {
	return owner ? owner->get() : nullptr;
}

bool ElementRef::is_valid() const {
	return owner != nullptr;
}

Rml::ElementPtr ElementRef::pop_owned() {
	ERR_FAIL_NULL_V(owner, nullptr);
	return owner->pop_owned();
}

void ElementRef::push_owner(Rml::ElementPtr &&p_ptr) {
	ERR_FAIL_NULL(owner);
	owner->push_owned(std::move(p_ptr));
}

bool ElementRef::operator==(const Rml::Element *p_ptr) const {
	return get() == p_ptr;
}

bool ElementRef::operator==(const ElementRef &p_ref) const {
	return get() == p_ref.get();
}

Rml::Element *ElementRef::operator*() const {
	return get();
}

Rml::Element *ElementRef::operator->() const {
	return get();
}

ElementRef::ElementRef() { }

ElementRef::ElementRef(ElementRef &ref) {
	owner = ref.owner;
}

ElementRef::ElementRef(Rml::Element *el) {
	owner = ElementOwner::map(el);
}

ElementRef::ElementRef(Rml::ElementPtr &&el) {
	owner = ElementOwner::map(std::move(el));
}


