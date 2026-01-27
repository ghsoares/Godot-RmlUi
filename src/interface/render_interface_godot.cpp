#include "render_interface_godot.h"
#include <RmlUi/Core/Log.h>

using namespace godot;


#ifdef DEBUG_ENABLED
void RenderInterfaceGodot::clear_debug_commands() {
	debug_commands.clear();
}
void RenderInterfaceGodot::push_debug_command(const String &p_command) {
	debug_commands.append(p_command);
}
void RenderInterfaceGodot::flush_debug_commands() {
	String msg = vformat("Commands:\n- %s", String("\n- ").join(debug_commands));
	Rml::Log::Message(Rml::Log::LT_INFO, msg.utf8().get_data());
}
#else
void RenderInterfaceGodot::clear_debug_commands() {}
void RenderInterfaceGodot::push_debug_command(const String &p_command) {}
void RenderInterfaceGodot::flush_debug_commands() {}
#endif