#pragma once
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <RmlUi/Core/RenderInterface.h>

namespace godot {

class RMLServer;

#ifdef DEBUG_ENABLED
#define PUSH_DEBUG_COMMAND(cmd_str) push_debug_command(cmd_str);
#else
#define PUSH_DEBUG_COMMAND
#endif

class RenderInterfaceGodot: public Rml::RenderInterface {
private:
#ifdef DEBUG_ENABLED
    PackedStringArray debug_commands;
#endif

protected:
    void clear_debug_commands();
    void push_debug_command(const String &p_command);
    void flush_debug_commands();

public:
    virtual void initialize() = 0;
    virtual void finalize() = 0;

    virtual void push_context(void *&p_ctx, const Vector2i &p_size) = 0;
    virtual void pop_context() = 0;
    virtual void draw_context(void *&p_ctx, const RID &p_canvas_item) = 0;
    virtual void free_context(void *&p_ctx) = 0;
};

}