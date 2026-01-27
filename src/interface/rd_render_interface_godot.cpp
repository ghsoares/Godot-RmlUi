#include "rd_render_interface_godot.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/uniform_set_cache_rd.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/rd_shader_file.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_sampler_state.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_attachment_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/classes/rd_vertex_attribute.hpp>
#include <godot_cpp/classes/rd_pipeline_rasterization_state.hpp>
#include <godot_cpp/classes/rd_pipeline_multisample_state.hpp>
#include <godot_cpp/classes/rd_pipeline_depth_stencil_state.hpp>
#include <godot_cpp/classes/rd_pipeline_color_blend_state.hpp>
#include <godot_cpp/classes/rd_pipeline_color_blend_state_attachment.hpp>

#include <RmlUi/Core/Dictionary.h>
#include <RmlUi/Core/DecorationTypes.h>

#include <iostream>

#include "../rml_util.h"
#include "../util.h"

using namespace godot;

using RD = RenderingDevice;

const uint64_t SHADER_FILTER_MODULATE = 1;
const uint64_t SHADER_FILTER_BLUR = 2;
const uint64_t SHADER_FILTER_DROP_SHADOW = 3;
const uint64_t SHADER_FILTER_COLOR_MATRIX = 4;
const uint64_t SHADER_FILTER_MASK = 5;

const uint64_t SHADER_GRADIENT = 16;

const uint64_t PIPELINE_GEOMETRY = 1;

const uint64_t PIPELINE_LAYER_COMPOSITION = 2;

const uint64_t PIPELINE_FILTER_MODULATE = 3;
const uint64_t PIPELINE_FILTER_BLUR = 4;
const uint64_t PIPELINE_FILTER_DROP_SHADOW = 5;
const uint64_t PIPELINE_FILTER_COLOR_MATRIX = 6;
const uint64_t PIPELINE_FILTER_MASK = 7;

const uint64_t PIPELINE_GRADIENT = 16;

Rml::Matrix4f get_final_transform(const Rml::Matrix4f &p_drawing_matrix, const Rml::Vector2f &translation) {
    return p_drawing_matrix * Rml::Matrix4f::Translate(Rml::Vector3f(translation.x, translation.y, 0.0));
}

void matrix_to_pointer(float *ptr, const Rml::Matrix4f &p_mat) {
    ptr[0] = p_mat[0].x;
    ptr[1] = p_mat[0].y;
    ptr[2] = p_mat[0].z;
    ptr[3] = p_mat[0].w;

    ptr[4] = p_mat[1].x;
    ptr[5] = p_mat[1].y;
    ptr[6] = p_mat[1].z;
    ptr[7] = p_mat[1].w;

    ptr[8] = p_mat[2].x;
    ptr[9] = p_mat[2].y;
    ptr[10] = p_mat[2].z;
    ptr[11] = p_mat[2].w;
    
    ptr[12] = p_mat[3].x;
    ptr[13] = p_mat[3].y;
    ptr[14] = p_mat[3].z;
    ptr[15] = p_mat[3].w;
}

void RDRenderInterfaceGodot::create_render_pipeline_with_clip(uint64_t p_id, const std::map<String, Variant> &p_params) {
    std::map clipping_params = std::map(p_params);
    clipping_params.merge(std::map<String, Variant>({
        {"enable_stencil", true},
        {"op_ref", 0x01},
        {"op_compare", RD::COMPARE_OP_EQUAL},
        {"op_compare_mask", 0xff},
        {"op_write_mask", 0x00},
        {"op_pass", RD::STENCIL_OP_KEEP},
        {"op_fail", RD::STENCIL_OP_KEEP}
    }));
    RID pipeline_not_clipping = internal_rendering_resources.create_render_pipeline(p_params);
    RID pipeline_clipping = internal_rendering_resources.create_render_pipeline(clipping_params);

    pipelines_with_clip[p_id] = {pipeline_not_clipping, pipeline_clipping};
}

RID RDRenderInterfaceGodot::get_shader_pipeline(uint64_t p_id) const {
    return clip_mask_enabled ? std::get<1>(pipelines_with_clip.at(p_id)) : std::get<0>(pipelines_with_clip.at(p_id));
}

