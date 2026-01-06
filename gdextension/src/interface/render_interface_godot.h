#pragma once
#include <RmlUi/Core/RenderInterface.h>
#include <godot_cpp/classes/rendering_server.hpp>

namespace godot {

class RenderInterfaceGodot: public Rml::RenderInterface {
private:
    class GeometryData {
    public:
        PackedInt32Array indices;
        PackedVector2Array points;
        PackedColorArray colors;
        PackedVector2Array uvs;
    };

    class TextureData {
    public:
        bool is_generated;
        RID rid;
    };

    struct ContextRenderState {
    public:
        RID canvas_item;
    };

    static RenderInterfaceGodot *singleton;
    RID canvas_item;
public:
    static RenderInterfaceGodot *get_singleton();
    void set_drawing_canvas_item(const RID &p_ci);

	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
	void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
	void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;

    RenderInterfaceGodot();
    ~RenderInterfaceGodot();
};

}