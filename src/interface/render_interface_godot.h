#pragma once
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <RmlUi/Core/RenderInterface.h>

namespace godot {

class RMLServer;

class RenderInterfaceGodot: public Rml::RenderInterface {
public:
    virtual void initialize() = 0;
    virtual void finalize() = 0;

    virtual void push_context(void *&p_ctx, const Vector2i &p_size) = 0;
    virtual void pop_context() = 0;
    virtual void draw_context(void *&p_ctx, const RID &p_canvas_item, const Transform2D &p_base_transform) = 0;
    virtual void free_context(void *&p_ctx) = 0;
};

}