void RDRenderInterfaceGodot::initialize() {
    RenderingServer *rs = RenderingServer::get_singleton();
    RD *rd = rs->get_rendering_device();

    internal_rendering_resources = RenderingResources(rd);
    rendering_resources = RenderingResources(rd);

    Ref<RDVertexAttribute> pos_attr = memnew(RDVertexAttribute);
    pos_attr->set_format(RD::DATA_FORMAT_R32G32_SFLOAT);
    pos_attr->set_location(0);
    pos_attr->set_stride(4 * 2); // 4 bytes * 2 members (xy)

    Ref<RDVertexAttribute> color_attr = memnew(RDVertexAttribute);
    color_attr->set_format(RD::DATA_FORMAT_R32G32B32A32_SFLOAT);
    color_attr->set_location(1);
    color_attr->set_stride(4 * 4); // 4 bytes * 4 members (rgba)

    Ref<RDVertexAttribute> uv_attr = memnew(RDVertexAttribute);
    uv_attr->set_format(RD::DATA_FORMAT_R32G32_SFLOAT);
    uv_attr->set_location(2);
    uv_attr->set_stride(4 * 2); // 4 bytes * 2 members (xy)

    Ref<RDAttachmentFormat> color_attachment = memnew(RDAttachmentFormat);
    Ref<RDAttachmentFormat> clip_mask_attachment = memnew(RDAttachmentFormat);

    uint64_t main_usage = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;

    color_attachment->set_format(RD::DATA_FORMAT_R8G8B8A8_UNORM);
    color_attachment->set_usage_flags(main_usage);

    clip_mask_attachment->set_format(RD::DATA_FORMAT_S8_UINT);
    clip_mask_attachment->set_usage_flags(RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT);

    color_framebuffer_format = rd->framebuffer_format_create({ color_attachment });
    geometry_framebuffer_format = rd->framebuffer_format_create({ color_attachment, clip_mask_attachment });
    clip_mask_framebuffer_format = rd->framebuffer_format_create({ clip_mask_attachment });
    geometry_vertex_format = rd->vertex_format_create({ pos_attr, color_attr, uv_attr });

    shader_blit = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/blit.glsl"},
        {"name", "rmlui_blit_shader"}
    });
    shader_geometry = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/geometry.glsl"},
        {"name", "rmlui_geometry_shader"}
    });
    shader_clip_mask = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/clip_mask.glsl"},
        {"name", "rmlui_clip_mask_shader"}
    });
    shader_layer_composition = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/layer_composition.glsl"},
        {"name", "rmlui_layer_composition_shader"}
    });
    shader_post_process = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/post_process.glsl"},
        {"name", "rmlui_post_process_shader"}
    });

    pipeline_blit = internal_rendering_resources.create_render_pipeline({
        {"shader", shader_blit},
        {"framebuffer_format", color_framebuffer_format},
        {"attachment_count", 1}
    });
    create_render_pipeline_with_clip(PIPELINE_GEOMETRY, {
        {"shader", shader_geometry},
        {"framebuffer_format", geometry_framebuffer_format},
        {"vertex_format", geometry_vertex_format},
        {"attachment_count", 2},
        {"attachment_enable_blend", true},
        {"attachment_color_blend_op", RD::BLEND_OP_ADD},
        {"attachment_alpha_blend_op", RD::BLEND_OP_ADD},
        {"attachment_src_color_blend_factor", RD::BLEND_FACTOR_ONE},
        {"attachment_dst_color_blend_factor", RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
        {"attachment_src_alpha_blend_factor", RD::BLEND_FACTOR_ONE},
        {"attachment_dst_alpha_blend_factor", RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
        {"print", true}
    });
    create_render_pipeline_with_clip(PIPELINE_LAYER_COMPOSITION, {
        {"shader", shader_layer_composition},
        {"framebuffer_format", geometry_framebuffer_format},
        {"attachment_count", 2},
    });

    pipeline_clip_mask_set = internal_rendering_resources.create_render_pipeline({
        {"shader", shader_clip_mask},
        {"framebuffer_format", clip_mask_framebuffer_format},
        {"vertex_format", geometry_vertex_format},
        {"attachment_count", 0},

        {"enable_stencil", true},
        {"op_ref", 0x01},
        {"op_write_mask", 0xff},
        {"op_compare", RD::COMPARE_OP_ALWAYS},
        {"op_compare_mask", 0xff},
        {"op_pass", RD::STENCIL_OP_REPLACE},
    });
    pipeline_clip_mask_set_inverse = internal_rendering_resources.create_render_pipeline({
        {"shader", shader_clip_mask},
        {"framebuffer_format", clip_mask_framebuffer_format},
        {"vertex_format", geometry_vertex_format},
        {"attachment_count", 0},

        {"enable_stencil", true},
        {"op_ref", 0x01},
        {"op_write_mask", 0xff},
        {"op_compare", RD::COMPARE_OP_ALWAYS},
        {"op_compare_mask", 0xff},
        {"op_pass", RD::STENCIL_OP_REPLACE},
    });
    pipeline_clip_mask_intersect = internal_rendering_resources.create_render_pipeline({
        {"shader", shader_clip_mask},
        {"framebuffer_format", clip_mask_framebuffer_format},
        {"vertex_format", geometry_vertex_format},
        {"attachment_count", 0},

        {"enable_stencil", true},
        {"op_ref", 0x01},
        {"op_write_mask", 0xff},
        {"op_compare", RD::COMPARE_OP_ALWAYS},
        {"op_compare_mask", 0xff},
        {"op_pass", RD::STENCIL_OP_REPLACE},
    });

    pipeline_post_process = internal_rendering_resources.create_render_pipeline({
        {"shader", shader_post_process},
        {"framebuffer_format", geometry_framebuffer_format},
        {"attachment_count", 2},
    });

    shaders[SHADER_FILTER_MODULATE] = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/filters/modulate.glsl"},
        {"name", "rmlui_filter_modulate_shader"}
    });
    shaders[SHADER_FILTER_BLUR] = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/filters/blur.glsl"},
        {"name", "rmlui_filter_blur_shader"}
    });
    shaders[SHADER_FILTER_DROP_SHADOW] = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/filters/drop_shadow.glsl"},
        {"name", "rmlui_filter_drop_shadow_shader"}
    });
    shaders[SHADER_FILTER_COLOR_MATRIX] = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/filters/color_matrix.glsl"},
        {"name", "rmlui_filter_color_matrix_shader"}
    });
    shaders[SHADER_FILTER_MASK] = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/filters/mask.glsl"},
        {"name", "rmlui_filter_mask_shader"}
    });

    shaders[SHADER_GRADIENT] = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/shaders/gradient.glsl"},
        {"name", "rmlui_filter_gradient_shader"}
    });

    pipelines[PIPELINE_FILTER_MODULATE] = internal_rendering_resources.create_render_pipeline({
        {"shader", shaders[SHADER_FILTER_MODULATE]},
        {"framebuffer_format", geometry_framebuffer_format},
        {"attachment_count", 2}
    });
    pipelines[PIPELINE_FILTER_BLUR] = internal_rendering_resources.create_render_pipeline({
        {"shader", shaders[SHADER_FILTER_BLUR]},
        {"framebuffer_format", geometry_framebuffer_format},
        {"attachment_count", 2}
    });
    pipelines[PIPELINE_FILTER_DROP_SHADOW] = internal_rendering_resources.create_render_pipeline({
        {"shader", shaders[SHADER_FILTER_DROP_SHADOW]},
        {"framebuffer_format", geometry_framebuffer_format},
        {"attachment_count", 2}
    });
    pipelines[PIPELINE_FILTER_COLOR_MATRIX] = internal_rendering_resources.create_render_pipeline({
        {"shader", shaders[SHADER_FILTER_COLOR_MATRIX]},
        {"framebuffer_format", geometry_framebuffer_format},
        {"attachment_count", 2}
    });
    pipelines[PIPELINE_FILTER_MASK] = internal_rendering_resources.create_render_pipeline({
        {"shader", shaders[SHADER_FILTER_MASK]},
        {"framebuffer_format", geometry_framebuffer_format},
        {"attachment_count", 2}
    });
    pipelines[PIPELINE_FILTER_MASK] = internal_rendering_resources.create_render_pipeline({
        {"shader", shaders[SHADER_FILTER_MASK]},
        {"framebuffer_format", geometry_framebuffer_format},
        {"attachment_count", 2}
    });

    create_render_pipeline_with_clip(PIPELINE_GRADIENT, {
        {"shader", shaders[SHADER_GRADIENT]},
        {"framebuffer_format", geometry_framebuffer_format},
        {"vertex_format", geometry_vertex_format},
        {"attachment_count", 2}
    });

    sampler_nearest = internal_rendering_resources.create_sampler({});
    sampler_linear = internal_rendering_resources.create_sampler({
        {"filter", RD::SAMPLER_FILTER_LINEAR},
        {"repeat_u", RD::SAMPLER_REPEAT_MODE_REPEAT},
        {"repeat_v", RD::SAMPLER_REPEAT_MODE_REPEAT},
        {"repeat_w", RD::SAMPLER_REPEAT_MODE_REPEAT},
    });

    PackedByteArray white_texture_buf, transparent_texture_buf;
    // width * height * rgba
    white_texture_buf.resize(4 * 4 * 4);
    transparent_texture_buf.resize(4 * 4 * 4);
    white_texture_buf.fill(255);
    transparent_texture_buf.fill(0);

    texture_white = internal_rendering_resources.create_texture({
        {"texture_type", RD::TEXTURE_TYPE_2D},
        {"width", 4},
        {"height", 4},
        {"format", RD::DATA_FORMAT_R8G8B8A8_UNORM},
        {"usage_bits", RD::TEXTURE_USAGE_SAMPLING_BIT},
        {"data", TypedArray<PackedByteArray>({white_texture_buf})}
    });
    texture_transparent = internal_rendering_resources.create_texture({
        {"texture_type", RD::TEXTURE_TYPE_2D},
        {"width", 4},
        {"height", 4},
        {"format", RD::DATA_FORMAT_R8G8B8A8_UNORM},
        {"usage_bits", RD::TEXTURE_USAGE_SAMPLING_BIT},
        {"data", TypedArray<PackedByteArray>({transparent_texture_buf})}
    });
}

