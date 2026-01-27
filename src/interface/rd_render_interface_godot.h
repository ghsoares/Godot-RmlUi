#pragma once
#include <RmlUi/Core/RenderInterface.h>
#include <godot_cpp/classes/rendering_server.hpp>
#include <vector>

#include "render_interface_godot.h"
#include "../rendering/rendering_resources.h"

namespace godot {

class RDRenderInterfaceGodot: public RenderInterfaceGodot {
	struct MeshData {
        RID position_buffer;
        RID color_buffer;
        RID uv_buffer;
        RID index_buffer;
        RID vertex_array;
        RID index_array;
    };

    struct TextureData {
        RID rid;
        Ref<Texture> tex_ref;
        bool linear_filtering = true;
    };

	struct RenderTarget {
        RID color;
        RID framebuffer;
		Vector2i size;
    };

	struct Context {
		std::vector<RenderTarget *> target_stack;
		uintptr_t target_stack_ptr = 0;

		RenderTarget main_target;
		RenderTarget back_buffer0;
        RenderTarget back_buffer1;
        RenderTarget back_buffer2;
        RenderTarget blend_target;
		
		RID main_tex;
		RID clip_mask, clip_mask_framebuffer;

		Vector2i size;

		bool is_valid() { return main_tex.is_valid(); }

        RID get_texture() { return main_tex; }
	};

	struct RenderPass {
        String debug_name = "Pass_";

		RID shader;
		RID pipeline;

		MeshData *mesh_data = nullptr;

		PackedByteArray push_const;

		std::vector<std::pair<RID, bool>> uniform_textures;
		RID uniform_buffer = RID();

		RID framebuffer;

		BitField<RenderingDevice::DrawFlags> draw_flags = RenderingDevice::DRAW_DEFAULT_ALL;
		PackedColorArray clear_colors = {};
		unsigned int clear_stencil = 0;
        Rect2i region = Rect2i(0, 0, 0, 0);
	};

    struct RenderPasses {
        std::vector<RenderPass> passes;
    };

    struct ShaderInfo {
        RID shader;
        RID uniform_buffer = RID();
        uint64_t pipeline_id;
        PackedByteArray push_const;
    };

	Context *context = nullptr;

	RenderingResources internal_rendering_resources;
    RenderingResources rendering_resources;

	int64_t color_framebuffer_format;
    int64_t geometry_framebuffer_format;
    int64_t clip_mask_framebuffer_format;
    int64_t geometry_vertex_format;

    RID shader_blit;
    RID shader_geometry;
    RID shader_clip_mask;
    RID shader_layer_composition;
    RID shader_post_process;

    RID pipeline_blit;
    RID pipeline_geometry;
    RID pipeline_geometry_clipping;
    RID pipeline_clip_mask_set;
    RID pipeline_clip_mask_set_inverse;
    RID pipeline_clip_mask_intersect;
    RID pipeline_layer_composition;
    RID pipeline_post_process;

    std::map<uint64_t, RID> shaders;
    std::map<uint64_t, RID> pipelines;
    std::map<uint64_t, std::tuple<RID, RID>> pipelines_with_clip;

    RID sampler_nearest;
    RID sampler_linear;

    RID texture_white, texture_transparent;

    bool scissor_enabled = false;
    Rect2 scissor_region = Rect2();
    bool clip_mask_enabled = false;

    Rml::Matrix4f drawing_matrix = Rml::Matrix4f::Identity();

	void create_render_pipeline_with_clip(uint64_t p_id, const std::map<String, Variant> &p_params);
    RID get_shader_pipeline(uint64_t p_id) const;

	RenderTarget *get_render_target();
	
	void allocate_render_target(RenderTarget *p_target, const Vector2i &p_size);
    void free_render_target(RenderTarget *p_target);

	void allocate_context(Context *p_context, const Vector2i &p_size);
	void free_context(Context *p_context);

	void render_pass(const RenderPass &p_pass);

    RenderPass blit_pass(const RID &p_tex, const RID &p_framebuffer, const Vector2i &p_dst_pos = Vector2i(), const Vector2i &p_src_pos = Vector2i(), const Vector2i &p_size = Vector2i());

    bool check_if_can_render_with_scissor() const;
public:
	void initialize() override;
    void finalize() override;

    void push_context(void *&p_ctx, const Vector2i &p_size) override;
    void pop_context() override;
    void draw_context(void *&p_ctx, const RID &p_canvas_item) override;
	void free_context(void *&p_ctx) override;

	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
	void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
	void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;

    void EnableClipMask(bool enable) override;
    void RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) override;

    void SetTransform(const Rml::Matrix4f* transform) override;

    Rml::LayerHandle PushLayer() override;
    void CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination, Rml::BlendMode blend_mode, Rml::Span<const Rml::CompiledFilterHandle> filters) override;
    void PopLayer() override;

    Rml::TextureHandle SaveLayerAsTexture() override;
    Rml::CompiledFilterHandle SaveLayerAsMaskImage() override;

    Rml::CompiledFilterHandle CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) override;
    void ReleaseFilter(Rml::CompiledFilterHandle filter) override;

    Rml::CompiledShaderHandle CompileShader(const Rml::String& name, const Rml::Dictionary& parameters) override;
    void RenderShader(Rml::CompiledShaderHandle shader, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    void ReleaseShader(Rml::CompiledShaderHandle shader) override;
};

}