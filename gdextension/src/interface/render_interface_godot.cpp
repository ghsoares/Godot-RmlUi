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

#include <iostream>

#include "../util.h"

using namespace godot;

using RD = RenderingDevice;

RenderInterfaceGodot *RenderInterfaceGodot::singleton = nullptr;

RenderInterfaceGodot *RenderInterfaceGodot::get_singleton() {
    return singleton;
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
    Ref<RDAttachmentFormat> alpha_attachment = memnew(RDAttachmentFormat);
    Ref<RDAttachmentFormat> clip_mask_attachment = memnew(RDAttachmentFormat);

    uint64_t main_usage = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;

    color_attachment->set_format(RD::DATA_FORMAT_R8G8B8A8_UNORM);
    color_attachment->set_usage_flags(main_usage);
    
    alpha_attachment->set_format(RD::DATA_FORMAT_R8_UINT);
    alpha_attachment->set_usage_flags(main_usage);

    clip_mask_attachment->set_format(RD::DATA_FORMAT_S8_UINT);
    clip_mask_attachment->set_usage_flags(RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT);

    geometry_framebuffer_format = rd->framebuffer_format_create({ color_attachment, alpha_attachment, clip_mask_attachment });
    clip_mask_framebuffer_format = rd->framebuffer_format_create({ clip_mask_attachment });
    geometry_vertex_format = rd->vertex_format_create({ pos_attr, color_attr, uv_attr });

    shader_geometry = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/geometry.glsl"},
        {"name", "__render_interface_geometry_shader"}
    });
    shader_clip_mask = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/clip_mask.glsl"},
        {"name", "__render_interface_clip_mask_shader"}
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

    sampler_nearest = internal_rendering_resources.create_sampler({});
    sampler_linear = internal_rendering_resources.create_sampler({
        {"min_filter", RD::SAMPLER_FILTER_LINEAR},
        {"mag_filter", RD::SAMPLER_FILTER_LINEAR}
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
    p_target->alpha_mask0 = rendering_resources.create_texture({
        {"width", target_frame->current_size.x},
        {"height", target_frame->current_size.y},
        {"format", RD::DATA_FORMAT_R8_UINT},
        {"usage_bits", main_usage},
    });
    p_target->alpha_mask1 = rendering_resources.create_texture({
        {"width", target_frame->current_size.x},
        {"height", target_frame->current_size.y},
        {"format", RD::DATA_FORMAT_R8_UINT},
        {"usage_bits", main_usage},
    });

    p_target->framebuffer0 = rendering_resources.create_framebuffer({
        {"textures", TypedArray<RID>({ p_target->color0, p_target->alpha_mask0, target_frame->clip_mask })}
    });
    p_target->framebuffer1 = rendering_resources.create_framebuffer({
        {"textures", TypedArray<RID>({ p_target->color1, p_target->alpha_mask1, target_frame->clip_mask })}
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
    if (p_target->alpha_mask0.is_valid()) {
        rendering_resources.free_texture(p_target->alpha_mask0);
    }
    if (p_target->alpha_mask1.is_valid()) {
        rendering_resources.free_texture(p_target->alpha_mask1);
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
    target_frame->main_tex = rs->texture_rd_create(target_frame->main_target.color0);
}

void RenderInterfaceGodot::free_render_frame(RenderFrame *p_frame) {
    RenderingServer *rs = RenderingServer::get_singleton();

    free_render_target(&p_frame->main_target);

    if (p_frame->clip_mask_framebuffer.is_valid()) {
        rendering_resources.free_framebuffer(p_frame->clip_mask_framebuffer);
    }
    if (p_frame->clip_mask.is_valid()) {
        rendering_resources.free_texture(p_frame->clip_mask);
    }
}

void RenderInterfaceGodot::set_render_frame(RenderFrame *p_frame) {
    render_target_stack.clear();
    target_frame = p_frame;
    if (p_frame == nullptr) return;

    allocate_render_frame();

    render_target_stack.push_back(&p_frame->main_target);
    RD *rd = rendering_resources.device();
    rd->texture_clear(p_frame->main_target.color0, Color(0, 0, 0, 0), 0, 1, 0, 1);
    rd->texture_clear(p_frame->main_target.color1, Color(0, 0, 0, 0), 0, 1, 0, 1);
    rd->texture_clear(p_frame->main_target.alpha_mask0, Color(0, 0, 0, 0), 0, 1, 0, 1);
    rd->texture_clear(p_frame->main_target.alpha_mask1, Color(0, 0, 0, 0), 0, 1, 0, 1);
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
    ERR_FAIL_COND(!shader_geometry.is_valid() || !pipeline_geometry.is_valid());
    ERR_FAIL_COND(render_target_stack.size() == 0);
    RenderTarget *target = render_target_stack.back();

    MeshData *mesh_data = reinterpret_cast<MeshData *>(geometry);
    ERR_FAIL_NULL(mesh_data);
    
    RD *rd = rendering_resources.device();

    Ref<RDUniform> screen_texture_uniform = memnew(RDUniform);
    screen_texture_uniform->set_uniform_type(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
    screen_texture_uniform->set_binding(0);
    screen_texture_uniform->add_id(sampler_nearest);
    screen_texture_uniform->add_id(target->color0);
    
    Ref<RDUniform> screen_alpha_uniform = memnew(RDUniform);
    screen_alpha_uniform->set_uniform_type(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
    screen_alpha_uniform->set_binding(1);
    screen_alpha_uniform->add_id(sampler_nearest);
    screen_alpha_uniform->add_id(target->alpha_mask0);
    
    Ref<RDUniform> texture_uniform = memnew(RDUniform);
    texture_uniform->set_uniform_type(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
    texture_uniform->set_binding(2);
    texture_uniform->add_id(sampler_linear);
    if (texture == 0) {
        texture_uniform->add_id(texture_white);
    } else {
        TextureData *tex_data = reinterpret_cast<TextureData *>(texture);
        texture_uniform->add_id(tex_data->rid);
    }

    RID uniform_set = UniformSetCacheRD::get_cache(shader_geometry, 0, { screen_texture_uniform, screen_alpha_uniform, texture_uniform });

    Projection final_transform = drawing_matrix * Projection(Transform3D(Basis(), Vector3(translation.x, translation.y, 0.0)));

    PackedByteArray push_const;
    push_const.resize(96);
    float *push_const_ptr = (float *)push_const.ptrw();
    push_const_ptr[0] = 1.0 / target_frame->current_size.x;
    push_const_ptr[1] = 1.0 / target_frame->current_size.y;

    push_const_ptr[4] = final_transform.columns[0].x;
    push_const_ptr[5] = final_transform.columns[0].y;
    push_const_ptr[6] = final_transform.columns[0].z;
    push_const_ptr[7] = final_transform.columns[0].w;

    push_const_ptr[8] = final_transform.columns[1].x;
    push_const_ptr[9] = final_transform.columns[1].y;
    push_const_ptr[10] = final_transform.columns[1].z;
    push_const_ptr[11] = final_transform.columns[1].w;

    push_const_ptr[12] = final_transform.columns[2].x;
    push_const_ptr[13] = final_transform.columns[2].y;
    push_const_ptr[14] = final_transform.columns[2].z;
    push_const_ptr[15] = final_transform.columns[2].w;

    push_const_ptr[16] = final_transform.columns[3].x;
    push_const_ptr[17] = final_transform.columns[3].y;
    push_const_ptr[18] = final_transform.columns[3].z;
    push_const_ptr[19] = final_transform.columns[3].w;

    unsigned int *push_const_int_ptr = (unsigned int *)push_const.ptrw();
    push_const_int_ptr[20] = clip_mask_enabled ? 1 : 0;

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

    rd->texture_copy(target->color1, target->color0, Vector3(), Vector3(), Vector3(target_frame->current_size.x, target_frame->current_size.y, 1), 0, 0, 0, 0);
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
	ERR_FAIL_COND(!shader_clip_mask.is_valid());
    ERR_FAIL_COND(render_target_stack.size() == 0);
    RenderTarget *target = render_target_stack.back();

    MeshData *mesh_data = reinterpret_cast<MeshData *>(geometry);
    ERR_FAIL_NULL(mesh_data);
    
    RD *rd = rendering_resources.device();

    Projection final_transform = drawing_matrix * Projection(Transform3D(Basis(), Vector3(translation.x, translation.y, 0.0)));

    PackedByteArray push_const;
    push_const.resize(80);
    float *push_const_ptr = (float *)push_const.ptrw();
    push_const_ptr[0] = 1.0 / target_frame->current_size.x;
    push_const_ptr[1] = 1.0 / target_frame->current_size.y;

    push_const_ptr[4] = final_transform.columns[0].x;
    push_const_ptr[5] = final_transform.columns[0].y;
    push_const_ptr[6] = final_transform.columns[0].z;
    push_const_ptr[7] = final_transform.columns[0].w;

    push_const_ptr[8] = final_transform.columns[1].x;
    push_const_ptr[9] = final_transform.columns[1].y;
    push_const_ptr[10] = final_transform.columns[1].z;
    push_const_ptr[11] = final_transform.columns[1].w;

    push_const_ptr[12] = final_transform.columns[2].x;
    push_const_ptr[13] = final_transform.columns[2].y;
    push_const_ptr[14] = final_transform.columns[2].z;
    push_const_ptr[15] = final_transform.columns[2].w;

    push_const_ptr[16] = final_transform.columns[3].x;
    push_const_ptr[17] = final_transform.columns[3].y;
    push_const_ptr[18] = final_transform.columns[3].z;
    push_const_ptr[19] = final_transform.columns[3].w;
    
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
        drawing_matrix = Projection();
        return;
    }
    Rml::Matrix4f mat = *transform;
    drawing_matrix[0] = Vector4(mat[0].x, mat[0].y, mat[0].z, mat[0].w);
    drawing_matrix[1] = Vector4(mat[1].x, mat[1].y, mat[1].z, mat[1].w);
    drawing_matrix[2] = Vector4(mat[2].x, mat[2].y, mat[2].z, mat[2].w);
    drawing_matrix[3] = Vector4(mat[3].x, mat[3].y, mat[3].z, mat[3].w);
}

RenderInterfaceGodot::RenderInterfaceGodot() {
    singleton = this;
}
RenderInterfaceGodot::~RenderInterfaceGodot() {
    singleton = nullptr;
}