void RDRenderInterfaceGodot::finalize() {
    internal_rendering_resources.free_all_resources();
}

void RDRenderInterfaceGodot::allocate_render_target(RenderTarget *p_target, const Vector2i &p_size) {
    RenderingServer *rs = RenderingServer::get_singleton();

	if (p_target->size == p_size) return;
	free_render_target(p_target);

    uint64_t main_usage = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;
    p_target->color = rendering_resources.create_texture({
        {"width", p_size.x},
        {"height", p_size.y},
        {"format", RD::DATA_FORMAT_R8G8B8A8_UNORM},
        {"usage_bits", main_usage},
    });

    p_target->framebuffer = rendering_resources.create_framebuffer({
        {"textures", TypedArray<RID>({ p_target->color, context->clip_mask })}
    });
}

void RDRenderInterfaceGodot::free_render_target(RenderTarget *p_target) {
    if (p_target->framebuffer.is_valid()) {
        rendering_resources.free_framebuffer(p_target->framebuffer);
        p_target->framebuffer = RID();
    }

    if (p_target->color.is_valid()) {
        rendering_resources.free_texture(p_target->color);
        p_target->color = RID();
    }
}

void RDRenderInterfaceGodot::allocate_context(Context *p_ctx, const Vector2i &p_size) {
    if (p_ctx->size == p_size) {
        return;
    }
    free_context(p_ctx);

    p_ctx->size = p_size;

    RenderingServer *rs = RenderingServer::get_singleton();

    p_ctx->clip_mask = rendering_resources.create_texture({
        {"width", p_size.x},
        {"height", p_size.y},
        {"format", RD::DATA_FORMAT_S8_UINT},
        {"usage_bits", RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT},
    });
    p_ctx->clip_mask_framebuffer = rendering_resources.create_framebuffer({
        {"textures", TypedArray<RID>({ p_ctx->clip_mask })}
    });

    allocate_render_target(&p_ctx->main_target, p_size);
    allocate_render_target(&p_ctx->back_buffer0, p_size);
    allocate_render_target(&p_ctx->back_buffer1, p_size);
    allocate_render_target(&p_ctx->back_buffer2, p_size);
    allocate_render_target(&p_ctx->blend_target, p_size);
    p_ctx->main_tex = rs->texture_rd_create(p_ctx->main_target.color);
	p_ctx->target_stack.push_back(&p_ctx->main_target);
}

void RDRenderInterfaceGodot::free_context(Context *p_ctx) {
    RenderingServer *rs = RenderingServer::get_singleton();
    
    free_render_target(&p_ctx->main_target);
    free_render_target(&p_ctx->back_buffer0);
    free_render_target(&p_ctx->back_buffer1);
    free_render_target(&p_ctx->back_buffer2);
    free_render_target(&p_ctx->blend_target);

	for (auto render_target : p_ctx->target_stack) {
		free_render_target(render_target);
	}
	p_ctx->target_stack.clear();
    
    if (p_ctx->clip_mask_framebuffer.is_valid()) {
        rendering_resources.free_framebuffer(p_ctx->clip_mask_framebuffer);
        p_ctx->clip_mask_framebuffer = RID();
    }

    if (p_ctx->clip_mask.is_valid()) {
        rendering_resources.free_texture(p_ctx->clip_mask);
        p_ctx->clip_mask = RID();
    }
    if (p_ctx->main_tex.is_valid()) {
        rs->free_rid(p_ctx->main_tex);
        p_ctx->main_tex = RID();
    }
}

RDRenderInterfaceGodot::RenderTarget *RDRenderInterfaceGodot::get_render_target() {
	return context->target_stack[context->target_stack_ptr];
}

void RDRenderInterfaceGodot::push_context(void *&p_ctx, const Vector2i &p_size) {
    Context *ctx = static_cast<Context *>(p_ctx);
    if (ctx == nullptr) {
        ctx = memnew(Context);
        p_ctx = ctx;
    }
    context = ctx;

    allocate_context(ctx, p_size);

    RD *rd = rendering_resources.device();
    rd->texture_clear(ctx->main_target.color, Color(0, 0, 0, 0), 0, 1, 0, 1);

    context->target_stack_ptr = 0;

    clear_debug_commands();
}

void RDRenderInterfaceGodot::pop_context() {
    ERR_FAIL_COND_MSG(!check_if_can_render_with_scissor(), "Cannot happen, scissor must be cleared before finishing rendering");
    render_pass(blit_pass(context->main_target.color, context->back_buffer0.framebuffer));

    RenderPass pass;
    pass.debug_name = "GodotRmlUi_PostProcess";
	pass.shader = shader_post_process;
	pass.pipeline = pipeline_post_process;
	
	pass.uniform_textures.push_back(std::make_pair(context->back_buffer0.color, false));
	pass.framebuffer = context->main_target.framebuffer;

	render_pass(pass);

    context = nullptr;

    flush_debug_commands();
}

void RDRenderInterfaceGodot::draw_context(void *&p_ctx, const RID &p_canvas_item) {
    Context *ctx = static_cast<Context *>(p_ctx);
    RenderingServer *rs = RenderingServer::get_singleton();

    Vector2i size = ctx->size;

    rs->canvas_item_add_texture_rect(
		p_canvas_item,
		Rect2(0, 0, size.x, size.y),
		ctx->get_texture()
	);
}

void RDRenderInterfaceGodot::free_context(void *&p_ctx) {
    Context *ctx = static_cast<Context *>(p_ctx);
    if (ctx == nullptr) return;
    
    free_context(ctx);
    memdelete(ctx);

    p_ctx = nullptr;
}

