#include "register_types.h"

#include <iostream>
#include <sstream>

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/godot.hpp>

#include <RmlUi/Core.h>
#include "interface/system_interface_godot.h"
#include "interface/render_interface_godot.h"
#include "element/rml_document.h"

using namespace godot;

void initialize_rmlui() {
    static SystemInterfaceGodot system;
    static RenderInterfaceGodot render;
	
	Rml::SetSystemInterface(&system);
	Rml::SetRenderInterface(&render);
	Rml::Initialise();
}

void uninitialize_rmlui() {
	Rml::Shutdown();
}

void initialize_gdex_module(ModuleInitializationLevel p_level) {
	switch (p_level) {
		case MODULE_INITIALIZATION_LEVEL_CORE: {
			initialize_rmlui();
		} break;
		case MODULE_INITIALIZATION_LEVEL_SCENE: {
			GDREGISTER_CLASS(RMLDocument);
		} break;
		default: break;
	}
}

void uninitialize_gdex_module(ModuleInitializationLevel p_level) {
	switch (p_level) {
		case MODULE_INITIALIZATION_LEVEL_CORE: {
			uninitialize_rmlui();
		} break;
		default: break;
	}
}

extern "C" {
// Initialization.
GDExtensionBool GDE_EXPORT gdex_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_gdex_module);
	init_obj.register_terminator(uninitialize_gdex_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}