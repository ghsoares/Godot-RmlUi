#pragma once
#include <RmlUi/Core/RenderInterface.h>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/render_canvas_data_rd.hpp>
#include <godot_cpp/templates/rid_owner.hpp>
#include <vector>

#include "../rendering/rendering_resources.h"

namespace godot {

#define MAX_FILTERS_COUNT 16

class RDRenderInterfaceGodot: public Rml::RenderInterface {
private:
	struct MeshData {
        RID position_buffer;
        RID color_buffer;
        RID uv_buffer;
        RID index_buffer;
        RID vertex_array;
        RID index_array;
		Rect2 bounds;
    };

    struct TextureData {
        RID rid;
		RID rid_srgb;
        Ref<Texture> tex_ref;
        bool linear_filtering = true;
		Vector2i size;
    };

	struct FilterData {
		enum {
			TYPE_NONE = 0,
			TYPE_MODULATE,
			TYPE_COLOR_MATRIX,
			TYPE_BLUR,
			TYPE_DROP_SHADOW,
			TYPE_BLEND_MASK
		} type;

		union {
			struct {
				Color color;
				unsigned int space;
			} modulate;
			struct {
				float sigma;
			} blur;
			struct {
				float sigma;
				Vector2 offset;
				Color color;
			} drop_shadow;
			struct {
				Projection matrix;
			} color_matrix;
		};

		FilterData() {}
		~FilterData() {}
	};

	struct ShaderData {
		String name;

		Pipeline *pipeline;

		PackedByteArray push_const;
		RID uniform_buffer;
	};

	struct RenderTarget {
        RID color;
		std::vector<RID> mipmap_levels;
		Vector2i size;
		int64_t texture_format = -1;
		int64_t srgb_format = -1;
    };

	struct RenderPass {
        String debug_label = "Pass_";

		RID shader;
		RID pipeline;
		RID framebuffer;

		bool ignore_scissor = false;

		MeshData *mesh_data = nullptr;

		PackedByteArray push_const;

		TypedArray<RID> uniform_textures;
		TypedArray<RID> uniform_samplers;
		RID uniform_buffer = RID();

		BitField<RenderingDevice::DrawFlags> draw_flags = RenderingDevice::DRAW_DEFAULT_ALL;
		PackedColorArray clear_colors = {};
		unsigned int clear_stencil = 0;
        Rect2i region = Rect2i(0, 0, 0, 0);
	};

	struct DrawCommand {
		enum Type {
			TYPE_NONE = 0,
			TYPE_RENDER_GEOMETRY,
			TYPE_RENDER_SHADER,
			TYPE_RENDER_CLIP_MASK,
			TYPE_COMPOSITE_LAYERS,
			TYPE_RENDER_LAYER_TO_TEXTURE,
			TYPE_RENDER_LAYER_TO_BLEND_MASK,
			TYPE_SCISSOR,
			TYPE_CLIP_MASK,
			TYPE_TRANSFORM,
			TYPE_PUSH_LAYER,
			TYPE_POP_LAYER
		} type = TYPE_NONE;

		union {
			struct {
				uintptr_t geometry_id;
				uintptr_t texture_id;
				Vector2 translation;
			} render_geometry;
			struct {
				uintptr_t shader_id;
				uintptr_t geometry_id;
				uintptr_t texture_id;
				Vector2 translation;
			} render_shader;
			struct {
				unsigned int clip_operation; 
				uintptr_t geometry_id;
				Vector2 translation;
			} render_clip_mask;
			struct {
				uintptr_t source_layer_id, destination_layer_id;
				unsigned int blend_mode;
				uintptr_t filter_count;
				uintptr_t filter_ids[MAX_FILTERS_COUNT];
			} composite_layers;
			struct {
				uintptr_t texture_id;
			} render_layer_to_texture;
			struct {
				bool enable;
				Rect2i region;
			} scissor;
			struct {
				bool enable;
			} clip_mask;
			struct {
				Projection matrix;
			} transform;
		};

		DrawCommand() {}
		~DrawCommand() {}
	};

	struct Context {
		RID canvas_item;

		std::vector<RenderTarget *> target_stack;
		uintptr_t target_stack_ptr = 0;

		std::vector<DrawCommand> draw_commands;

		RID clip_mask;

		// This render target is populated on draw for the screen texture and framebuffer
		RenderTarget main_target;

		RenderTarget back_target0;
		RenderTarget back_target1;
		RenderTarget filter_target;
		RenderTarget blend_mask_target;

		Vector2i size;
		int64_t texture_format = -1;
	};

	Context *context = nullptr;

	RenderingResources internal_rendering_resources;
    RenderingResources rendering_resources;
	
	Pipeline *pipeline_geometry;
	Pipeline *pipeline_layer_composition;
	Pipeline *pipeline_clip_mask_set;
	Pipeline *pipeline_clip_mask_set_inverse;
	Pipeline *pipeline_clip_mask_intersect;
	Pipeline *pipeline_downsample;

	Pipeline *pipeline_filter_modulate;
	Pipeline *pipeline_filter_color_matrix;
	Pipeline *pipeline_filter_blur;
	Pipeline *pipeline_filter_drop_shadow;
	Pipeline *pipeline_filter_blend_mask;

	Pipeline *pipeline_shader_gradient;

	RID sampler_nearest_no_repeat;
    RID sampler_linear_no_repeat;
    RID sampler_linear_repeat;

    RID texture_white, texture_transparent;

    int64_t geometry_vertex_format;

	std::vector<MeshData *> meshes_to_release;
	std::vector<TextureData *> textures_to_release;
	std::vector<FilterData *> filters_to_release;
	std::vector<ShaderData *> shaders_to_release;

	RID_Owner<Pipeline> pipelines;

	bool scissor_enabled = false;
    Rect2i scissor_region = Rect2i();
    bool clip_mask_enabled = false;

    Projection drawing_matrix = Projection();

	DrawCommand *push_draw_command();

	void allocate_render_target(RenderTarget *p_target, const Vector2i &p_size, int64_t p_texture_format);
    void free_render_target(RenderTarget *p_target);

	void allocate_context(Context *p_context, const Vector2i &p_size, int64_t p_texture_format);
	void free_context(Context *p_context);

	RenderTarget *context_get_current_render_target(Context *p_context);

	bool empty_scissor();
	bool bounds_culled(const Rect2 &p_bounds, const Projection &p_projection, const Vector2i &p_screen_size);

	void render_pass(const RenderPass &p_pass);

	void blit(
		const RID &p_src_tex, 
		const RID &p_dst_tex, 
		const Vector2i &p_size,
		const Vector2i &p_src_pos = Vector2i(), const Vector2i &p_dst_pos = Vector2i(), 
		int p_src_mip = 0, int p_dst_mip = 0
	);
	void filter_pass(Context *p_context, FilterData *p_filter);
	void generate_mipmaps(RenderTarget *p_target, int p_mipmaps = -1);

	RID get_framebuffer(const TypedArray<RID> &p_textures, int64_t &p_framebuffer_format);
public:
	void initialize();
    void finalize();

    void push_context(void *&p_ctx, const Vector2i &p_size);
    void pop_context();
    void draw_context(void *&p_ctx, const RID &p_canvas_item, const Vector2i &p_base_offset, RenderCanvasDataRD *p_render_data);
	void free_context(void *&p_ctx);
	
	void free_pending_resources();

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