void RDRenderInterfaceGodot::render_pass(const RenderPass &p_pass) {
	RD *rd = rendering_resources.device();

	TypedArray<RDUniform> uniforms;
	uniforms.resize(p_pass.uniform_textures.size());
	for (int i = p_pass.uniform_textures.size() - 1; i >= 0; i--) {
		Ref<RDUniform> uniform = memnew(RDUniform);
		uniform->set_uniform_type(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
		uniform->set_binding(i);
		uniform->add_id(p_pass.uniform_textures[i].second ? sampler_linear : sampler_nearest);
		uniform->add_id(p_pass.uniform_textures[i].first);
		uniforms[i] = uniform;
	}

	if (p_pass.uniform_buffer.is_valid()) {
		Ref<RDUniform> uniform = memnew(RDUniform);
		uniform->set_uniform_type(RD::UNIFORM_TYPE_STORAGE_BUFFER);
		uniform->set_binding(uniforms.size());
		uniform->add_id(p_pass.uniform_buffer);
		uniforms.append(uniform);
	}

    rd->draw_command_begin_label(p_pass.debug_name, Color(0, 0, 0, 0));

	int64_t draw_list = rd->draw_list_begin(
		p_pass.framebuffer,
		p_pass.draw_flags,
		p_pass.clear_colors,
		1.0,
		p_pass.clear_stencil,
        Rect2(p_pass.region)
	);

	if (scissor_enabled) {
		rd->draw_list_enable_scissor(draw_list, scissor_region);
	} else {
		rd->draw_list_disable_scissor(draw_list);
	}

	bool procedural = true;
	rd->draw_list_bind_render_pipeline(draw_list, p_pass.pipeline);
	if (p_pass.mesh_data) {
		rd->draw_list_bind_vertex_array(draw_list, p_pass.mesh_data->vertex_array);
		rd->draw_list_bind_index_array(draw_list, p_pass.mesh_data->index_array);
		procedural = false;
	}
	if (!uniforms.is_empty()) {
		RID uniform_set = UniformSetCacheRD::get_cache(p_pass.shader, 0, uniforms);
		rd->draw_list_bind_uniform_set(draw_list, uniform_set, 0);
	}
	if (!p_pass.push_const.is_empty()) {
		rd->draw_list_set_push_constant(draw_list, p_pass.push_const, p_pass.push_const.size());
	}

	if (procedural) {
		rd->draw_list_draw(draw_list, false, 1, 3);
	} else {
		rd->draw_list_draw(draw_list, true, 1);
	}

	rd->draw_list_end();

    rd->draw_command_end_label();
}

RDRenderInterfaceGodot::RenderPass RDRenderInterfaceGodot::blit_pass(const RID &p_tex, const RID &p_framebuffer, const Vector2i &p_dst_pos, const Vector2i &p_src_pos, const Vector2i &p_size) {
    RenderPass pass;
    pass.debug_name = "GodotRmlUi_BlitPass";
	pass.shader = shader_blit;
	pass.pipeline = pipeline_blit;
	
	pass.uniform_textures.push_back(std::make_pair(p_tex, false));
	pass.framebuffer = p_framebuffer;

    pass.push_const.resize(16);
    float *push_const = (float *)pass.push_const.ptrw();
    push_const[0] = p_src_pos.x;
    push_const[1] = p_src_pos.y;

    pass.region = Rect2i(p_dst_pos, p_size);

    return pass;
}

bool RDRenderInterfaceGodot::check_if_can_render_with_scissor() const {
    return !scissor_enabled || (scissor_region.size.x > 0 && scissor_region.size.y > 0);
}

Rml::CompiledGeometryHandle RDRenderInterfaceGodot::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
    MeshData *mesh_data = memnew(MeshData);

    RD *rd = rendering_resources.device();

    PackedByteArray position_array, color_array, uv_array, indices_array;
    position_array.resize(vertices.size() * 2 * 4);
    color_array.resize(vertices.size() * 4 * 4);
    uv_array.resize(vertices.size() * 2 * 4);
    indices_array.resize(indices.size() * 4);

    float *position_ptr = (float *)(position_array.ptrw());
    float *color_ptr = (float *)(color_array.ptrw());
    float *uv_ptr = (float *)(uv_array.ptrw());
    int *index_ptr = (int *)(indices_array.ptrw());

    int vert_count = vertices.size();
    int index_count = indices.size();

    for (int i = vert_count - 1; i >= 0; i--) {
        const Rml::Vertex &v = vertices[i];
        position_ptr[i * 2 + 0] = v.position.x;
        position_ptr[i * 2 + 1] = v.position.y;
        color_ptr[i * 4 + 0] = v.colour.red / 255.0;
        color_ptr[i * 4 + 1] = v.colour.green / 255.0;
        color_ptr[i * 4 + 2] = v.colour.blue / 255.0;
        color_ptr[i * 4 + 3] = v.colour.alpha / 255.0;
        uv_ptr[i * 2 + 0] = v.tex_coord.x;
        uv_ptr[i * 2 + 1] = v.tex_coord.y;
    }

    for (int i = index_count - 1; i >= 0; i--) {
        index_ptr[i] = indices[i];
    }

    RID position_buffer = rendering_resources.create_vertex_buffer({
        {"data", position_array}
    });
    RID color_buffer = rendering_resources.create_vertex_buffer({
        {"data", color_array}
    });
    RID uv_buffer = rendering_resources.create_vertex_buffer({
        {"data", uv_array}
    });
    RID index_buffer = rendering_resources.create_index_buffer({
        {"data", indices_array},
        {"count", index_count},
        {"format", RD::INDEX_BUFFER_FORMAT_UINT32}
    });

    RID vertex_array = rendering_resources.create_vertex_array({
        {"count", vert_count},
        {"format", geometry_vertex_format},
        {"buffers", TypedArray<RID>({position_buffer, color_buffer, uv_buffer})}
    });
    RID index_array = rendering_resources.create_index_array({
        {"count", index_count},
        {"buffer", index_buffer}
    });

    mesh_data->position_buffer = position_buffer;
    mesh_data->color_buffer = color_buffer;
    mesh_data->uv_buffer = uv_buffer;
    mesh_data->index_buffer = index_buffer;
    mesh_data->vertex_array = vertex_array;
    mesh_data->index_array = index_array;

    return reinterpret_cast<uintptr_t>(mesh_data);
}

void RDRenderInterfaceGodot::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
    if (!check_if_can_render_with_scissor()) return;
    PUSH_DEBUG_COMMAND(clip_mask_enabled ? "RenderGeometryWithClip" : "RenderGeometry");
	RenderPass pass;
    pass.debug_name = clip_mask_enabled ? "GodotRmlUi_RenderClipMask" : "GodotRmlUi_RenderGeometry";
	pass.shader = shader_geometry;
	pass.pipeline = get_shader_pipeline(PIPELINE_GEOMETRY);

	pass.mesh_data = reinterpret_cast<MeshData *>(geometry);

	pass.push_const.resize(80);
    float *push_const = (float *)pass.push_const.ptrw();
    push_const[0] = 1.0 / context->size.x;
    push_const[1] = 1.0 / context->size.y;
    matrix_to_pointer(push_const + 4, get_final_transform(drawing_matrix, translation));
	
	if (texture != 0) {
		TextureData *tex = reinterpret_cast<TextureData *>(texture);
		pass.uniform_textures.push_back(std::make_pair(tex->rid, tex->linear_filtering));
	} else {
		pass.uniform_textures.push_back(std::make_pair(texture_white, false));
	}
	pass.framebuffer = get_render_target()->framebuffer;

	render_pass(pass);
}

void RDRenderInterfaceGodot::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
	MeshData *mesh_data = reinterpret_cast<MeshData *>(geometry);
    ERR_FAIL_NULL(mesh_data);

    RenderingServer *rs = RenderingServer::get_singleton();
    RD *rd = rs->get_rendering_device();
    
    rendering_resources.free_vertex_array(mesh_data->vertex_array);
    rendering_resources.free_index_array(mesh_data->index_array);
    rendering_resources.free_vertex_buffer(mesh_data->position_buffer);
    rendering_resources.free_vertex_buffer(mesh_data->color_buffer);
    rendering_resources.free_vertex_buffer(mesh_data->uv_buffer);
    rendering_resources.free_index_buffer(mesh_data->index_buffer);

    memdelete(mesh_data);
}

