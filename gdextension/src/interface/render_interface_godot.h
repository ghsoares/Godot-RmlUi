#pragma once
#include <RmlUi/Core/RenderInterface.h>
#include <godot_cpp/classes/rendering_server.hpp>
#include <deque>

#include "../rendering/rendering_utils.h"

namespace godot {

class RMLServer;

class RenderInterfaceGodot: public Rml::RenderInterface {

    friend class RMLServer;
private:
    static RenderInterfaceGodot *singleton;

    RenderFrame *target_frame;
    std::deque<RenderTarget *> render_target_stack;
    RenderingResources internal_rendering_resources;
    RenderingResources rendering_resources;

    int64_t geometry_framebuffer_format;
    int64_t clip_mask_framebuffer_format;
    int64_t geometry_vertex_format;

    RID shader_geometry;
    RID shader_clip_mask;

    RID pipeline_geometry;
    RID pipeline_geometry_clipping;
    RID pipeline_clip_mask_set;
    RID pipeline_clip_mask_set_inverse;
    RID pipeline_clip_mask_intersect;

    RID sampler_nearest;
    RID sampler_linear;

    RID texture_white;

    bool scissor_enabled = false;
    Rect2 scissor_region = Rect2();
    bool clip_mask_enabled = false;

    Projection drawing_matrix;

    void allocate_render_target(RenderTarget *p_target);
    void free_render_target(RenderTarget *p_target);

    void allocate_render_frame();
public:
    static RenderInterfaceGodot *get_singleton();

    void initialize();
    void uninitialize();
    
    void set_render_frame(RenderFrame *p_frame);
    void free_render_frame(RenderFrame *p_frame);

	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
	void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
	void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;

    void EnableClipMask(bool enable);
    void RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation);

    void SetTransform(const Rml::Matrix4f* transform) override;

    RenderInterfaceGodot();
    ~RenderInterfaceGodot();
};

}