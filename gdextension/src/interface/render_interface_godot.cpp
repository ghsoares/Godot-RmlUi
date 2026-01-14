#include "render_interface_godot.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/texture.hpp>
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

const uint64_t PIPELINE_FILTER_MODULATE = 1;
const uint64_t PIPELINE_FILTER_BLUR = 2;
const uint64_t PIPELINE_FILTER_DROP_SHADOW = 3;
const uint64_t PIPELINE_FILTER_COLOR_MATRIX = 4;
const uint64_t PIPELINE_FILTER_MASK = 4;

const uint64_t PIPELINE_GRADIENT = 16;

Rml::Matrix4f get_final_transform(const Rml::Matrix4f &p_drawing_matrix, const Rml::Vector2f &translation) {
    return Rml::Matrix4f::Translate(Rml::Vector3f(translation.x, translation.y, 0.0)) * p_drawing_matrix;
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

RID *filter_get_texture(RenderFrame *frame, const BufferTarget &target) {
	switch (target) {
		case BufferTarget::PRIMARY_BUFFER0: return &frame->primary_filter_target.color0;
		case BufferTarget::PRIMARY_BUFFER1: return &frame->primary_filter_target.color1;
		case BufferTarget::SECONDARY_BUFFER0: return &frame->secondary_filter_target.color0;
		case BufferTarget::SECONDARY_BUFFER1: return &frame->secondary_filter_target.color1;
		case BufferTarget::BLEND_BUFFER0: return &frame->blend_mask_target.color0;
		case BufferTarget::BLEND_BUFFER1: return &frame->blend_mask_target.color1;
	}
	return nullptr;
}

RID *filter_get_framebuffer(RenderFrame *frame, const BufferTarget &target) {
	switch (target) {
		case BufferTarget::PRIMARY_BUFFER0: return &frame->primary_filter_target.framebuffer0;
		case BufferTarget::PRIMARY_BUFFER1: return &frame->primary_filter_target.framebuffer1;
		case BufferTarget::SECONDARY_BUFFER0: return &frame->secondary_filter_target.framebuffer0;
		case BufferTarget::SECONDARY_BUFFER1: return &frame->secondary_filter_target.framebuffer1;
		case BufferTarget::BLEND_BUFFER0: return &frame->blend_mask_target.framebuffer0;
		case BufferTarget::BLEND_BUFFER1: return &frame->blend_mask_target.framebuffer1;
	}
	return nullptr;
}

RenderInterfaceGodot *RenderInterfaceGodot::singleton = nullptr;

RenderInterfaceGodot *RenderInterfaceGodot::get_singleton() {
    return singleton;
}

void RenderInterfaceGodot::create_shader_pipeline(uint64_t p_id, const std::map<String, Variant> &p_params) {
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
    RID pipeline_not_clipping = internal_rendering_resources.create_pipeline(p_params);
    RID pipeline_clipping = internal_rendering_resources.create_pipeline(clipping_params);

    pipelines[p_id] = {pipeline_not_clipping, pipeline_clipping};
}

RID RenderInterfaceGodot::get_shader_pipeline(uint64_t p_id) const {
    return clip_mask_enabled ? std::get<1>(pipelines.at(p_id)) : std::get<0>(pipelines.at(p_id));
}

void RenderInterfaceGodot::initialize() {
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

    pipeline_blit = internal_rendering_resources.create_pipeline({
        {"shader", shader_blit},
        {"framebuffer_format", color_framebuffer_format},
        {"attachment_count", 3}
    });
    pipeline_geometry = internal_rendering_resources.create_pipeline({
        {"shader", shader_geometry},
        {"framebuffer_format", geometry_framebuffer_format},
        {"vertex_format", geometry_vertex_format},
        {"attachment_count", 3}
    });
    pipeline_geometry_clipping = internal_rendering_resources.create_pipeline({
        {"shader", shader_geometry},
        {"framebuffer_format", geometry_framebuffer_format},
        {"vertex_format", geometry_vertex_format},
        {"attachment_count", 3},
        
        {"enable_stencil", true},
        {"op_ref", 0x01},
        {"op_compare", RD::COMPARE_OP_EQUAL},
        {"op_compare_mask", 0xff},
        {"op_write_mask", 0x00},
        {"op_pass", RD::STENCIL_OP_KEEP},
        {"op_fail", RD::STENCIL_OP_KEEP},
    });

    pipeline_clip_mask_set = internal_rendering_resources.create_pipeline({
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
    pipeline_clip_mask_set_inverse = internal_rendering_resources.create_pipeline({
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
    pipeline_clip_mask_intersect = internal_rendering_resources.create_pipeline({
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

    pipeline_layer_composition = internal_rendering_resources.create_pipeline({
        {"shader", shader_layer_composition},
        {"framebuffer_format", geometry_framebuffer_format},
        {"attachment_count", 3},
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

    create_shader_pipeline(PIPELINE_FILTER_MODULATE, {
        {"shader", shaders[SHADER_FILTER_MODULATE]},
        {"framebuffer_format", geometry_framebuffer_format},
        {"attachment_count", 3},
    });
    create_shader_pipeline(PIPELINE_FILTER_BLUR, {
        {"shader", shaders[SHADER_FILTER_BLUR]},
        {"framebuffer_format", geometry_framebuffer_format},
        {"attachment_count", 3},
    });
    create_shader_pipeline(PIPELINE_FILTER_DROP_SHADOW, {
        {"shader", shaders[SHADER_FILTER_DROP_SHADOW]},
        {"framebuffer_format", geometry_framebuffer_format},
        {"attachment_count", 3},
    });
    create_shader_pipeline(PIPELINE_FILTER_COLOR_MATRIX, {
        {"shader", shaders[SHADER_FILTER_COLOR_MATRIX]},
        {"framebuffer_format", geometry_framebuffer_format},
        {"attachment_count", 3},
    });
    create_shader_pipeline(PIPELINE_FILTER_MASK, {
        {"shader", shaders[SHADER_FILTER_MASK]},
        {"framebuffer_format", geometry_framebuffer_format},
        {"attachment_count", 3},
    });

    create_shader_pipeline(PIPELINE_GRADIENT, {
        {"shader", shaders[SHADER_GRADIENT]},
        {"framebuffer_format", geometry_framebuffer_format},
        {"vertex_format", geometry_vertex_format},
        {"attachment_count", 3},
    });

    sampler_nearest = internal_rendering_resources.create_sampler({});
    sampler_linear = internal_rendering_resources.create_sampler({
        {"filter", RD::SAMPLER_FILTER_LINEAR}
    });

    PackedByteArray white_texture_buf;
    white_texture_buf.resize(4 * 4 * 4); // width * height * rgba
    white_texture_buf.fill(255);

    texture_white = internal_rendering_resources.create_texture({
        {"texture_type", RD::TEXTURE_TYPE_2D},
        {"width", 4},
        {"height", 4},
        {"format", RD::DATA_FORMAT_R8G8B8A8_UNORM},
        {"usage_bits", RD::TEXTURE_USAGE_SAMPLING_BIT},
        {"data", TypedArray<PackedByteArray>({white_texture_buf})}
    });
}

void RenderInterfaceGodot::uninitialize() {
    internal_rendering_resources.free_all_resources();
}

void RenderInterfaceGodot::allocate_render_target(RenderTarget *p_target) {
    RenderingServer *rs = RenderingServer::get_singleton();

    uint64_t main_usage = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;
    p_target->color0 = rendering_resources.create_texture({
        {"width", target_frame->current_size.x},
        {"height", target_frame->current_size.y},
        {"format", RD::DATA_FORMAT_R8G8B8A8_UNORM},
        {"usage_bits", main_usage},
    });
    p_target->color1 = rendering_resources.create_texture({
        {"width", target_frame->current_size.x},
        {"height", target_frame->current_size.y},
        {"format", RD::DATA_FORMAT_R8G8B8A8_UNORM},
        {"usage_bits", main_usage},
    });

    p_target->framebuffer0 = rendering_resources.create_framebuffer({
        {"textures", TypedArray<RID>({ p_target->color0, target_frame->clip_mask })}
    });
    p_target->framebuffer1 = rendering_resources.create_framebuffer({
        {"textures", TypedArray<RID>({ p_target->color1, target_frame->clip_mask })}
    });
}

void RenderInterfaceGodot::free_render_target(RenderTarget *p_target) {
    if (p_target->framebuffer0.is_valid()) {
        rendering_resources.free_framebuffer(p_target->framebuffer0);
    }
    if (p_target->framebuffer1.is_valid()) {
        rendering_resources.free_framebuffer(p_target->framebuffer1);
    }

    if (p_target->color0.is_valid()) {
        rendering_resources.free_texture(p_target->color0);
    }
    if (p_target->color1.is_valid()) {
        rendering_resources.free_texture(p_target->color1);
    }
}

void RenderInterfaceGodot::allocate_render_frame() {
    if (target_frame->current_size == target_frame->desired_size) {
        return;
    }

    target_frame->current_size = target_frame->desired_size;

    RenderingServer *rs = RenderingServer::get_singleton();

    free_render_frame(target_frame);

    target_frame->clip_mask = rendering_resources.create_texture({
        {"width", target_frame->current_size.x},
        {"height", target_frame->current_size.y},
        {"format", RD::DATA_FORMAT_S8_UINT},
        {"usage_bits", RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT},
    });
    target_frame->clip_mask_framebuffer = rendering_resources.create_framebuffer({
        {"textures", TypedArray<RID>({ target_frame->clip_mask })}
    });

    allocate_render_target(&target_frame->main_target);
    allocate_render_target(&target_frame->primary_filter_target);
    allocate_render_target(&target_frame->secondary_filter_target);
    allocate_render_target(&target_frame->blend_mask_target);
    target_frame->main_tex = rs->texture_rd_create(target_frame->main_target.color0);
}

void RenderInterfaceGodot::blit_texture(const RID &p_dst, const RID &p_src, const Vector2i &p_dst_pos, const Vector2i &p_src_pos, const Vector2i &p_size) {
    RD *rd = internal_rendering_resources.device();

    Ref<RDUniform> screen_texture_uniform = memnew(RDUniform);
    screen_texture_uniform->set_uniform_type(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
    screen_texture_uniform->set_binding(0);
    screen_texture_uniform->add_id(sampler_nearest);
    screen_texture_uniform->add_id(p_src);

    RID uniform_set = UniformSetCacheRD::get_cache(shader_blit, 0, { screen_texture_uniform });

    PackedByteArray push_const;
    push_const.resize(16);
    int *push_const_ptr = (int *)push_const.ptrw();
    push_const_ptr[0] = p_src_pos.x;
    push_const_ptr[1] = p_src_pos.y;

    int draw_list = rd->draw_list_begin(p_dst, RD::DRAW_DEFAULT_ALL, {}, 1.0f, 0, Rect2(p_dst_pos.x, p_dst_pos.y, p_size.x, p_size.y));

    rd->draw_list_bind_render_pipeline(draw_list, pipeline_blit);
    rd->draw_list_bind_uniform_set(draw_list, uniform_set, 0);
    rd->draw_list_set_push_constant(draw_list, push_const, push_const.size());

    rd->draw_list_draw(draw_list, false, 1, 3);
    rd->draw_list_end();
}

void RenderInterfaceGodot::free_render_frame(RenderFrame *p_frame) {
    RenderingServer *rs = RenderingServer::get_singleton();

    free_render_target(&p_frame->main_target);
    free_render_target(&p_frame->primary_filter_target);
    free_render_target(&p_frame->secondary_filter_target);
    free_render_target(&p_frame->blend_mask_target);

    if (p_frame->clip_mask_framebuffer.is_valid()) {
        rendering_resources.free_framebuffer(p_frame->clip_mask_framebuffer);
    }
    if (p_frame->clip_mask.is_valid()) {
        rendering_resources.free_texture(p_frame->clip_mask);
    }
}

void RenderInterfaceGodot::set_render_frame(RenderFrame *p_frame) {
    // Cannot delete render stack at pointer = 0
    for (int i = render_target_stack.size() - 1; i > 0; i--) {
        free_render_target(render_target_stack[i]);
        memdelete(render_target_stack[i]);
    }

    render_target_stack.clear();
    render_target_stack_ptr = 0;
    target_frame = p_frame;
    if (p_frame == nullptr) return;

    allocate_render_frame();

    render_target_stack.push_back(&p_frame->main_target);
    RD *rd = rendering_resources.device();
    rd->texture_clear(p_frame->main_target.color0, Color(0, 0, 0, 0), 0, 1, 0, 1);
    rd->texture_clear(p_frame->main_target.color1, Color(0, 0, 0, 0), 0, 1, 0, 1);
}

Rml::CompiledGeometryHandle RenderInterfaceGodot::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
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

void RenderInterfaceGodot::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
    ERR_FAIL_COND(render_target_stack.size() == 0);
    RenderTarget *target = render_target_stack[render_target_stack_ptr];

    MeshData *mesh_data = reinterpret_cast<MeshData *>(geometry);
    ERR_FAIL_NULL(mesh_data);
    
    RD *rd = rendering_resources.device();

    Ref<RDUniform> screen_texture_uniform = memnew(RDUniform);
    screen_texture_uniform->set_uniform_type(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
    screen_texture_uniform->set_binding(0);
    screen_texture_uniform->add_id(sampler_nearest);
    screen_texture_uniform->add_id(target->color0);
    
    Ref<RDUniform> texture_uniform = memnew(RDUniform);
    texture_uniform->set_uniform_type(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
    texture_uniform->set_binding(1);
    texture_uniform->add_id(sampler_linear);
    if (texture == 0) {
        texture_uniform->add_id(texture_white);
    } else {
        TextureData *tex_data = reinterpret_cast<TextureData *>(texture);
        texture_uniform->add_id(tex_data->rid);
    }

    RID uniform_set = UniformSetCacheRD::get_cache(shader_geometry, 0, { screen_texture_uniform, texture_uniform });

    Rml::Matrix4f final_transform = get_final_transform(drawing_matrix, translation);

    PackedByteArray push_const;
    push_const.resize(80);
    float *push_const_ptr = (float *)push_const.ptrw();
    push_const_ptr[0] = 1.0 / target_frame->current_size.x;
    push_const_ptr[1] = 1.0 / target_frame->current_size.y;
    matrix_to_pointer(push_const_ptr + 4, final_transform);

    int draw_list = rd->draw_list_begin(target->framebuffer1, RD::DRAW_DEFAULT_ALL);
    if (scissor_enabled) {
        rd->draw_list_enable_scissor(draw_list, scissor_region);
    } else {
        RenderingServer *rs = RenderingServer::get_singleton();
        RD *rd = rs->get_rendering_device();
        rd->draw_list_disable_scissor(draw_list);
    }
    RID pipeline = clip_mask_enabled ? pipeline_geometry_clipping : pipeline_geometry;
    rd->draw_list_bind_render_pipeline(draw_list, pipeline);
    rd->draw_list_bind_vertex_array(draw_list, mesh_data->vertex_array);
    rd->draw_list_bind_index_array(draw_list, mesh_data->index_array);
    rd->draw_list_bind_uniform_set(draw_list, uniform_set, 0);
    rd->draw_list_set_push_constant(draw_list, push_const, push_const.size());

    rd->draw_list_draw(draw_list, true, 1);
    rd->draw_list_end();

    blit_texture(target->framebuffer0, target->color1, Vector2i(), Vector2i(), target_frame->current_size);
}

void RenderInterfaceGodot::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
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

Rml::TextureHandle RenderInterfaceGodot::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
    ResourceLoader *rl = ResourceLoader::get_singleton();

    String source_str = String(source.c_str());

    Ref<Texture> tex = rl->load(source_str);
    if (!tex.is_valid()) {
        return 0;
    }

    RenderingServer *rs = RenderingServer::get_singleton();

    TextureData *tex_data = memnew(TextureData());
    tex_data->tex_ref = tex;
    tex_data->rid = rs->texture_get_rd_texture(tex->get_rid());

    return reinterpret_cast<uintptr_t>(tex_data);
}

Rml::TextureHandle RenderInterfaceGodot::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) {
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

void RenderInterfaceGodot::ReleaseTexture(Rml::TextureHandle texture) {
    TextureData *tex_data = reinterpret_cast<TextureData *>(texture);

    // Is a generated texture
    if (!tex_data->tex_ref.is_valid()) {
        rendering_resources.free_texture(tex_data->rid);
    }
    memdelete(tex_data);
}

void RenderInterfaceGodot::EnableScissorRegion(bool enable) {
    scissor_enabled = enable;
}

void RenderInterfaceGodot::SetScissorRegion(Rml::Rectanglei region) {
    scissor_region = Rect2(
        region.Position().x,
        region.Position().y,
        region.Size().x,
        region.Size().y
    );
}

void RenderInterfaceGodot::EnableClipMask(bool enable) {
	clip_mask_enabled = enable;
}

void RenderInterfaceGodot::RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) {
    ERR_FAIL_COND(render_target_stack.size() == 0);
    RenderTarget *target = render_target_stack[render_target_stack_ptr];

    MeshData *mesh_data = reinterpret_cast<MeshData *>(geometry);
    ERR_FAIL_NULL(mesh_data);
    
    RD *rd = rendering_resources.device();

    Rml::Matrix4f final_transform = get_final_transform(drawing_matrix, translation);

    PackedByteArray push_const;
    push_const.resize(80);
    float *push_const_ptr = (float *)push_const.ptrw();
    push_const_ptr[0] = 1.0 / target_frame->current_size.x;
    push_const_ptr[1] = 1.0 / target_frame->current_size.y;
    matrix_to_pointer(push_const_ptr + 4, final_transform);
    
    uint64_t clear_flags = RD::DRAW_CLEAR_STENCIL;
    uint32_t clear_stencil_value = 0;
    if (operation == Rml::ClipMaskOperation::SetInverse) {
        clear_stencil_value = 1;
    }
    if (operation == Rml::ClipMaskOperation::Intersect) {
        clear_flags = RD::DRAW_DEFAULT_ALL;
    }

    int draw_list = rd->draw_list_begin(target_frame->clip_mask_framebuffer, clear_flags, {}, 1.0f, clear_stencil_value);

    if (scissor_enabled) {
        rd->draw_list_enable_scissor(draw_list, scissor_region);
    } else {
        RenderingServer *rs = RenderingServer::get_singleton();
        RD *rd = rs->get_rendering_device();
        rd->draw_list_disable_scissor(draw_list);
    }

    RID pipeline;
    switch (operation) {
        case Rml::ClipMaskOperation::Set: {
            pipeline = pipeline_clip_mask_set;
        } break;
        case Rml::ClipMaskOperation::SetInverse: {
            pipeline = pipeline_clip_mask_set_inverse;
        } break;
        case Rml::ClipMaskOperation::Intersect: {
            pipeline = pipeline_clip_mask_intersect;
        } break;
    }

    rd->draw_list_bind_render_pipeline(draw_list, pipeline);
    rd->draw_list_bind_vertex_array(draw_list, mesh_data->vertex_array);
    rd->draw_list_bind_index_array(draw_list, mesh_data->index_array);
    rd->draw_list_set_push_constant(draw_list, push_const, push_const.size());

    rd->draw_list_draw(draw_list, true, 1);
    rd->draw_list_end();
}

void RenderInterfaceGodot::SetTransform(const Rml::Matrix4f* transform) {
    if (transform == nullptr) {
        drawing_matrix = Rml::Matrix4f::Identity();
        return;
    }
    drawing_matrix = *transform;
}

Rml::LayerHandle RenderInterfaceGodot::PushLayer() {
    render_target_stack_ptr++;

    RenderTarget *target;
    if (render_target_stack_ptr == render_target_stack.size()) {
        target = memnew(RenderTarget);
        allocate_render_target(target);
        render_target_stack.push_back(target);
    } else {
        target = render_target_stack[render_target_stack_ptr];
    }

    RD *rd = rendering_resources.device();
    rd->texture_clear(target->color0, Color(0, 0, 0, 0), 0, 1, 0, 1);
    rd->texture_clear(target->color1, Color(0, 0, 0, 0), 0, 1, 0, 1);

    return render_target_stack_ptr;
}

void RenderInterfaceGodot::RenderFilter(Rml::CompiledFilterHandle filter) {
    ShaderRenderData *shader_data = reinterpret_cast<ShaderRenderData *>(filter);

    RD *rd = rendering_resources.device();

    rd->draw_command_begin_label(vformat("Rendering filter %s", shader_data->name), Color(1, 1, 1));

    for (int i = 0; i < shader_data->passes.size(); i++) {
        const ShaderRenderData::Pass &pass = shader_data->passes[i];

        TypedArray<Ref<RDUniform>> uniforms;

        RID *src_tex0, *src_tex1;
        src_tex0 = filter_get_texture(target_frame, pass.src0);
        src_tex1 = filter_get_texture(target_frame, pass.src1);
        
        Ref<RDUniform> src0 = memnew(RDUniform);
        src0->set_uniform_type(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
        src0->set_binding(0);
        src0->add_id(sampler_nearest);
        src0->add_id(*src_tex0);
        uniforms.append(src0);

        if (src_tex1 != nullptr) {
            Ref<RDUniform> src1 = memnew(RDUniform);
            src1->set_uniform_type(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
            src1->set_binding(1);
            src1->add_id(sampler_nearest);
            src1->add_id(*src_tex1);
            uniforms.append(src1);
        }

        RID *framebuffer = filter_get_framebuffer(target_frame, pass.dst);

        RID uniform_set = UniformSetCacheRD::get_cache(pass.shader, 0, uniforms);

        int draw_list = rd->draw_list_begin(*framebuffer, RD::DRAW_CLEAR_ALL, {Color(0, 0, 0, 0)});

        RID pipeline = get_shader_pipeline(pass.pipeline_id);
        rd->draw_list_bind_render_pipeline(draw_list, pipeline);
        rd->draw_list_bind_uniform_set(draw_list, uniform_set, 0);
        if (!pass.push_const.is_empty()) {
            rd->draw_list_set_push_constant(draw_list, pass.push_const, pass.push_const.size());
        }

        rd->draw_list_draw(draw_list, false, 1, 3);

        rd->draw_list_end();

        RID *swap0 = filter_get_texture(target_frame, pass.swap0);
        RID *swap1 = filter_get_texture(target_frame, pass.swap1);
        RID *swap2 = filter_get_framebuffer(target_frame, pass.swap0);
        RID *swap3 = filter_get_framebuffer(target_frame, pass.swap1);
        if (swap0 != nullptr && swap1 != nullptr) {
            std::swap(*swap0, *swap1);
            std::swap(*swap2, *swap3);
        }
    }

    rd->draw_command_end_label();
}

void RenderInterfaceGodot::CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination, Rml::BlendMode blend_mode, Rml::Span<const Rml::CompiledFilterHandle> filters) {
    RenderTarget *source_target = render_target_stack[source];
    RenderTarget *destination_target = render_target_stack[destination];

    blit_texture(target_frame->primary_filter_target.framebuffer0, source_target->color0, Vector2i(), Vector2i(), target_frame->current_size);
    for (auto it : filters) {
        RenderFilter(it);
    }

    Ref<RDUniform> screen_texture_uniform = memnew(RDUniform);
    screen_texture_uniform->set_uniform_type(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
    screen_texture_uniform->set_binding(0);
    screen_texture_uniform->add_id(sampler_nearest);
    screen_texture_uniform->add_id(destination_target->color0);

    Ref<RDUniform> src_screen_texture_uniform = memnew(RDUniform);
    src_screen_texture_uniform->set_uniform_type(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
    src_screen_texture_uniform->set_binding(1);
    src_screen_texture_uniform->add_id(sampler_nearest);
    src_screen_texture_uniform->add_id(target_frame->primary_filter_target.color0);

    RID uniform_set = UniformSetCacheRD::get_cache(shader_layer_composition, 0, { screen_texture_uniform, src_screen_texture_uniform });

    PackedByteArray push_const;
    push_const.resize(16);
    unsigned int *push_const_ptr = (unsigned int *)push_const.ptrw();
    push_const_ptr[0] = (unsigned int)blend_mode;

    RD *rd = internal_rendering_resources.device();

    int draw_list = rd->draw_list_begin(destination_target->framebuffer1, RD::DRAW_DEFAULT_ALL);

    rd->draw_list_bind_render_pipeline(draw_list, pipeline_layer_composition);
    rd->draw_list_bind_uniform_set(draw_list, uniform_set, 0);
    rd->draw_list_set_push_constant(draw_list, push_const, push_const.size());

    rd->draw_list_draw(draw_list, false, 1, 3);
    rd->draw_list_end();

    blit_texture(destination_target->framebuffer0, destination_target->color1, Vector2i(), Vector2i(), target_frame->current_size);
}

void RenderInterfaceGodot::PopLayer() {
    render_target_stack_ptr--;
}

Rml::TextureHandle RenderInterfaceGodot::SaveLayerAsTexture() {
    RenderTarget *target = render_target_stack[render_target_stack_ptr];

    TextureData *tex_data = memnew(TextureData());
    Vector2i off = scissor_region.position;
    Vector2i size = scissor_region.size;
    tex_data->rid = rendering_resources.create_texture({
        {"width", size.x},
        {"height", size.y},
        {"format", RD::DATA_FORMAT_R8G8B8A8_UNORM},
        {"usage_bits", RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT}
    });

    RD *rd = rendering_resources.device();
    RID fb = rd->framebuffer_create({ tex_data->rid });

    blit_texture(fb, target->color0, Vector2i(), off, size);

    rd->free_rid(fb);

    return reinterpret_cast<uintptr_t>(tex_data);
}

Rml::CompiledFilterHandle RenderInterfaceGodot::SaveLayerAsMaskImage() {
    RenderTarget *target = render_target_stack[render_target_stack_ptr];

    ShaderRenderData params;
    params.name = "mask";

    blit_texture(target_frame->blend_mask_target.framebuffer0, target->color0, Vector2i(), Vector2i(), target_frame->current_size);

    ShaderRenderData::Pass pass;
    pass.shader = shaders[SHADER_FILTER_MASK];
    pass.pipeline_id = PIPELINE_FILTER_MASK;
    pass.src0 = BufferTarget::PRIMARY_BUFFER0;
    pass.src1 = BufferTarget::BLEND_BUFFER0;

    params.passes.push_back(pass);

    ShaderRenderData *shader_data = memnew(ShaderRenderData(std::move(params)));
    return reinterpret_cast<uintptr_t>(shader_data);
}

Rml::CompiledFilterHandle RenderInterfaceGodot::CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) {
    ShaderRenderData params;
    params.name = rml_to_godot_string(name);

    if (name == "opacity") {
        ShaderRenderData::Pass pass;
        pass.shader = shaders[SHADER_FILTER_MODULATE];
        pass.pipeline_id = PIPELINE_FILTER_MODULATE;

        const float value = Rml::Get(parameters, "value", 1.f);

        pass.push_const = PackedByteArray();
        pass.push_const.resize(32);

        float *push_const_ptr = (float *)pass.push_const.ptrw();
        push_const_ptr[0] = 1.0f;
        push_const_ptr[1] = 1.0f;
        push_const_ptr[2] = 1.0f;
        push_const_ptr[3] = value;
        push_const_ptr[4] = 0;

        params.passes.push_back(pass);
    } else if (name == "blur") {
        ShaderRenderData::Pass pass;
        pass.shader = shaders[SHADER_FILTER_BLUR];
        pass.pipeline_id = PIPELINE_FILTER_BLUR;

        const float sigma = Rml::Get(parameters, "sigma", 0.f);
        
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
        ShaderRenderData::Pass pass;
        pass.shader = shaders[SHADER_FILTER_BLUR];
        pass.pipeline_id = PIPELINE_FILTER_BLUR;
        pass.src0 = BufferTarget::PRIMARY_BUFFER0;
        pass.dst = BufferTarget::SECONDARY_BUFFER1;
        pass.swap0 = BufferTarget::SECONDARY_BUFFER0;
        pass.swap1 = BufferTarget::SECONDARY_BUFFER1;

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
        pass.src0 = BufferTarget::SECONDARY_BUFFER0;
        pass.dst = BufferTarget::SECONDARY_BUFFER1;
        pass.swap0 = BufferTarget::SECONDARY_BUFFER0;
        pass.swap1 = BufferTarget::SECONDARY_BUFFER1;

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
        pass.shader = shaders[SHADER_FILTER_DROP_SHADOW];
        pass.pipeline_id = PIPELINE_FILTER_DROP_SHADOW;
        pass.src0 = BufferTarget::PRIMARY_BUFFER0;
        pass.src1 = BufferTarget::SECONDARY_BUFFER0;
        pass.dst = BufferTarget::PRIMARY_BUFFER1;
        pass.swap0 = BufferTarget::PRIMARY_BUFFER0;
        pass.swap1 = BufferTarget::PRIMARY_BUFFER1;

        pass.push_const = PackedByteArray();
        pass.push_const.resize(16);
        push_const_ptr = (float *)pass.push_const.ptrw();
        push_const_ptr[0] = color.red / 255.0;
        push_const_ptr[1] = color.green / 255.0;
        push_const_ptr[2] = color.blue / 255.0;
        push_const_ptr[3] = color.alpha / 255.0;

        params.passes.push_back(pass);
    } else if (name == "brightness") {
        ShaderRenderData::Pass pass;
        pass.shader = shaders[SHADER_FILTER_MODULATE];
        pass.pipeline_id = PIPELINE_FILTER_MODULATE;

        const float value = Rml::Get(parameters, "value", 1.f);

        pass.push_const = PackedByteArray();
        pass.push_const.resize(32);
        
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        push_const_ptr[0] = value;
        push_const_ptr[1] = value;
        push_const_ptr[2] = value;
        push_const_ptr[3] = 1.0f;
        push_const_ptr[4] = 0;

        params.passes.push_back(pass);
    } else if (name == "contrast") {
        ShaderRenderData::Pass pass;
        pass.shader = shaders[SHADER_FILTER_COLOR_MATRIX];
        pass.pipeline_id = PIPELINE_FILTER_COLOR_MATRIX;

        const float value = Rml::Get(parameters, "value", 1.f);
        const float gray = 0.5f - 0.5f * value;

        Rml::Matrix4f color_matrix = Rml::Matrix4f::Diag(value, value, value, 1.0f);
        color_matrix.SetColumn(3, Rml::Vector4f(gray, gray, gray, 1.0f));

        pass.push_const = PackedByteArray();
        pass.push_const.resize(64);
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        matrix_to_pointer(push_const_ptr, color_matrix);

        params.passes.push_back(pass);
    } else if (name == "invert") {
        ShaderRenderData::Pass pass;
        pass.shader = shaders[SHADER_FILTER_COLOR_MATRIX];
        pass.pipeline_id = PIPELINE_FILTER_COLOR_MATRIX;

        const float value = Rml::Get(parameters, "value", 0.f);
        const float inverted = 1.f - 2.f * value;

        Rml::Matrix4f color_matrix = Rml::Matrix4f::Diag(inverted, inverted, inverted, 1.f);
        color_matrix.SetColumn(3, Rml::Vector4f(value, value, value, 1.f));

        pass.push_const = PackedByteArray();
        pass.push_const.resize(64);
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        matrix_to_pointer(push_const_ptr, color_matrix);

        params.passes.push_back(pass);
    } else if (name == "grayscale") {
        ShaderRenderData::Pass pass;
        pass.shader = shaders[SHADER_FILTER_COLOR_MATRIX];
        pass.pipeline_id = PIPELINE_FILTER_COLOR_MATRIX;

        const float value = Rml::Get(parameters, "value", 1.f);
        const float rev_value = 1.f - value;
		const Rml::Vector3f gray = value * Rml::Vector3f(0.2126f, 0.7152f, 0.0722f);

        Rml::Matrix4f color_matrix = Rml::Matrix4f::FromRows(
			{gray.x + rev_value, gray.y,             gray.z,             0.f},
			{gray.x,             gray.y + rev_value, gray.z,             0.f},
			{gray.x,             gray.y,             gray.z + rev_value, 0.f},
			{0.f,                0.f,                0.f,                1.f}
		);

        pass.push_const = PackedByteArray();
        pass.push_const.resize(64);
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        matrix_to_pointer(push_const_ptr, color_matrix);

        params.passes.push_back(pass);
    } else if (name == "sepia") {
        ShaderRenderData::Pass pass;
        pass.shader = shaders[SHADER_FILTER_COLOR_MATRIX];
        pass.pipeline_id = PIPELINE_FILTER_COLOR_MATRIX;

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

        pass.push_const = PackedByteArray();
        pass.push_const.resize(64);
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        matrix_to_pointer(push_const_ptr, color_matrix);

        params.passes.push_back(pass);
    } else if (name == "hue-rotate") {
        ShaderRenderData::Pass pass;
        pass.shader = shaders[SHADER_FILTER_MODULATE];
        pass.pipeline_id = PIPELINE_FILTER_MODULATE;

        const float value = Rml::Get(parameters, "value", 0.f);

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
    } else if (name == "saturate") {
        ShaderRenderData::Pass pass;
        pass.shader = shaders[SHADER_FILTER_COLOR_MATRIX];
        pass.pipeline_id = PIPELINE_FILTER_COLOR_MATRIX;

        const float value = Rml::Get(parameters, "value", 1.f);

		Rml::Matrix4f color_matrix = Rml::Matrix4f::FromRows(
			{0.213f + 0.787f * value,  0.715f - 0.715f * value,  0.072f - 0.072f * value,  0.f},
			{0.213f - 0.213f * value,  0.715f + 0.285f * value,  0.072f - 0.072f * value,  0.f},
			{0.213f - 0.213f * value,  0.715f - 0.715f * value,  0.072f + 0.928f * value,  0.f},
			{0.f,                      0.f,                      0.f,                      1.f}
		);

        pass.push_const = PackedByteArray();
        pass.push_const.resize(64);
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        matrix_to_pointer(push_const_ptr, color_matrix);

        params.passes.push_back(pass);
    }

    ShaderRenderData *shader_data = memnew(ShaderRenderData(std::move(params)));
    return reinterpret_cast<uintptr_t>(shader_data);
}

void RenderInterfaceGodot::ReleaseFilter(Rml::CompiledFilterHandle filter) {
    ShaderRenderData *shader_data = reinterpret_cast<ShaderRenderData *>(filter);

    memdelete(shader_data);
}

Rml::CompiledShaderHandle RenderInterfaceGodot::CompileShader(const Rml::String& name, const Rml::Dictionary& parameters) {
    ShaderRenderData params;
    params.name = rml_to_godot_string(name);

    RD *rd = rendering_resources.device();

    if (name == "linear-gradient" || name == "radial-gradient" || name == "conic-gradient") {
        ShaderRenderData::Pass pass;
        pass.shader = shaders[SHADER_GRADIENT];
        pass.pipeline_id = PIPELINE_GRADIENT;
        pass.src0 = BufferTarget::NONE;
        pass.src1 = BufferTarget::NONE;
        pass.dst = BufferTarget::PRIMARY_BUFFER1;
        pass.swap0 = BufferTarget::PRIMARY_BUFFER0;
        pass.swap1 = BufferTarget::PRIMARY_BUFFER1;

        const bool repeating = Rml::Get(parameters, "repeating", false);
        const Rml::ColorStopList stop_list = Rml::Get(parameters, "color_stop_list", Rml::ColorStopList());
        Rml::Vector2f p, v;
        uint grad_type;

        if (name == "linear-gradient") {
            grad_type = 1;
            p = Rml::Get(parameters, "p0", Rml::Vector2f());
            v = Rml::Get(parameters, "p1", Rml::Vector2f(0.f)) - p;
        }

        int stop_count = stop_list.size();

        pass.push_const = PackedByteArray();
        pass.push_const.resize(112);
        float *push_const_ptr = (float *)pass.push_const.ptrw();
        unsigned int *push_const_uint_ptr = (unsigned int *)pass.push_const.ptrw();
        push_const_uint_ptr[20] = (grad_type << 1) | (repeating ? 1 : 0);
        push_const_uint_ptr[21] = stop_count;
        push_const_ptr[22] = p.x;
        push_const_ptr[23] = p.y;
        push_const_ptr[24] = v.x;
        push_const_ptr[25] = v.y;

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

        pass.buffer = rendering_resources.create_storage_buffer({
            {"data", storage_buffer}
        });

        params.passes.push_back(pass);
    }

    ShaderRenderData *shader_data = memnew(ShaderRenderData(std::move(params)));
    return reinterpret_cast<uintptr_t>(shader_data);
}

void RenderInterfaceGodot::RenderShader(Rml::CompiledShaderHandle shader, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
    ERR_FAIL_COND(render_target_stack.size() == 0);
    RenderTarget *target = render_target_stack[render_target_stack_ptr];

    MeshData *mesh_data = reinterpret_cast<MeshData *>(geometry);
    ERR_FAIL_NULL(mesh_data);

    ShaderRenderData *shader_data = reinterpret_cast<ShaderRenderData *>(shader);
    
    RD *rd = rendering_resources.device();

    Rml::Matrix4f final_transform = get_final_transform(drawing_matrix, translation);

    for (int i = 0; i < shader_data->passes.size(); i++) {
        ShaderRenderData::Pass &pass = shader_data->passes[i];

        TypedArray<Ref<RDUniform>> uniforms;

        RID *src_tex0, *src_tex1;
        src_tex0 = filter_get_texture(target_frame, pass.src0);
        src_tex1 = filter_get_texture(target_frame, pass.src1);
        
        int uniform_binding = 0;
        if (src_tex0 != nullptr) {
            Ref<RDUniform> src0 = memnew(RDUniform);
            src0->set_uniform_type(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
            src0->set_binding(uniform_binding++);
            src0->add_id(sampler_nearest);
            src0->add_id(*src_tex0);
            uniforms.append(src0);
        }

        if (src_tex1 != nullptr) {
            Ref<RDUniform> src1 = memnew(RDUniform);
            src1->set_uniform_type(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
            src1->set_binding(uniform_binding++);
            src1->add_id(sampler_nearest);
            src1->add_id(*src_tex1);
            uniforms.append(src1);
        }

        if (pass.buffer.is_valid()) {
            Ref<RDUniform> buf = memnew(RDUniform);
            buf->set_uniform_type(RD::UNIFORM_TYPE_STORAGE_BUFFER);
            buf->set_binding(uniform_binding++);
            buf->add_id(pass.buffer);
            uniforms.append(buf);
        }

        RID *framebuffer = filter_get_framebuffer(target_frame, pass.dst);

        RID uniform_set = UniformSetCacheRD::get_cache(pass.shader, 0, uniforms);

        float *push_const_ptr = (float *)pass.push_const.ptrw();
        push_const_ptr[0] = 1.0 / target_frame->current_size.x;
        push_const_ptr[1] = 1.0 / target_frame->current_size.y;
        matrix_to_pointer(push_const_ptr + 4, final_transform);

        int draw_list = rd->draw_list_begin(target->framebuffer1, RD::DRAW_DEFAULT_ALL);
        if (scissor_enabled) {
            rd->draw_list_enable_scissor(draw_list, scissor_region);
        } else {
            RenderingServer *rs = RenderingServer::get_singleton();
            RD *rd = rs->get_rendering_device();
            rd->draw_list_disable_scissor(draw_list);
        }

        RID pipeline = get_shader_pipeline(pass.pipeline_id);
        rd->draw_list_bind_render_pipeline(draw_list, pipeline);
        rd->draw_list_bind_vertex_array(draw_list, mesh_data->vertex_array);
        rd->draw_list_bind_index_array(draw_list, mesh_data->index_array);
        rd->draw_list_bind_uniform_set(draw_list, uniform_set, 0);
        rd->draw_list_set_push_constant(draw_list, pass.push_const, pass.push_const.size());

        rd->draw_list_draw(draw_list, true, 1);
        rd->draw_list_end();

        RID *swap0 = filter_get_texture(target_frame, pass.swap0);
        RID *swap1 = filter_get_texture(target_frame, pass.swap1);
        RID *swap2 = filter_get_framebuffer(target_frame, pass.swap0);
        RID *swap3 = filter_get_framebuffer(target_frame, pass.swap1);
        if (swap0 != nullptr && swap1 != nullptr) {
            std::swap(*swap0, *swap1);
            std::swap(*swap2, *swap3);
        }
    }
}

void RenderInterfaceGodot::ReleaseShader(Rml::CompiledShaderHandle shader) {
    ShaderRenderData *shader_data = reinterpret_cast<ShaderRenderData *>(shader);

    memdelete(shader_data);
}

RenderInterfaceGodot::RenderInterfaceGodot() {
    singleton = this;
}
RenderInterfaceGodot::~RenderInterfaceGodot() {
    singleton = nullptr;
}