Rml::TextureHandle RDRenderInterfaceGodot::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
	ResourceLoader *rl = ResourceLoader::get_singleton();

    String source_str = rml_to_godot_string(source);

    Ref<Texture2D> tex = rl->load(source_str);
    if (!tex.is_valid()) {
        return 0;
    }

    texture_dimensions.x = tex->get_width();
    texture_dimensions.y = tex->get_height();

    RenderingServer *rs = RenderingServer::get_singleton();

    TextureData *tex_data = memnew(TextureData());
    tex_data->tex_ref = tex;
    tex_data->rid = rs->texture_get_rd_texture(tex->get_rid());

    return reinterpret_cast<uintptr_t>(tex_data);
}

Rml::TextureHandle RDRenderInterfaceGodot::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) {
	PackedByteArray p_data;
    p_data.resize(source.size());

    uint8_t *ptrw = p_data.ptrw();

    memcpy(ptrw, source.data(), source.size());

    RD *rd = rendering_resources.device();

    TextureData *tex_data = memnew(TextureData());
    tex_data->rid = rendering_resources.create_texture({
        {"width", source_dimensions.x},
        {"height", source_dimensions.y},
        {"format", RD::DATA_FORMAT_R8G8B8A8_UNORM},
        {"usage_bits", RD::TEXTURE_USAGE_SAMPLING_BIT},
        {"data", TypedArray<PackedByteArray>({ p_data })}
    });

    return reinterpret_cast<uintptr_t>(tex_data);
}

void RDRenderInterfaceGodot::ReleaseTexture(Rml::TextureHandle texture) {
	TextureData *tex_data = reinterpret_cast<TextureData *>(texture);

    // Is a generated texture
    if (!tex_data->tex_ref.is_valid()) {
        rendering_resources.free_texture(tex_data->rid);
    }
    memdelete(tex_data);
}

void RDRenderInterfaceGodot::EnableScissorRegion(bool enable) {
    PUSH_DEBUG_COMMAND(enable ? "EnableScissorRegion" : "DisableScissorRegion");
	scissor_enabled = enable;
}

void RDRenderInterfaceGodot::SetScissorRegion(Rml::Rectanglei region) {
    PUSH_DEBUG_COMMAND(vformat(
        "SetScissorRegion: (%d, %d, %d, %d)", 
        region.Position().x, region.Position().y,
        region.Size().x, region.Size().y
    ));
	scissor_region = Rect2(
        region.Position().x,
        region.Position().y,
        region.Size().x,
        region.Size().y
    );
}

void RDRenderInterfaceGodot::EnableClipMask(bool enable) {
    PUSH_DEBUG_COMMAND("EnableClipMask");
    clip_mask_enabled = enable;
}

void RDRenderInterfaceGodot::RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) {
    if (!check_if_can_render_with_scissor()) return;
    PUSH_DEBUG_COMMAND(vformat("RenderToClipMask: %d", (int)operation));
    RenderPass pass;
    pass.debug_name = "GodotRmlUi_RenderToClipMask";
	pass.shader = shader_clip_mask;
    switch (operation) {
        case Rml::ClipMaskOperation::Set: {
            pass.pipeline = pipeline_clip_mask_set;
        } break;
        case Rml::ClipMaskOperation::Intersect: {
            pass.pipeline = pipeline_clip_mask_intersect;
        } break;
        case Rml::ClipMaskOperation::SetInverse: {
            pass.pipeline = pipeline_clip_mask_set_inverse;
        } break;
    }

	pass.mesh_data = reinterpret_cast<MeshData *>(geometry);

	pass.push_const.resize(80);
    float *push_const = (float *)pass.push_const.ptrw();
    push_const[0] = 1.0 / context->size.x;
    push_const[1] = 1.0 / context->size.y;
    matrix_to_pointer(push_const + 4, get_final_transform(drawing_matrix, translation));
	
    uint64_t clear_flags = RD::DRAW_CLEAR_STENCIL;
    uint32_t clear_stencil_value = 0;
    if (operation == Rml::ClipMaskOperation::SetInverse) {
        clear_stencil_value = 1;
    }
    if (operation == Rml::ClipMaskOperation::Intersect) {
        clear_flags = RD::DRAW_DEFAULT_ALL;
    }

	pass.framebuffer = context->clip_mask_framebuffer;
    pass.draw_flags = clear_flags;
    pass.clear_stencil = clear_stencil_value;

	render_pass(pass);
}

void RDRenderInterfaceGodot::SetTransform(const Rml::Matrix4f* transform) {
    if (transform == nullptr) {
        PUSH_DEBUG_COMMAND("Clear transform");
        drawing_matrix = Rml::Matrix4f::Identity();
        return;
    }
    drawing_matrix = *transform;

    Rml::Vector4f origin = transform->operator*(Rml::Vector4f(0, 0, 0, 1));
    Rml::Vector4f x = transform->operator*(Rml::Vector4f(1, 0, 0, 0));
    origin /= origin.w;

    float rot = Math::atan2(x.y, x.x);

    PUSH_DEBUG_COMMAND(vformat("SetTransform: (%.1f, %.1f, %.1f, %.1f)", origin.x, origin.y, origin.z, RAD_TO_DEG * rot));
}

Rml::LayerHandle RDRenderInterfaceGodot::PushLayer() {
    PUSH_DEBUG_COMMAND("PushLayer");
    context->target_stack_ptr++;
    RenderTarget *target;
    if (context->target_stack_ptr == context->target_stack.size()) {
        target = memnew(RenderTarget);
        context->target_stack.push_back(target);
    } else {
        target = context->target_stack[context->target_stack_ptr];
    }

    allocate_render_target(target, context->size);

    RD *rd = rendering_resources.device();
    rd->texture_clear(target->color, Color(0, 0, 0, 0), 0, 1, 0, 1);

    return context->target_stack_ptr;
}

void RDRenderInterfaceGodot::CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination, Rml::BlendMode blend_mode, Rml::Span<const Rml::CompiledFilterHandle> filters) {
    if (!check_if_can_render_with_scissor()) return;
    PUSH_DEBUG_COMMAND(vformat("CompositeLayers: %d, %d %d", source, destination, (int)blend_mode));
    RenderTarget *source_target = context->target_stack[source];
    RenderTarget *destination_target = context->target_stack[destination];

    render_pass(blit_pass(source_target->color, context->back_buffer0.framebuffer));
    for (auto it : filters) {
        RenderPasses *passes = reinterpret_cast<RenderPasses *>(it);
        for (auto pass : passes->passes) {
            render_pass(pass);
        }
    }
    render_pass(blit_pass(context->back_buffer0.color, context->back_buffer1.framebuffer));

    RenderPass pass;
    pass.debug_name = "GodotRmlUi_CompositeLayers";
	pass.shader = shader_layer_composition;
    pass.pipeline = get_shader_pipeline(PIPELINE_LAYER_COMPOSITION);

	pass.push_const.resize(16);
    unsigned int *push_const = (unsigned int *)pass.push_const.ptrw();
    push_const[0] = (unsigned int)blend_mode;
	
    pass.uniform_textures.push_back(std::make_pair(destination_target->color, false));
    pass.uniform_textures.push_back(std::make_pair(context->back_buffer1.color, false));

	pass.framebuffer = context->back_buffer0.framebuffer;

	render_pass(pass);
    render_pass(blit_pass(context->back_buffer0.color, destination_target->framebuffer));
}

void RDRenderInterfaceGodot::PopLayer() {
    PUSH_DEBUG_COMMAND("PopLayer");
    context->target_stack_ptr--;
}

