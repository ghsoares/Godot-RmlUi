#include "register_types.h"

#include "project_settings.h"

#include "interface/system_interface_godot.h"
#include "interface/rd_render_interface_godot.h"
#include "interface/file_interface_godot.h"
#include "element/rml_document.h"
#include "element/rml_element.h"
#include "server/rml_server.h"
#include "plugin/rml_godot_plugin.h"

#include <iostream>
#include <sstream>

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/godot.hpp>

#include <RmlUi/Core.h>

using namespace godot;

static RMLServer *rml_server = nullptr;

void initialize_rmlui() {
    static SystemInterfaceGodot system;
    static RDRenderInterfaceGodot render;
    static FileInterfaceGodot file;
	static RmlPluginGodot plugin;
	
	Rml::SetSystemInterface(&system);
	Rml::SetRenderInterface(&render);
	Rml::SetFileInterface(&file);
	Rml::RegisterPlugin(&plugin);
	Rml::Initialise();
}

void uninitialize_rmlui() {
	Rml::Shutdown();
}

void initialize_gdex_module(ModuleInitializationLevel p_level) {
	switch (p_level) {
		case MODULE_INITIALIZATION_LEVEL_CORE: {
			GLOBAL_DEF_RST("RmlUi/load_user_agent_stylesheet", true);
			GLOBAL_DEF_RST("RmlUi/custom_user_agent_stylesheet", String());

			initialize_rmlui();
		} break;
		case MODULE_INITIALIZATION_LEVEL_SERVERS: {
			GDREGISTER_CLASS(RMLServer);

			rml_server = memnew(RMLServer);
			rml_server->load_resources();

			Engine::get_singleton()->register_singleton("RMLServer", rml_server);
		} break;
		case MODULE_INITIALIZATION_LEVEL_SCENE: {
			GDREGISTER_CLASS(RMLDocument);
			GDREGISTER_CLASS(RMLElement);
		} break;
		default: break;
	}
}

void uninitialize_gdex_module(ModuleInitializationLevel p_level) {
	switch (p_level) {
		case MODULE_INITIALIZATION_LEVEL_SERVERS: {
			uninitialize_rmlui();
			memdelete(rml_server);
		} break;
		default: break;
	}
}

void startup_gdex_module() {
	RMLServer::get_singleton()->initialize();
}

void shutdown_gdex_module() {
	RMLServer::get_singleton()->uninitialize();
}

extern "C" {
// Initialization.
GDExtensionBool GDE_EXPORT gdex_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_gdex_module);
	init_obj.register_terminator(uninitialize_gdex_module);
	init_obj.register_startup_callback(startup_gdex_module);
	init_obj.register_shutdown_callback(shutdown_gdex_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}