Rml::TextureHandle RDRenderInterfaceGodot::SaveLayerAsTexture() {
    PUSH_DEBUG_COMMAND("SaveLayerAsTexture");
    RenderTarget *target = get_render_target();

    Rect2i region = Rect2i(0, 0, context->size.x, context->size.y);
    if (scissor_enabled) {
        ERR_FAIL_COND_V_MSG(!check_if_can_render_with_scissor(), 0, "Cannot happen, scissor must be valid");
        region = scissor_region;
    }

    TextureData *tex_data = memnew(TextureData());
    tex_data->rid = rendering_resources.create_texture({
        {"width", region.size.x},
        {"height", region.size.y},
        {"format", RD::DATA_FORMAT_R8G8B8A8_UNORM},
        {"usage_bits", RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT}
    });
    tex_data->linear_filtering = false;

    RD *rd = rendering_resources.device();
    RID fb = rd->framebuffer_create({ tex_data->rid });

    render_pass(blit_pass(target->color, fb, Vector2i(), region.position, region.size));
    rd->free_rid(fb);

    return reinterpret_cast<uintptr_t>(tex_data);
}

Rml::CompiledFilterHandle RDRenderInterfaceGodot::SaveLayerAsMaskImage() {
    PUSH_DEBUG_COMMAND("SaveLayerAsMaskImage");
    RenderTarget *target = get_render_target();

    RenderPasses *passes = memnew(RenderPasses);

    bool valid = check_if_can_render_with_scissor();
    if (valid) {
        render_pass(blit_pass(target->color, context->blend_target.framebuffer));
    }

    RenderPass pass;
    pass.debug_name = "GodotRmlUi_SaveLayerAsMaskImage";
	pass.shader = shaders[SHADER_FILTER_MASK];
    pass.pipeline = pipelines[PIPELINE_FILTER_MASK];
	
    pass.uniform_textures.push_back(std::make_pair(context->back_buffer0.color, false));
    if (valid) {
        pass.uniform_textures.push_back(std::make_pair(context->blend_target.color, false));
    } else {
        pass.uniform_textures.push_back(std::make_pair(texture_transparent, false));
    }

	pass.framebuffer = context->back_buffer1.framebuffer;
    passes->passes.push_back(pass);

    passes->passes.push_back(blit_pass(context->back_buffer1.color, context->back_buffer0.framebuffer));

    return reinterpret_cast<uintptr_t>(passes);
}

Rml::CompiledFilterHandle RDRenderInterfaceGodot::CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) {
    RenderPasses params;

    // For the created passes array, 
    // the first pass must read from back_buffer0 and the last write to back_buffer0

    if (name == "opacity") {
        const float value = Rml::Get(parameters, "value", 1.f);

        RenderPass pass;
        pass.debug_name = "GodotRmlUi_Opacity";
        pass.shader = shaders[SHADER_FILTER_MODULATE];
        pass.pipeline = pipelines[PIPELINE_FILTER_MODULATE];
        pass.uniform_textures.push_back(std::make_pair(context->back_buffer0.color, false));
        pass.framebuffer = context->back_buffer1.framebuffer;

        pass.push_const = PackedByteArray();
        pass.push_const.resize(32);

        float *push_const_ptr = (float *)pass.push_const.ptrw();
        push_const_ptr[0] = 1.0f;
        push_const_ptr[1] = 1.0f;
        push_const_ptr[2] = 1.0f;
        push_const_ptr[3] = value;
        push_const_ptr[4] = 0;

        params.passes.push_back(pass);
        params.passes.push_back(blit_pass(context->back_buffer1.color, context->back_buffer0.framebuffer));
    } else if (name == "blur") {
        const float sigma = Rml::Get(parameters, "sigma", 0.f);

        RenderPass pass;
        pass.debug_name = "GodotRmlUi_BlurH";
        pass.shader = shaders[SHADER_FILTER_BLUR];
        pass.pipeline = pipelines[PIPELINE_FILTER_BLUR];
        pass.uniform_textures.push_back(std::make_pair(context->back_buffer0.color, false));
        pass.framebuffer = context->back_buffer1.framebuffer;
        
        // Horizontal pass
        pass.push_const = PackedByteArray();
        pass.push_const.resize(32);
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        push_const_ptr[0] = 1.0f;
        push_const_ptr[1] = 0.0f;
        push_const_ptr[2] = 0.0f;
        push_const_ptr[3] = 0.0f;
        push_const_ptr[4] = sigma;

        params.passes.push_back(pass);

        // Vertial pass
        pass.debug_name = "GodotRmlUi_BlurV";
        pass.uniform_textures.clear();
        pass.uniform_textures.push_back(std::make_pair(context->back_buffer1.color, false));
        pass.framebuffer = context->back_buffer0.framebuffer;
        pass.push_const = PackedByteArray();
        pass.push_const.resize(32);
        push_const_ptr = (float *)pass.push_const.ptrw();
        push_const_ptr[0] = 0.0f;
        push_const_ptr[1] = 1.0f;
        push_const_ptr[2] = 0.0f;
        push_const_ptr[3] = 0.0f;
        push_const_ptr[4] = sigma;

        params.passes.push_back(pass);
    } else if (name == "drop-shadow") {
        const float sigma = Rml::Get(parameters, "sigma", 0.f);
        const Rml::Colourb color = Rml::Get(parameters, "color", Rml::Colourb());
        const Rml::Vector2f offset = Rml::Get(parameters, "offset", Rml::Vector2f());

        // Blur horizontal pass
        RenderPass pass;
        pass.debug_name = "GodotRmlUi_DropShadowBlurH";
        pass.shader = shaders[SHADER_FILTER_BLUR];
        pass.pipeline = pipelines[PIPELINE_FILTER_BLUR];
        pass.uniform_textures.push_back(std::make_pair(context->back_buffer0.color, false));
        pass.framebuffer = context->back_buffer1.framebuffer;

        pass.push_const = PackedByteArray();
        pass.push_const.resize(32);
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        push_const_ptr[0] = 1.0f;
        push_const_ptr[1] = 0.0f;
        push_const_ptr[2] = offset.x;
        push_const_ptr[3] = offset.y;
        push_const_ptr[4] = sigma;

        params.passes.push_back(pass);

        // Blur vertial pass
        pass.debug_name = "GodotRmlUi_DropShadowBlurV";
        pass.uniform_textures.clear();
        pass.uniform_textures.push_back(std::make_pair(context->back_buffer1.color, false));
        pass.framebuffer = context->back_buffer2.framebuffer;
        pass.push_const = PackedByteArray();
        pass.push_const.resize(32);
        push_const_ptr = (float *)pass.push_const.ptrw();
        push_const_ptr[0] = 0.0f;
        push_const_ptr[1] = 1.0f;
        push_const_ptr[2] = 0;
        push_const_ptr[3] = 0;
        push_const_ptr[4] = sigma;

        params.passes.push_back(pass);

        // Drop shadow pass
        pass.debug_name = "GodotRmlUi_DropShadow";
        pass.shader = shaders[SHADER_FILTER_DROP_SHADOW];
        pass.pipeline = pipelines[PIPELINE_FILTER_DROP_SHADOW];
        pass.uniform_textures.clear();
        pass.uniform_textures.push_back(std::make_pair(context->back_buffer0.color, false));
        pass.uniform_textures.push_back(std::make_pair(context->back_buffer2.color, false));
        pass.framebuffer = context->back_buffer1.framebuffer;

        pass.push_const = PackedByteArray();
        pass.push_const.resize(16);
        push_const_ptr = (float *)pass.push_const.ptrw();
        push_const_ptr[0] = color.red / 255.0;
        push_const_ptr[1] = color.green / 255.0;
        push_const_ptr[2] = color.blue / 255.0;
        push_const_ptr[3] = color.alpha / 255.0;

        params.passes.push_back(pass);
        params.passes.push_back(blit_pass(context->back_buffer1.color, context->back_buffer0.framebuffer));
    } else if (name == "brightness") {
        const float value = Rml::Get(parameters, "value", 1.f);

        RenderPass pass;
        pass.debug_name = "GodotRmlUi_Brightness";
        pass.shader = shaders[SHADER_FILTER_MODULATE];
        pass.pipeline = pipelines[PIPELINE_FILTER_MODULATE];
        pass.uniform_textures.push_back(std::make_pair(context->back_buffer0.color, false));
        pass.framebuffer = context->back_buffer1.framebuffer;

        pass.push_const = PackedByteArray();
        pass.push_const.resize(32);
        
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        push_const_ptr[0] = value;
        push_const_ptr[1] = value;
        push_const_ptr[2] = value;
        push_const_ptr[3] = 1.0f;
        push_const_ptr[4] = 0;

        params.passes.push_back(pass);
        params.passes.push_back(blit_pass(context->back_buffer1.color, context->back_buffer0.framebuffer));
    } else if (name == "contrast") {
        const float value = Rml::Get(parameters, "value", 1.f);
        const float gray = 0.5f - 0.5f * value;

        Rml::Matrix4f color_matrix = Rml::Matrix4f::Diag(value, value, value, 1.0f);
        color_matrix.SetColumn(3, Rml::Vector4f(gray, gray, gray, 1.0f));

        RenderPass pass;
        pass.debug_name = "GodotRmlUi_Contrast";
        pass.shader = shaders[SHADER_FILTER_COLOR_MATRIX];
        pass.pipeline = pipelines[PIPELINE_FILTER_COLOR_MATRIX];
        pass.uniform_textures.push_back(std::make_pair(context->back_buffer0.color, false));
        pass.framebuffer = context->back_buffer1.framebuffer;

        pass.push_const = PackedByteArray();
        pass.push_const.resize(64);
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        matrix_to_pointer(push_const_ptr, color_matrix);

        params.passes.push_back(pass);
        params.passes.push_back(blit_pass(context->back_buffer1.color, context->back_buffer0.framebuffer));
    } else if (name == "invert") {
        const float value = Rml::Get(parameters, "value", 0.f);
        const float inverted = 1.f - 2.f * value;

        Rml::Matrix4f color_matrix = Rml::Matrix4f::Diag(inverted, inverted, inverted, 1.f);
        color_matrix.SetColumn(3, Rml::Vector4f(value, value, value, 1.f));

        RenderPass pass;
        pass.debug_name = "GodotRmlUi_Invert";
        pass.shader = shaders[SHADER_FILTER_COLOR_MATRIX];
        pass.pipeline = pipelines[PIPELINE_FILTER_COLOR_MATRIX];
        pass.uniform_textures.push_back(std::make_pair(context->back_buffer0.color, false));
        pass.framebuffer = context->back_buffer1.framebuffer;

        pass.push_const = PackedByteArray();
        pass.push_const.resize(64);
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        matrix_to_pointer(push_const_ptr, color_matrix);

        params.passes.push_back(pass);
        params.passes.push_back(blit_pass(context->back_buffer1.color, context->back_buffer0.framebuffer));
    } else if (name == "grayscale") {
        const float value = Rml::Get(parameters, "value", 1.f);
        const float rev_value = 1.f - value;
		const Rml::Vector3f gray = value * Rml::Vector3f(0.2126f, 0.7152f, 0.0722f);

        Rml::Matrix4f color_matrix = Rml::Matrix4f::FromRows(
			{gray.x + rev_value, gray.y,             gray.z,             0.f},
			{gray.x,             gray.y + rev_value, gray.z,             0.f},
			{gray.x,             gray.y,             gray.z + rev_value, 0.f},
			{0.f,                0.f,                0.f,                1.f}
		);

        RenderPass pass;
        pass.debug_name = "GodotRmlUi_Grayscale";
        pass.shader = shaders[SHADER_FILTER_COLOR_MATRIX];
        pass.pipeline = pipelines[PIPELINE_FILTER_COLOR_MATRIX];
        pass.uniform_textures.push_back(std::make_pair(context->back_buffer0.color, false));
        pass.framebuffer = context->back_buffer1.framebuffer;

        pass.push_const = PackedByteArray();
        pass.push_const.resize(64);
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        matrix_to_pointer(push_const_ptr, color_matrix);

        params.passes.push_back(pass);
        params.passes.push_back(blit_pass(context->back_buffer1.color, context->back_buffer0.framebuffer));
    } else if (name == "sepia") {
        const float value = Rml::Get(parameters, "value", 1.f);
        const float rev_value = 1.f - value;
		const Rml::Vector3f r_mix = value * Rml::Vector3f(0.393f, 0.769f, 0.189f);
		const Rml::Vector3f g_mix = value * Rml::Vector3f(0.349f, 0.686f, 0.168f);
		const Rml::Vector3f b_mix = value * Rml::Vector3f(0.272f, 0.534f, 0.131f);

        Rml::Matrix4f color_matrix = Rml::Matrix4f::FromRows(
			{r_mix.x + rev_value, r_mix.y,             r_mix.z,             0.f},
			{g_mix.x,             g_mix.y + rev_value, g_mix.z,             0.f},
			{b_mix.x,             b_mix.y,             b_mix.z + rev_value, 0.f},
			{0.f,                 0.f,                 0.f,                 1.f}
		);

        RenderPass pass;
        pass.debug_name = "GodotRmlUi_Sepia";
        pass.shader = shaders[SHADER_FILTER_COLOR_MATRIX];
        pass.pipeline = pipelines[PIPELINE_FILTER_COLOR_MATRIX];
        pass.uniform_textures.push_back(std::make_pair(context->back_buffer0.color, false));
        pass.framebuffer = context->back_buffer1.framebuffer;

        pass.push_const = PackedByteArray();
        pass.push_const.resize(64);
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        matrix_to_pointer(push_const_ptr, color_matrix);

        params.passes.push_back(pass);
        params.passes.push_back(blit_pass(context->back_buffer1.color, context->back_buffer0.framebuffer));
    } else if (name == "hue-rotate") {
        const float value = Rml::Get(parameters, "value", 0.f);

        RenderPass pass;
        pass.debug_name = "GodotRmlUi_HueRotate";
        pass.shader = shaders[SHADER_FILTER_MODULATE];
        pass.pipeline = pipelines[PIPELINE_FILTER_MODULATE];
        pass.uniform_textures.push_back(std::make_pair(context->back_buffer0.color, false));
        pass.framebuffer = context->back_buffer1.framebuffer;

        pass.push_const = PackedByteArray();
        pass.push_const.resize(32);
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        unsigned int *push_const_int_ptr = (unsigned int *)pass.push_const.ptrw();
        push_const_ptr[0] = value / Math_TAU;
        push_const_ptr[1] = 1.0f;
        push_const_ptr[2] = 1.0f;
        push_const_ptr[3] = 1.0f;
        push_const_int_ptr[4] = 1; // HSV

        params.passes.push_back(pass);
        params.passes.push_back(blit_pass(context->back_buffer1.color, context->back_buffer0.framebuffer));
    } else if (name == "saturate") {
        const float value = Rml::Get(parameters, "value", 1.f);

		Rml::Matrix4f color_matrix = Rml::Matrix4f::FromRows(
			{0.213f + 0.787f * value,  0.715f - 0.715f * value,  0.072f - 0.072f * value,  0.f},
			{0.213f - 0.213f * value,  0.715f + 0.285f * value,  0.072f - 0.072f * value,  0.f},
			{0.213f - 0.213f * value,  0.715f - 0.715f * value,  0.072f + 0.928f * value,  0.f},
			{0.f,                      0.f,                      0.f,                      1.f}
		);

        RenderPass pass;
        pass.debug_name = "GodotRmlUi_Saturate";
        pass.shader = shaders[SHADER_FILTER_COLOR_MATRIX];
        pass.pipeline = pipelines[PIPELINE_FILTER_COLOR_MATRIX];
        pass.uniform_textures.push_back(std::make_pair(context->back_buffer0.color, false));
        pass.framebuffer = context->back_buffer1.framebuffer;

        pass.push_const = PackedByteArray();
        pass.push_const.resize(64);
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        matrix_to_pointer(push_const_ptr, color_matrix);

        params.passes.push_back(pass);
        params.passes.push_back(blit_pass(context->back_buffer1.color, context->back_buffer0.framebuffer));
    }

    RenderPasses *passes = memnew(RenderPasses(std::move(params)));
    return reinterpret_cast<uintptr_t>(passes);
}

void RDRenderInterfaceGodot::ReleaseFilter(Rml::CompiledFilterHandle filter) {
    RenderPasses *passes = reinterpret_cast<RenderPasses *>(filter);

    for (auto pass : passes->passes) {
        if (pass.uniform_buffer.is_valid()) {
            rendering_resources.free_storage_buffer(pass.uniform_buffer);
        }
    }

    memdelete(passes);
}

Rml::CompiledShaderHandle RDRenderInterfaceGodot::CompileShader(const Rml::String& name, const Rml::Dictionary& parameters) {
    ShaderInfo params;

    if (name == "linear-gradient" || name == "radial-gradient" || name == "conic-gradient") {
        params.shader = shaders[SHADER_GRADIENT];
        params.pipeline_id = PIPELINE_GRADIENT;

        const bool repeating = Rml::Get(parameters, "repeating", false);
        const Rml::ColorStopList stop_list = Rml::Get(parameters, "color_stop_list", Rml::ColorStopList());
        Rml::Vector2f p, v;
        unsigned int grad_type = 0;

        if (name == "linear-gradient") {
            grad_type = 1;
            p = Rml::Get(parameters, "p0", Rml::Vector2f(0.f));
            v = Rml::Get(parameters, "p1", Rml::Vector2f(0.f)) - p;
        } else if (name == "radial-gradient") {
            grad_type = 2;
            p = Rml::Get(parameters, "center", Rml::Vector2f(0.f));
            v = Rml::Vector2f(1.f) / Rml::Get(parameters, "radius", Rml::Vector2f(1.f));
        } else if (name == "conic-gradient") {
            grad_type = 3;
            p = Rml::Get(parameters, "center", Rml::Vector2f(0.f));
            const float angle = Rml::Get(parameters, "angle", 0.f);
            v = Rml::Vector2f(Rml::Math::Cos(angle), Rml::Math::Sin(angle));
        }

        int stop_count = stop_list.size();

        PackedByteArray storage_buffer;
        storage_buffer.resize(stop_count * (4 + 1) * 4); // N * (rgba + position) * 4 bytes
        float *buffer_ptr = (float *)storage_buffer.ptrw();

        for (int i = 0; i < stop_count; i++) {
            Rml::ColourbPremultiplied color = stop_list[i].color;
            float position = stop_list[i].position.number;
            buffer_ptr[i * 5 + 0] = color.red / 255.0;
            buffer_ptr[i * 5 + 1] = color.green / 255.0;
            buffer_ptr[i * 5 + 2] = color.blue / 255.0;
            buffer_ptr[i * 5 + 3] = color.alpha / 255.0;
            buffer_ptr[i * 5 + 4] = position;
        }

        params.uniform_buffer = rendering_resources.create_storage_buffer({
            {"data", storage_buffer}
        });

        params.push_const = PackedByteArray();
        params.push_const.resize(112);
        float *push_const_ptr = (float *)params.push_const.ptrw();
        unsigned int *push_const_uint_ptr = (unsigned int *)params.push_const.ptrw();
        push_const_uint_ptr[20] = (grad_type << 1) | (repeating ? 1 : 0);
        push_const_uint_ptr[21] = stop_count;
        push_const_ptr[22] = p.x;
        push_const_ptr[23] = p.y;
        push_const_ptr[24] = v.x;
        push_const_ptr[25] = v.y;
    }

    ShaderInfo *shader = memnew(ShaderInfo(std::move(params)));
    return reinterpret_cast<uintptr_t>(shader);
}

void RDRenderInterfaceGodot::RenderShader(Rml::CompiledShaderHandle shader, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
    if (!check_if_can_render_with_scissor()) return;
    PUSH_DEBUG_COMMAND("RenderShader");
    ShaderInfo *info = reinterpret_cast<ShaderInfo *>(shader);

    RenderPass pass;
    pass.debug_name = "GodotRmlUi_RenderShader";
	pass.shader = info->shader;
	pass.pipeline = get_shader_pipeline(info->pipeline_id);

	pass.mesh_data = reinterpret_cast<MeshData *>(geometry);

	pass.push_const = info->push_const;
    float *push_const = (float *)pass.push_const.ptrw();
    push_const[0] = 1.0 / context->size.x;
    push_const[1] = 1.0 / context->size.y;
    matrix_to_pointer(push_const + 4, get_final_transform(drawing_matrix, translation));

    pass.uniform_buffer = info->uniform_buffer;
	
	if (texture != 0) {
		TextureData *tex = reinterpret_cast<TextureData *>(texture);
		pass.uniform_textures.push_back(std::make_pair(tex->rid, tex->linear_filtering));
	} else {
		pass.uniform_textures.push_back(std::make_pair(texture_white, false));
	}
	pass.framebuffer = get_render_target()->framebuffer;

	render_pass(pass);
}

void RDRenderInterfaceGodot::ReleaseShader(Rml::CompiledShaderHandle shader) {
    ShaderInfo *info = reinterpret_cast<ShaderInfo *>(shader);

    if (info->uniform_buffer.is_valid()) {
        rendering_resources.free_storage_buffer(info->uniform_buffer);
    }
    
    memdelete(info);
}