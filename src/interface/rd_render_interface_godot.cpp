#include "rd_render_interface_godot.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/uniform_set_cache_rd.hpp>
#include <godot_cpp/classes/framebuffer_cache_rd.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/rd_framebuffer_pass.hpp>
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
#include "../image_utils.h"

using namespace godot;

using RD = RenderingDevice;

#define FLAGS_CONVERT_SRGB_TO_LINEAR (1 << 0)

Projection get_final_transform(const Projection &p_drawing_matrix, const Vector2 &translation) {
    return p_drawing_matrix * Projection(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, translation.x, translation.y, 0, 1);
}

void matrix_to_pointer(float *ptr, const Projection &p_mat) {
    ptr[0]  = p_mat[0][0];
    ptr[1]  = p_mat[0][1];
    ptr[2]  = p_mat[0][2];
    ptr[3]  = p_mat[0][3];

    ptr[4]  = p_mat[1][0];
    ptr[5]  = p_mat[1][1];
    ptr[6]  = p_mat[1][2];
    ptr[7]  = p_mat[1][3];

    ptr[8]  = p_mat[2][0];
    ptr[9]  = p_mat[2][1];
    ptr[10] = p_mat[2][2];
    ptr[11] = p_mat[2][3];
    
    ptr[12] = p_mat[3][0];
    ptr[13] = p_mat[3][1];
    ptr[14] = p_mat[3][2];
    ptr[15] = p_mat[3][3];
}

void create_pipeline_with_clip_variant(RenderingResources &p_resources, RID &p_pipeline, RID &p_clip_pipeline, const std::map<String, Variant> &p_params) {
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
    p_pipeline = p_resources.create_render_pipeline(p_params);
    p_clip_pipeline = p_resources.create_render_pipeline(clipping_params);
}

void create_geometry_shader_and_pipeline(RenderingResources &p_resources, const RID &p_shader, RID &p_pipeline, RID &p_clip_pipeline, int64_t p_fb_format, int64_t p_vertex_format) {
    create_pipeline_with_clip_variant(p_resources, p_pipeline, p_clip_pipeline, {
        {"shader", p_shader},
        {"framebuffer_format", p_fb_format},
        {"vertex_format", p_vertex_format},
        {"attachment_count", 2},
        {"attachment_enable_blend", true},
        {"attachment_color_blend_op", RD::BLEND_OP_ADD},
        {"attachment_alpha_blend_op", RD::BLEND_OP_ADD},
        {"attachment_src_color_blend_factor", RD::BLEND_FACTOR_ONE},
        {"attachment_dst_color_blend_factor", RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
        {"attachment_src_alpha_blend_factor", RD::BLEND_FACTOR_ONE},
        {"attachment_dst_alpha_blend_factor", RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
    });
}

void create_filter_shader_and_pipeline(RenderingResources &p_resources, Pipeline *&p_pipeline, const String &p_name) {
    RID shader = p_resources.create_shader({
        {"path", vformat("res://addons/rmlui/shaders/filters/%s.glsl", p_name)},
        {"name", vformat("rmlui_filter_%s_shader", p_name)}
    });
    p_pipeline = memnew(Pipeline(
        &p_resources, shader, {
            {"shader", shader},
            {"attachment_count", 2}
        }
    ));
}

RID RDRenderInterfaceGodot::get_framebuffer(const TypedArray<RID> &p_textures, int64_t &p_framebuffer_format) {
    RID fb = FramebufferCacheRD::get_cache_multipass(p_textures, {}, 1);
    p_framebuffer_format = internal_rendering_resources.device()->framebuffer_get_format(fb);
    return fb;
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

	geometry_vertex_format = rd->vertex_format_create({ pos_attr, color_attr, uv_attr });

    RID shader_geometry = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/geometry.glsl"},
        {"name", "rmlui_geometry_shader"}
    });
    RID shader_layer_composition = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/layer_composition.glsl"},
        {"name", "rmlui_layer_composition_shader"}
    });
    RID shader_clip_mask = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/clip_mask.glsl"},
        {"name", "rmlui_clip_mask_shader"}
    });
    RID shader_downsample = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/downsample.glsl"},
        {"name", "rmlui_downsample_shader"}
    });

    pipeline_geometry = memnew(Pipeline(
        &internal_rendering_resources, shader_geometry, {
            {"vertex_format", geometry_vertex_format},
            {"attachment_count", 2},
            {"attachment_enable_blend", true},
            {"attachment_color_blend_op", RD::BLEND_OP_ADD},
            {"attachment_alpha_blend_op", RD::BLEND_OP_ADD},
            {"attachment_src_color_blend_factor", RD::BLEND_FACTOR_ONE},
            {"attachment_dst_color_blend_factor", RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
            {"attachment_src_alpha_blend_factor", RD::BLEND_FACTOR_ONE},
            {"attachment_dst_alpha_blend_factor", RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
        }
    ));
    pipeline_layer_composition = memnew(Pipeline(
        &internal_rendering_resources, shader_layer_composition, {
            {"attachment_count", 2},
        }
    ));
    pipeline_clip_mask_set = memnew(Pipeline(
        &internal_rendering_resources, shader_clip_mask, {
            {"vertex_format", geometry_vertex_format},
            {"attachment_count", 1},

            {"enable_stencil", true},
            {"op_ref", 0x01},
            {"op_write_mask", 0xff},
            {"op_compare", RD::COMPARE_OP_ALWAYS},
            {"op_compare_mask", 0xff},
            {"op_pass", RD::STENCIL_OP_REPLACE},
        }
    ));
    pipeline_clip_mask_set_inverse = memnew(Pipeline(
        &internal_rendering_resources, shader_clip_mask, {
            {"vertex_format", geometry_vertex_format},
            {"attachment_count", 1},

            {"enable_stencil", true},
            {"op_ref", 0x01},
            {"op_write_mask", 0xff},
            {"op_compare", RD::COMPARE_OP_ALWAYS},
            {"op_compare_mask", 0xff},
            {"op_pass", RD::STENCIL_OP_REPLACE},
        }
    ));
    pipeline_clip_mask_intersect = memnew(Pipeline(
        &internal_rendering_resources, shader_clip_mask, {
            {"vertex_format", geometry_vertex_format},
            {"attachment_count", 1},

            {"enable_stencil", true},
            {"op_ref", 0x01},
            {"op_write_mask", 0xff},
            {"op_compare", RD::COMPARE_OP_ALWAYS},
            {"op_compare_mask", 0xff},
            {"op_pass", RD::STENCIL_OP_REPLACE},
        }
    ));
    pipeline_downsample = memnew(Pipeline(
        &internal_rendering_resources, shader_downsample, {
            {"attachment_count", 1}
        }
    ));

    create_filter_shader_and_pipeline(internal_rendering_resources, pipeline_filter_modulate, "modulate");
    create_filter_shader_and_pipeline(internal_rendering_resources, pipeline_filter_color_matrix, "color_matrix");
    create_filter_shader_and_pipeline(internal_rendering_resources, pipeline_filter_blur, "blur");
    create_filter_shader_and_pipeline(internal_rendering_resources, pipeline_filter_drop_shadow, "drop_shadow");
    create_filter_shader_and_pipeline(internal_rendering_resources, pipeline_filter_blend_mask, "blend_mask");

    RID shader_gradient = internal_rendering_resources.create_shader({
        {"path", "res://addons/rmlui/shaders/shaders/gradient.glsl"},
        {"name", "rmlui_gradient_shader"}
    });
    pipeline_shader_gradient = memnew(Pipeline(
        &internal_rendering_resources, shader_gradient, {
            {"vertex_format", geometry_vertex_format},
            {"attachment_count", 2},
            {"attachment_enable_blend", true},
            {"attachment_color_blend_op", RD::BLEND_OP_ADD},
            {"attachment_alpha_blend_op", RD::BLEND_OP_ADD},
            {"attachment_src_color_blend_factor", RD::BLEND_FACTOR_ONE},
            {"attachment_dst_color_blend_factor", RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
            {"attachment_src_alpha_blend_factor", RD::BLEND_FACTOR_ONE},
            {"attachment_dst_alpha_blend_factor", RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA},
        }
    ));

    sampler_nearest_no_repeat = internal_rendering_resources.create_sampler({
        {"filter", RD::SAMPLER_FILTER_NEAREST},
        {"repeat_u", RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE},
        {"repeat_v", RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE},
        {"repeat_w", RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE},
    });
    sampler_linear_no_repeat = internal_rendering_resources.create_sampler({
        {"filter", RD::SAMPLER_FILTER_LINEAR},
        {"repeat_u", RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE},
        {"repeat_v", RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE},
        {"repeat_w", RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE},
    });
    sampler_linear_repeat = internal_rendering_resources.create_sampler({
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
    pipeline_geometry->free(true);
    pipeline_layer_composition->free(true);

    // For clip mask, only free the shader on the first, as the others use the same shader RID
    pipeline_clip_mask_set->free(true);
    pipeline_clip_mask_set_inverse->free();
    pipeline_clip_mask_intersect->free();

    pipeline_downsample->free(true);

    pipeline_filter_modulate->free(true);
    pipeline_filter_color_matrix->free(true);
    pipeline_filter_blur->free(true);
    pipeline_filter_drop_shadow->free(true);
    pipeline_filter_blend_mask->free(true);

    pipeline_shader_gradient->free(true);

    memdelete(pipeline_geometry);
    memdelete(pipeline_layer_composition);
    memdelete(pipeline_clip_mask_set);
    memdelete(pipeline_clip_mask_set_inverse);
    memdelete(pipeline_clip_mask_intersect);
    memdelete(pipeline_downsample);

    memdelete(pipeline_filter_modulate);
    memdelete(pipeline_filter_color_matrix);
    memdelete(pipeline_filter_blur);
    memdelete(pipeline_filter_drop_shadow);
    memdelete(pipeline_filter_blend_mask);

    memdelete(pipeline_shader_gradient);

	internal_rendering_resources.free_all_resources();
    rendering_resources.free_all_resources();
    free_pending_resources();
}

void RDRenderInterfaceGodot::push_context(void *&p_ctx, const Vector2i &p_size) {
	Context *ctx = static_cast<Context *>(p_ctx);
    if (unlikely(ctx == nullptr)) {
        ctx = memnew(Context);
        p_ctx = ctx;
    }
    context = ctx;
    context->target_stack_ptr = 0;

	ctx->draw_commands.clear();
}

void RDRenderInterfaceGodot::pop_context() {
    int layer = context->target_stack_ptr;
    context = nullptr;
    ERR_FAIL_COND(layer != 0);
}

void RDRenderInterfaceGodot::draw_context(void *&p_ctx, const RID &p_canvas_item, const Vector2i &p_base_offset, RenderCanvasDataRD *p_render_data) {
    Context *ctx = static_cast<Context *>(p_ctx);

    ctx->canvas_item = p_canvas_item;
    Projection base_transform = Projection(Transform3D(
        Vector3(1, 0, 0),
        Vector3(0, 1, 0),
        Vector3(0, 0, 1),
        Vector3(p_base_offset.x, p_base_offset.y, 0)
    ));

    RID screen_texture = p_render_data->get_screen_texture();
    bool use_linear_colors = p_render_data->is_using_linear_colors();

    RenderingServer *rs = RenderingServer::get_singleton();
    RenderingDevice *rd = internal_rendering_resources.device();
    Ref<RDTextureFormat> screen_format = rd->texture_get_format(screen_texture);
    Vector2i size = Vector2i(screen_format->get_width(), screen_format->get_height());
    float inv_size_x = 1.0 / size.x;
    float inv_size_y = 1.0 / size.y;

    int64_t screen_texture_format = (int64_t)screen_format->get_format();

    ctx->main_target.color = screen_texture;
    allocate_context(ctx, size, screen_texture_format);

    ctx->main_target.size = size;
    ctx->target_stack_ptr = 0;

    for (auto &cmd : ctx->draw_commands) {
        switch (cmd.type) {
            case DrawCommand::TYPE_RENDER_GEOMETRY: {
                Projection matrix = get_final_transform(base_transform * drawing_matrix, cmd.render_geometry.translation);
                MeshData *mesh_data = reinterpret_cast<MeshData *>(cmd.render_geometry.geometry_id);

                if (bounds_culled(mesh_data->bounds, matrix, size)) continue;

                RenderPass pass;
                pass.debug_label = clip_mask_enabled ? "Geometry (clip mask enabled)" : "Geometry";

                pass.mesh_data = mesh_data;

                struct {
                    float transform[16];
                    float inv_viewport_size[2];
                    unsigned int flags;
                    float pad0;
                } push_const;

                push_const.flags = 0;
                if (use_linear_colors) {
                    push_const.flags |= FLAGS_CONVERT_SRGB_TO_LINEAR;
                }

                push_const.inv_viewport_size[0] = 1.0 / size.x;
                push_const.inv_viewport_size[1] = 1.0 / size.y;
                matrix_to_pointer(&push_const.transform[0], matrix);

                pass.push_const.resize(sizeof(push_const));
                memcpy(pass.push_const.ptrw(), &push_const, sizeof(push_const));

                if (cmd.render_geometry.texture_id != 0) {
                    TextureData *tex = reinterpret_cast<TextureData *>(cmd.render_geometry.texture_id);
                    if (use_linear_colors) {
                        pass.uniform_textures.push_back(tex->rid_srgb);
                    } else {
                        pass.uniform_textures.push_back(tex->rid);
                    }
                    if (tex->linear_filtering) {
                        pass.uniform_samplers.push_back(sampler_linear_no_repeat);
                    } else {
                        pass.uniform_samplers.push_back(sampler_nearest_no_repeat);
                    }
                } else {
                    pass.uniform_textures.push_back(texture_white);
                    pass.uniform_samplers.push_back(sampler_nearest_no_repeat);
                }
                int64_t fb_format;
                pass.framebuffer = get_framebuffer({
                    context_get_current_render_target(ctx)->color,
                    ctx->clip_mask
                }, fb_format);

                pass.shader = pipeline_geometry->shader;
                pass.pipeline = pipeline_geometry->get_pipeline_variant(geometry_vertex_format, fb_format, clip_mask_enabled);

                render_pass(pass);
            } break;
            case DrawCommand::TYPE_RENDER_SHADER: {
                Projection matrix = get_final_transform(base_transform * drawing_matrix, cmd.render_shader.translation);
                MeshData *mesh_data = reinterpret_cast<MeshData *>(cmd.render_shader.geometry_id);

                if (bounds_culled(mesh_data->bounds, matrix, size)) continue;

                ShaderData *shader = reinterpret_cast<ShaderData *>(cmd.render_shader.shader_id);

                RenderPass pass;
                pass.debug_label = vformat("Shader %s %s", shader->name, clip_mask_enabled ? " (clip mask enabled)" : "");

                pass.mesh_data = mesh_data;

                struct {
                    float transform[16];
                    float inv_viewport_size[2];
                    unsigned int flags;
                    float pad0;
                } push_const;

                push_const.flags = 0;
                if (use_linear_colors) {
                    push_const.flags |= FLAGS_CONVERT_SRGB_TO_LINEAR;
                }

                push_const.inv_viewport_size[0] = 1.0 / size.x;
                push_const.inv_viewport_size[1] = 1.0 / size.y;
                matrix_to_pointer(&push_const.transform[0], matrix);

                pass.push_const.resize(shader->push_const.size());
                memcpy(pass.push_const.ptrw(), shader->push_const.ptr(), shader->push_const.size());
                memcpy(pass.push_const.ptrw(), &push_const, sizeof(push_const));

                pass.uniform_buffer = shader->uniform_buffer;

                if (cmd.render_shader.texture_id != 0) {
                    TextureData *tex = reinterpret_cast<TextureData *>(cmd.render_shader.texture_id);
                    if (use_linear_colors) {
                        pass.uniform_textures.push_back(tex->rid_srgb);
                    } else {
                        pass.uniform_textures.push_back(tex->rid);
                    }
                    if (tex->linear_filtering) {
                        pass.uniform_samplers.push_back(sampler_linear_no_repeat);
                    } else {
                        pass.uniform_samplers.push_back(sampler_nearest_no_repeat);
                    }
                } else {
                    pass.uniform_textures.push_back(texture_white);
                    pass.uniform_samplers.push_back(sampler_nearest_no_repeat);
                }
                int64_t fb_format;
                pass.framebuffer = get_framebuffer({
                    context_get_current_render_target(ctx)->color,
                    ctx->clip_mask
                }, fb_format);

                pass.shader = shader->pipeline->shader;
                pass.pipeline = shader->pipeline->get_pipeline_variant(geometry_vertex_format, fb_format, clip_mask_enabled);

                render_pass(pass);
            } break;
            case DrawCommand::TYPE_RENDER_CLIP_MASK: {
                Projection matrix = get_final_transform(base_transform * drawing_matrix, cmd.render_clip_mask.translation);
                MeshData *mesh_data = reinterpret_cast<MeshData *>(cmd.render_clip_mask.geometry_id);

                if (bounds_culled(mesh_data->bounds, matrix, size)) continue;

                RenderPass pass;
                pass.debug_label = "Render Clip Mask";

                constexpr unsigned int clip_mask_operation_set = (unsigned int)Rml::ClipMaskOperation::Set;
                constexpr unsigned int clip_mask_operation_set_inverse = (unsigned int)Rml::ClipMaskOperation::SetInverse;
                constexpr unsigned int clip_mask_operation_intersect = (unsigned int)Rml::ClipMaskOperation::Intersect;

                Pipeline *pipeline;

                switch (cmd.render_clip_mask.clip_operation) {
                    case clip_mask_operation_set: 
                        pipeline = pipeline_clip_mask_set; break;
                    case clip_mask_operation_set_inverse: 
                        pipeline = pipeline_clip_mask_set_inverse; break;
                    case clip_mask_operation_intersect: 
                        pipeline = pipeline_clip_mask_intersect; break;
                }

                pass.mesh_data = mesh_data;

                struct {
                    float transform[16];
                    float inv_viewport_size[2];
                    float pad0[2];
                } push_const;
                
                push_const.inv_viewport_size[0] = 1.0 / size.x;
                push_const.inv_viewport_size[1] = 1.0 / size.y;
                matrix_to_pointer(&push_const.transform[0], matrix);

                pass.push_const.resize(sizeof(push_const));
                memcpy(pass.push_const.ptrw(), &push_const, sizeof(push_const));

                pass.draw_flags = RD::DRAW_CLEAR_STENCIL;
                pass.clear_stencil = 0;
                if (cmd.render_clip_mask.clip_operation == clip_mask_operation_set_inverse) {
                    pass.clear_stencil = 1;
                }
                if (cmd.render_clip_mask.clip_operation == clip_mask_operation_intersect) {
                    pass.draw_flags = RD::DRAW_DEFAULT_ALL;
                }

                int64_t fb_format;
                pass.framebuffer = get_framebuffer({ctx->clip_mask}, fb_format);

                pass.shader = pipeline->shader;
                pass.pipeline = pipeline->get_pipeline_variant(geometry_vertex_format, fb_format, false);

                render_pass(pass);
            } break;
            case DrawCommand::TYPE_COMPOSITE_LAYERS: {
                if (empty_scissor()) continue;

                ERR_CONTINUE(cmd.composite_layers.source_layer_id < 0);
                ERR_CONTINUE(cmd.composite_layers.source_layer_id >= ctx->target_stack.size());
                ERR_CONTINUE(cmd.composite_layers.destination_layer_id < 0);
                ERR_CONTINUE(cmd.composite_layers.destination_layer_id >= ctx->target_stack.size());
                RenderTarget *source_target = ctx->target_stack[cmd.composite_layers.source_layer_id];
                RenderTarget *destination_target = ctx->target_stack[cmd.composite_layers.destination_layer_id];

                blit(source_target->color, ctx->filter_target.color, ctx->size);
                for (int i = 0; i < cmd.composite_layers.filter_count; i++) {
                    FilterData *filter_data = reinterpret_cast<FilterData *>(cmd.composite_layers.filter_ids[i]);
                    filter_pass(ctx, filter_data);
                }
                blit(destination_target->color, ctx->back_target0.color, ctx->size);

                RenderPass pass;
                pass.debug_label = clip_mask_enabled ? "Composite Layers (clip mask enabled)" : "Composite Layers";
                
                struct {
                    unsigned int blend_mode;
                    float pad0[3];
                } push_const;

                push_const.blend_mode = cmd.composite_layers.blend_mode;

                pass.push_const.resize(sizeof(push_const));
                memcpy(pass.push_const.ptrw(), &push_const, sizeof(push_const));

                pass.uniform_textures.push_back(ctx->back_target0.color);
                pass.uniform_textures.push_back(ctx->filter_target.color);
                pass.uniform_samplers.push_back(sampler_nearest_no_repeat);
                pass.uniform_samplers.push_back(sampler_nearest_no_repeat);

                int64_t fb_format;
                pass.framebuffer = get_framebuffer({
                    destination_target->color,
                    ctx->clip_mask
                }, fb_format);
                pass.shader = pipeline_layer_composition->shader;
                pass.pipeline = pipeline_layer_composition->get_pipeline_variant(-1, fb_format, clip_mask_enabled);

                render_pass(pass);
            } break;
            case DrawCommand::TYPE_RENDER_LAYER_TO_TEXTURE: {
                Rect2i region = Rect2i(Vector2i(), ctx->size);
                if (scissor_enabled) {
                    region = scissor_region;
                }
                if (region.size.x == 0 || region.size.y == 0) continue;

                TextureData *tex_data = reinterpret_cast<TextureData *>(cmd.render_layer_to_texture.texture_id);
                RenderTarget target;
                target.color = tex_data->rid;
                target.size = tex_data->size;
                if (likely(target.size != region.size)) {
                    free_render_target(&target);
                    allocate_render_target(&target, region.size, screen_texture_format);

                    Ref<RDTextureView> srgb_view = memnew(RDTextureView);
                    srgb_view->set_format_override((RD::DataFormat)target.srgb_format);

                    tex_data->rid = target.color;
                    tex_data->rid_srgb = rd->texture_create_shared(srgb_view, target.color);
                    
                    tex_data->size = target.size;
                }

                blit(
                    context_get_current_render_target(ctx)->color, target.color,
                    region.size, region.position
                );
            } break;
            case DrawCommand::TYPE_RENDER_LAYER_TO_BLEND_MASK: {
                Rect2i region = Rect2i(Vector2i(), ctx->size);
                if (scissor_enabled) {
                    region = scissor_region;
                }
                if (region.size.x == 0 || region.size.y == 0) continue;

                blit(
                    context_get_current_render_target(ctx)->color, ctx->blend_mask_target.color,
                    region.size, region.position, region.position
                );
            } break;
            case DrawCommand::TYPE_SCISSOR: {
                scissor_enabled = cmd.scissor.enable;
                if (!scissor_enabled) continue;

                Vector2i start = cmd.scissor.region.position + p_base_offset;
                Vector2i end = cmd.scissor.region.get_end() + p_base_offset;

                start = start.clamp(Vector2i(0, 0), size - Vector2i(1, 1));
                end = end.clamp(Vector2i(0, 0), size - Vector2i(1, 1));

                scissor_region = Rect2i(start, end - start);
            } break;
            case DrawCommand::TYPE_CLIP_MASK: {
                clip_mask_enabled = cmd.clip_mask.enable;
            } break;
            case DrawCommand::TYPE_TRANSFORM: {
                drawing_matrix = cmd.transform.matrix;
            } break;
            case DrawCommand::TYPE_PUSH_LAYER: {
                ctx->target_stack_ptr++;
                if (ctx->target_stack_ptr == ctx->target_stack.size()) {
                    ctx->target_stack.push_back(memnew(RenderTarget));
                    allocate_render_target(ctx->target_stack.back(), size, screen_texture_format);
                }
                RenderTarget *target = context_get_current_render_target(ctx);
                rd->texture_clear(target->color, Color(0, 0, 0, 0), 0, 1, 0, 1);
            } break;
            case DrawCommand::TYPE_POP_LAYER: {
                ctx->target_stack_ptr--;
            } break;

            case DrawCommand::TYPE_NONE: break;
        }
    }

    free_pending_resources();
}

void RDRenderInterfaceGodot::filter_pass(Context *p_context, FilterData *p_filter) {
    RD *rd = internal_rendering_resources.device();

    switch (p_filter->type) {
        case FilterData::TYPE_MODULATE: {
            blit(p_context->filter_target.color, p_context->back_target0.color, p_context->size);
            
            RenderPass pass;
            pass.debug_label = "Filter Modulate";
            pass.uniform_textures.push_back(p_context->back_target0.color);
            pass.uniform_samplers.push_back(sampler_nearest_no_repeat);
            int64_t fb_format;
            pass.framebuffer = get_framebuffer({p_context->filter_target.color}, fb_format);
            pass.shader = pipeline_filter_modulate->shader;
            pass.pipeline = pipeline_filter_modulate->get_pipeline_variant(-1, fb_format, false);

            struct {
                float modulate[4];
                unsigned int space;
                float pad0[3];
            } push_const;

            push_const.modulate[0] = p_filter->modulate.color.r;
            push_const.modulate[1] = p_filter->modulate.color.g;
            push_const.modulate[2] = p_filter->modulate.color.b;
            push_const.modulate[3] = p_filter->modulate.color.a;
            push_const.space = p_filter->modulate.space;

            pass.push_const.resize(sizeof(push_const));
            memcpy(pass.push_const.ptrw(), &push_const, sizeof(push_const));

            render_pass(pass);
        } break;
        case FilterData::TYPE_COLOR_MATRIX: {
            blit(p_context->filter_target.color, p_context->back_target0.color, p_context->size);

            RenderPass pass;
            pass.debug_label = "Filter Color Matrix";
            pass.uniform_textures.push_back(p_context->back_target0.color);
            pass.uniform_samplers.push_back(sampler_nearest_no_repeat);
            int64_t fb_format;
            pass.framebuffer = get_framebuffer({p_context->filter_target.color}, fb_format);
            pass.shader = pipeline_filter_color_matrix->shader;
            pass.pipeline = pipeline_filter_color_matrix->get_pipeline_variant(-1, fb_format, false);

            struct {
                float matrix[16];
            } push_const;

            matrix_to_pointer(&push_const.matrix[0], p_filter->color_matrix.matrix);

            pass.push_const.resize(sizeof(push_const));
            memcpy(pass.push_const.ptrw(), &push_const, sizeof(push_const));

            render_pass(pass);
        } break;
        case FilterData::TYPE_BLUR: {
            blit(p_context->filter_target.color, p_context->back_target0.color, p_context->size);

            float rad = p_filter->blur.sigma;
            
            float lvl = CLAMP(log2(MAX(rad, 1.0)), 0, p_context->back_target0.mipmap_levels.size() - 1);
            
            int required_mipmaps = Math::ceil(lvl);
            generate_mipmaps(&p_context->back_target0, required_mipmaps);

            RenderPass pass;
            pass.debug_label = "Filter Blur";
            pass.uniform_textures.push_back(p_context->back_target0.color);
            pass.uniform_samplers.push_back(sampler_linear_no_repeat);
            int64_t fb_format;
            pass.framebuffer = get_framebuffer({p_context->filter_target.color}, fb_format);
            pass.shader = pipeline_filter_blur->shader;
            pass.pipeline = pipeline_filter_blur->get_pipeline_variant(-1, fb_format, false);

            struct {
                float lvl;
                float pad0[3];
            } push_const;
            push_const.lvl = lvl;

            pass.push_const.resize(sizeof(push_const));
            memcpy(pass.push_const.ptrw(), &push_const, sizeof(push_const));

            render_pass(pass);
        } break;
        case FilterData::TYPE_DROP_SHADOW: {
            blit(p_context->filter_target.color, p_context->back_target0.color, p_context->size);
            blit(p_context->filter_target.color, p_context->back_target1.color, p_context->size);

            float rad = p_filter->drop_shadow.sigma;
            
            float lvl = CLAMP(log2(MAX(rad, 1.0)), 0, p_context->back_target0.mipmap_levels.size() - 1);
            
            int required_mipmaps = Math::ceil(lvl);
            generate_mipmaps(&p_context->back_target0, required_mipmaps);

            RenderPass pass;
            pass.debug_label = "Filter Drop Shadow";
            pass.uniform_textures.push_back(p_context->back_target0.color);
            pass.uniform_textures.push_back(p_context->back_target1.color);
            pass.uniform_samplers.push_back(sampler_linear_no_repeat);
            pass.uniform_samplers.push_back(sampler_nearest_no_repeat);
            int64_t fb_format;
            pass.framebuffer = get_framebuffer({p_context->filter_target.color}, fb_format);
            pass.shader = pipeline_filter_drop_shadow->shader;
            pass.pipeline = pipeline_filter_drop_shadow->get_pipeline_variant(-1, fb_format, false);

            struct {
                float color[4];
                float offset[2];
                float lvl;
                float pad0;
            } push_const;
            push_const.color[0] = p_filter->drop_shadow.color.r;
            push_const.color[1] = p_filter->drop_shadow.color.g;
            push_const.color[2] = p_filter->drop_shadow.color.b;
            push_const.color[3] = p_filter->drop_shadow.color.a;
            push_const.offset[0] = p_filter->drop_shadow.offset.x / p_context->size.x;
            push_const.offset[1] = p_filter->drop_shadow.offset.y / p_context->size.y;
            push_const.lvl = lvl;

            pass.push_const.resize(sizeof(push_const));
            memcpy(pass.push_const.ptrw(), &push_const, sizeof(push_const));

            render_pass(pass);
        } break;
        case FilterData::TYPE_BLEND_MASK: {
            blit(p_context->filter_target.color, p_context->back_target0.color, p_context->size);

            RenderPass pass;
            pass.debug_label = "Filter Blend Mask";
            pass.uniform_textures.push_back(p_context->back_target0.color);
            pass.uniform_textures.push_back(p_context->blend_mask_target.color);
            pass.uniform_samplers.push_back(sampler_linear_no_repeat);
            pass.uniform_samplers.push_back(sampler_linear_no_repeat);
            int64_t fb_format;
            pass.framebuffer = get_framebuffer({p_context->filter_target.color}, fb_format);
            pass.shader = pipeline_filter_blend_mask->shader;
            pass.pipeline = pipeline_filter_blend_mask->get_pipeline_variant(-1, fb_format, false);

            render_pass(pass);
        } break;
    }
}

void RDRenderInterfaceGodot::generate_mipmaps(RenderTarget *p_target, int p_mipmaps) {
    if (p_mipmaps <= -1) p_mipmaps = p_target->mipmap_levels.size() - 1;
    p_mipmaps = MIN(p_mipmaps, p_target->mipmap_levels.size() - 1);
    if (p_mipmaps == 0) return;

    RD *rd = internal_rendering_resources.device();

    RenderPass pass;
    pass.shader = pipeline_downsample->shader;
    pass.uniform_samplers.push_back(sampler_linear_no_repeat);
    pass.uniform_textures.resize(1);
    pass.ignore_scissor = true;

    struct {
        float dst_pixel_size[2];
        float pad0[2];
    } push_const;

    pass.push_const.resize(sizeof(push_const));

    int w = p_target->size.x;
    int h = p_target->size.y;

    for (int i = 0; i < p_mipmaps; i++) {
        w = MAX(w >> 1, 1);
        h = MAX(h >> 1, 1);

        pass.debug_label = vformat("Mipmap %d", i);
        pass.uniform_textures[0] = p_target->mipmap_levels[i];
        int64_t fb_format;
        pass.framebuffer = get_framebuffer({p_target->mipmap_levels[i + 1]}, fb_format);
        push_const.dst_pixel_size[0] = 1.0 / w;
        push_const.dst_pixel_size[1] = 1.0 / h;

        pass.pipeline = pipeline_downsample->get_pipeline_variant(-1, fb_format, false);

        memcpy(pass.push_const.ptrw(), &push_const, sizeof(push_const));
        render_pass(pass);
    }
}

void RDRenderInterfaceGodot::free_context(void *&p_ctx) {
    Context *ctx = static_cast<Context *>(p_ctx);
    if (ctx == nullptr) return;

    free_context(ctx);
    memdelete(ctx);

    p_ctx = nullptr;
}

RDRenderInterfaceGodot::DrawCommand *RDRenderInterfaceGodot::push_draw_command() {
	context->draw_commands.push_back(DrawCommand());
	return &context->draw_commands.back();
}

void RDRenderInterfaceGodot::allocate_render_target(RenderTarget *p_target, const Vector2i &p_size, int64_t p_texture_format) {
    if (unlikely(p_target->size == p_size && p_target->texture_format == p_texture_format)) return;

	free_render_target(p_target);

    Image::Format image_format;
    RD::DataFormat srgb_format;
    image_get_rd_format((RD::DataFormat)p_texture_format, image_format, srgb_format);

    int mipmap_count;
    image_get_dst_size(p_size.x, p_size.y, image_format, mipmap_count, -1);

    uint64_t main_usage = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;
    p_target->color = rendering_resources.create_texture({
        {"width", p_size.x},
        {"height", p_size.y},
        {"mipmaps", mipmap_count},
        {"format", p_texture_format},
        {"shareable_formats", TypedArray<int>({ srgb_format })},
        {"usage_bits", main_usage},
    });

    RenderingDevice *rd = rendering_resources.device();
    for (int i = 0; i < mipmap_count; i++) {
        Ref<RDTextureView> view = memnew(RDTextureView);
        RID mipmap = rd->texture_create_shared_from_slice(view, p_target->color, 0, i);
        p_target->mipmap_levels.push_back(mipmap);
    }

    p_target->size = p_size;
    p_target->texture_format = p_texture_format;
    p_target->srgb_format = srgb_format;
}

void RDRenderInterfaceGodot::free_render_target(RenderTarget *p_target) {
    if (likely(p_target->color.is_valid())) {
        rendering_resources.free_texture(p_target->color);
        p_target->color = RID();
    }

    p_target->mipmap_levels.clear();
}

void RDRenderInterfaceGodot::allocate_context(Context *p_context, const Vector2i &p_size, int64_t p_texture_format) {
    if (likely(p_context->size == p_size && p_context->texture_format == p_texture_format)) {
        return;
    }
    free_context(p_context);

    RenderingDevice *rd = rendering_resources.device();

    p_context->clip_mask = rendering_resources.create_texture({
        {"width", p_size.x},
        {"height", p_size.y},
        {"format", RD::DATA_FORMAT_S8_UINT},
        {"usage_bits", RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT},
    });

    allocate_render_target(&p_context->back_target0, p_size, p_texture_format);
    allocate_render_target(&p_context->back_target1, p_size, p_texture_format);
    allocate_render_target(&p_context->filter_target, p_size, p_texture_format);
    allocate_render_target(&p_context->blend_mask_target, p_size, p_texture_format);

    p_context->target_stack.push_back(&p_context->main_target);
    p_context->size = p_size;
    p_context->texture_format = p_texture_format;
}

void RDRenderInterfaceGodot::free_context(Context *p_context) {
    RenderingServer *rs = RenderingServer::get_singleton();

    free_render_target(&p_context->back_target0);
    free_render_target(&p_context->back_target1);
    free_render_target(&p_context->filter_target);
    free_render_target(&p_context->blend_mask_target);

    for (int i = p_context->target_stack.size() - 1; i >= 1; i--) {
        free_render_target(p_context->target_stack[i]);
    }
	p_context->target_stack.clear();
    
    if (likely(p_context->clip_mask.is_valid())) {
        rendering_resources.free_texture(p_context->clip_mask);
        p_context->clip_mask = RID();
    }
}

void RDRenderInterfaceGodot::free_pending_resources() {
    for (MeshData *mesh_data : meshes_to_release) {
        rendering_resources.free_vertex_array(mesh_data->vertex_array);
        rendering_resources.free_index_array(mesh_data->index_array);
        rendering_resources.free_vertex_buffer(mesh_data->position_buffer);
        rendering_resources.free_vertex_buffer(mesh_data->color_buffer);
        rendering_resources.free_vertex_buffer(mesh_data->uv_buffer);
        rendering_resources.free_index_buffer(mesh_data->index_buffer);

        memdelete(mesh_data);
    }
    for (TextureData *texture_data : textures_to_release) {
        // Is a generated texture
        if (!texture_data->tex_ref.is_valid()) {
            rendering_resources.free_texture(texture_data->rid);
        }
        texture_data->tex_ref.unref();
        memdelete(texture_data);
    }
    for (FilterData *filter_data : filters_to_release) {
        memdelete(filter_data);
    }
    for (ShaderData *shader_data : shaders_to_release) {
        if (likely(shader_data->uniform_buffer.is_valid())) {
            rendering_resources.free_storage_buffer(shader_data->uniform_buffer);
        }

        memdelete(shader_data);
    }
    meshes_to_release.clear();
    textures_to_release.clear();
    filters_to_release.clear();
    shaders_to_release.clear();
}

RDRenderInterfaceGodot::RenderTarget *RDRenderInterfaceGodot::context_get_current_render_target(Context *p_context) {
    return p_context->target_stack[p_context->target_stack_ptr];
}

bool RDRenderInterfaceGodot::empty_scissor() {
    return scissor_enabled && (scissor_region.size.x <= 0 || scissor_region.size.y <= 0);
}

bool RDRenderInterfaceGodot::bounds_culled(const Rect2 &p_bounds, const Projection &p_projection, const Vector2i &p_screen_size) {
    if (empty_scissor()) return true;

    Rect2 view = Rect2(Vector2(), p_screen_size);
    if (scissor_enabled) {
        view = Rect2(scissor_region.position, scissor_region.size);
    }

    Transform2D transform(
        p_projection[0][0], p_projection[0][1],
        p_projection[1][0], p_projection[1][1],
        p_projection[3][0], p_projection[3][1]
    );

    return !view.intersects_transformed(transform, p_bounds);
}

void RDRenderInterfaceGodot::render_pass(const RenderPass &p_pass) {
    RD *rd = rendering_resources.device();

	TypedArray<RDUniform> uniforms;
	uniforms.resize(p_pass.uniform_textures.size());
	for (int i = p_pass.uniform_textures.size() - 1; i >= 0; i--) {
		Ref<RDUniform> uniform = memnew(RDUniform);
		uniform->set_uniform_type(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
		uniform->set_binding(i);
		uniform->add_id(p_pass.uniform_samplers[i]);
		uniform->add_id(p_pass.uniform_textures[i]);
		uniforms[i] = uniform;
	}

	if (p_pass.uniform_buffer.is_valid()) {
		Ref<RDUniform> uniform = memnew(RDUniform);
		uniform->set_uniform_type(RD::UNIFORM_TYPE_STORAGE_BUFFER);
		uniform->set_binding(uniforms.size());
		uniform->add_id(p_pass.uniform_buffer);
		uniforms.append(uniform);
	}

    rd->draw_command_begin_label(vformat("[GodotRmlUi] %s", p_pass.debug_label), Color(0, 0, 0, 0));

	int64_t draw_list = rd->draw_list_begin(
		p_pass.framebuffer,
		p_pass.draw_flags,
		p_pass.clear_colors,
		1.0,
		p_pass.clear_stencil,
        Rect2(p_pass.region)
	);

	if (scissor_enabled && !p_pass.ignore_scissor) {
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

void RDRenderInterfaceGodot::blit(
    const RID &p_src_tex, const RID &p_dst_tex, 
    const Vector2i &p_size, 
    const Vector2i &p_src_pos, const Vector2i &p_dst_pos, 
    int p_src_mip, int p_dst_mip
) {
    RenderingDevice *rd = internal_rendering_resources.device();
    rd->texture_copy(
        p_src_tex, p_dst_tex, 
        Vector3(p_src_pos.x, p_src_pos.y, 0),
        Vector3(p_dst_pos.x, p_dst_pos.y, 0),
        Vector3(p_size.x, p_size.y, 1),
        p_src_mip, p_dst_mip, 0, 0
    );
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

    Vector2 bounds_min = Vector2( INFINITY,  INFINITY);
    Vector2 bounds_max = Vector2(-INFINITY, -INFINITY);

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

        bounds_min = bounds_min.min(Vector2(v.position.x, v.position.y));
        bounds_max = bounds_max.max(Vector2(v.position.x, v.position.y));
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
    mesh_data->bounds = Rect2(bounds_min, bounds_max - bounds_min);

    return reinterpret_cast<uintptr_t>(mesh_data);
}

void RDRenderInterfaceGodot::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
	DrawCommand *c = push_draw_command();
	c->type = DrawCommand::TYPE_RENDER_GEOMETRY;
	c->render_geometry.geometry_id = geometry;
	c->render_geometry.texture_id = texture;
	c->render_geometry.translation = Vector2(translation.x, translation.y);
}

void RDRenderInterfaceGodot::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
    meshes_to_release.push_back(reinterpret_cast<MeshData *>(geometry));
}

Rml::TextureHandle RDRenderInterfaceGodot::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
	ResourceLoader *rl = ResourceLoader::get_singleton();

    String source_str = rml_to_godot_string(source);

    Ref<Texture2D> tex = rl->load(source_str);
    if (unlikely(!tex.is_valid())) {
        return 0;
    }

    texture_dimensions.x = tex->get_width();
    texture_dimensions.y = tex->get_height();

    RenderingServer *rs = RenderingServer::get_singleton();

    TextureData *tex_data = memnew(TextureData());
    tex_data->tex_ref = tex;
    tex_data->rid = rs->texture_get_rd_texture(tex->get_rid());
    tex_data->rid_srgb = rs->texture_get_rd_texture(tex->get_rid(), true);
    tex_data->size.x = texture_dimensions.x;
    tex_data->size.y = texture_dimensions.y;
    ERR_FAIL_COND_V(!tex_data->rid_srgb.is_valid(), 0);

    return reinterpret_cast<uintptr_t>(tex_data);
}

Rml::TextureHandle RDRenderInterfaceGodot::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) {
	PackedByteArray p_data;
    p_data.resize(source.size());

    uint8_t *ptrw = p_data.ptrw();

    memcpy(ptrw, source.data(), source.size());

    RD *rd = rendering_resources.device();

    Ref<RDTextureView> srgb_view = memnew(RDTextureView);
    srgb_view->set_format_override(RD::DATA_FORMAT_R8G8B8A8_SRGB);

    TextureData *tex_data = memnew(TextureData());
    tex_data->size.x = source_dimensions.x;
    tex_data->size.y = source_dimensions.y;
    tex_data->rid = rendering_resources.create_texture({
        {"width", source_dimensions.x},
        {"height", source_dimensions.y},
        {"format", RD::DATA_FORMAT_R8G8B8A8_UNORM},
        {"usage_bits", RD::TEXTURE_USAGE_SAMPLING_BIT},
        {"shareable_formats", TypedArray<int>({ RD::DATA_FORMAT_R8G8B8A8_SRGB })},
        {"data", TypedArray<PackedByteArray>({ p_data })}
    });
    tex_data->rid_srgb = rd->texture_create_shared(srgb_view, tex_data->rid);
    ERR_FAIL_COND_V(!tex_data->rid_srgb.is_valid(), 0);

    return reinterpret_cast<uintptr_t>(tex_data);
}

void RDRenderInterfaceGodot::ReleaseTexture(Rml::TextureHandle texture) {
    textures_to_release.push_back(reinterpret_cast<TextureData *>(texture));
}

void RDRenderInterfaceGodot::EnableScissorRegion(bool enable) {
    if (!enable) {
        DrawCommand *c = push_draw_command();
        c->type = DrawCommand::TYPE_SCISSOR;
        c->scissor.enable = false;
    }
    scissor_enabled = enable;
}

void RDRenderInterfaceGodot::SetScissorRegion(Rml::Rectanglei region) {
    DrawCommand *c = push_draw_command();
    c->type = DrawCommand::TYPE_SCISSOR;
    c->scissor.enable = true;
    c->scissor.region = Rect2i(
        region.Position().x,
        region.Position().y,
        region.Size().x,
        region.Size().y
    );
    scissor_region = c->scissor.region;
}

void RDRenderInterfaceGodot::EnableClipMask(bool enable) {
    DrawCommand *c = push_draw_command();
    c->type = DrawCommand::TYPE_CLIP_MASK;
    c->clip_mask.enable = enable;
}

void RDRenderInterfaceGodot::RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) {
    DrawCommand *c = push_draw_command();
    c->type = DrawCommand::TYPE_RENDER_CLIP_MASK;
    c->render_clip_mask.clip_operation = (unsigned int)operation;
    c->render_clip_mask.geometry_id = geometry;
    c->render_clip_mask.translation = Vector2(translation.x, translation.y);
}

void RDRenderInterfaceGodot::SetTransform(const Rml::Matrix4f* transform) {
    DrawCommand *c = push_draw_command();
    c->type = DrawCommand::TYPE_TRANSFORM;
    if (transform == nullptr) {
        c->transform.matrix = Projection();
    } else {
        c->transform.matrix = rml_to_godot_projection(*transform);
    }
}

Rml::LayerHandle RDRenderInterfaceGodot::PushLayer() {
    uintptr_t new_layer = ++context->target_stack_ptr;
    DrawCommand *c = push_draw_command();
    c->type = DrawCommand::TYPE_PUSH_LAYER;
    return new_layer;
}

void RDRenderInterfaceGodot::CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination, Rml::BlendMode blend_mode, Rml::Span<const Rml::CompiledFilterHandle> filters) {
    DrawCommand *c = push_draw_command();
    c->type = DrawCommand::TYPE_COMPOSITE_LAYERS;
    c->composite_layers.source_layer_id = source;
    c->composite_layers.destination_layer_id = destination;
    c->composite_layers.blend_mode = (unsigned int)blend_mode;

    int filter_count = MIN(filters.size(), MAX_FILTERS_COUNT);

    c->composite_layers.filter_count = filter_count;
    for (int i = filter_count - 1; i >= 0; i--) {
        uintptr_t *ptr = &c->composite_layers.filter_ids[i];
        *ptr = filters[i];
    }
}

void RDRenderInterfaceGodot::PopLayer() {
    DrawCommand *c = push_draw_command();
    c->type = DrawCommand::TYPE_POP_LAYER;
    context->target_stack_ptr--;
}

Rml::TextureHandle RDRenderInterfaceGodot::SaveLayerAsTexture() {
    RenderTarget *target = memnew(RenderTarget);

    TextureData *tex_data = memnew(TextureData());
    tex_data->linear_filtering = false;

    DrawCommand *c = push_draw_command();
    c->type = DrawCommand::TYPE_RENDER_LAYER_TO_TEXTURE;
    c->render_layer_to_texture.texture_id = reinterpret_cast<uintptr_t>(tex_data);

    return reinterpret_cast<uintptr_t>(tex_data);
}

Rml::CompiledFilterHandle RDRenderInterfaceGodot::SaveLayerAsMaskImage() {
    DrawCommand *c = push_draw_command();
    c->type = DrawCommand::TYPE_RENDER_LAYER_TO_BLEND_MASK;

    FilterData *filter_data = memnew(FilterData);
    filter_data->type = FilterData::TYPE_BLEND_MASK;

    return reinterpret_cast<uintptr_t>(filter_data);
}

Rml::CompiledFilterHandle RDRenderInterfaceGodot::CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) {
    FilterData *filter_data = memnew(FilterData);

    if (name == "opacity") {
        filter_data->type = FilterData::TYPE_MODULATE;
        filter_data->modulate.color = Color(
            1, 1, 1, Rml::Get(parameters, "value", 1.f)
        );
        filter_data->modulate.space = 0;
    } else if (name == "brightness") {
        float value = Rml::Get(parameters, "value", 1.f);
        filter_data->type = FilterData::TYPE_MODULATE;
        filter_data->modulate.color = Color(
            value, value, value
        );
        filter_data->modulate.space = 0;
    } else if (name == "hue-rotate") {
        float value = Rml::Get(parameters, "value", 0.f);
        filter_data->type = FilterData::TYPE_MODULATE;
        filter_data->modulate.color = Color(
            value / Math_TAU, 0, 0
        );
        filter_data->modulate.space = 1;
    } else if (name == "blur") {
        filter_data->type = FilterData::TYPE_BLUR;
        filter_data->blur.sigma = Rml::Get(parameters, "sigma", 0.f);
    } else if (name == "drop-shadow") {
        const Rml::Colourb color = Rml::Get(parameters, "color", Rml::Colourb());
        const Rml::Vector2f offset = Rml::Get(parameters, "offset", Rml::Vector2f());

        filter_data->type = FilterData::TYPE_DROP_SHADOW;
        filter_data->drop_shadow.sigma = Rml::Get(parameters, "sigma", 0.f);
        filter_data->drop_shadow.offset = Vector2(offset.x, offset.y);
        filter_data->drop_shadow.color = Color(
            color.red / 255.0,
            color.green / 255.0,
            color.blue / 255.0,
            color.alpha / 255.0
        );
    } else if (name == "contrast") {
        const float value = Rml::Get(parameters, "value", 1.f);
        const float gray = 0.5f - 0.5f * value;

        Projection color_matrix = Projection(
            value, 0, 0, 0,
            0, value, 0, 0,
            0, 0, value, 0,
            gray, gray, gray, 1
        );

        filter_data->type = FilterData::TYPE_COLOR_MATRIX;
        filter_data->color_matrix.matrix = color_matrix;
    } else if (name == "invert") {
        const float value = Rml::Get(parameters, "value", 0.f);
        const float inverted = 1.f - 2.f * value;

        Projection color_matrix = Projection(
            inverted, 0, 0, 0,
            0, inverted, 0, 0,
            0, 0, inverted, 0,
            value, value, value, 1
        );

        filter_data->type = FilterData::TYPE_COLOR_MATRIX;
        filter_data->color_matrix.matrix = color_matrix;
    } else if (name == "grayscale") {
        const float value = Rml::Get(parameters, "value", 1.f);
        const float rev_value = 1.f - value;
        const Vector3 gray = value * Vector3(0.2126f, 0.7152f, 0.0722f);

        Projection color_matrix = Projection(
            gray.x + rev_value, gray.x, gray.x, 0,
            gray.y, gray.y + rev_value, gray.y, 0,
            gray.z, gray.z, gray.z + rev_value, 0,
            0, 0, 0, 1
        );

        filter_data->type = FilterData::TYPE_COLOR_MATRIX;
        filter_data->color_matrix.matrix = color_matrix;
    } else if (name == "sepia") {
        const float value = Rml::Get(parameters, "value", 1.f);
        const float rev_value = 1.f - value;
		const Vector3 r_mix = value * Vector3(0.393f, 0.769f, 0.189f);
		const Vector3 g_mix = value * Vector3(0.349f, 0.686f, 0.168f);
		const Vector3 b_mix = value * Vector3(0.272f, 0.534f, 0.131f);

        Projection color_matrix = Projection(
            r_mix.x + rev_value, g_mix.x, b_mix.x, 0,
            r_mix.y, g_mix.y + rev_value, g_mix.y, 0,
            r_mix.z, g_mix.z, b_mix.z + rev_value, 0,
            0, 0, 0, 1
        );

        filter_data->type = FilterData::TYPE_COLOR_MATRIX;
        filter_data->color_matrix.matrix = color_matrix;
    } else if (name == "saturate") {
        const float value = Rml::Get(parameters, "value", 1.f);

        Projection color_matrix = Projection(
            0.213f + 0.787f * value, 0.213f - 0.213f * value, 0.213f - 0.213f * value, 0,
            0.715f - 0.715f * value, 0.715f + 0.285f * value, 0.715f - 0.715f * value, 0,
            0.072f - 0.072f * value, 0.072f - 0.072f * value, 0.072f + 0.928f * value, 0,
            0, 0, 0, 1
        );

        filter_data->type = FilterData::TYPE_COLOR_MATRIX;
        filter_data->color_matrix.matrix = color_matrix;
    }

    return reinterpret_cast<uintptr_t>(filter_data);
}

void RDRenderInterfaceGodot::ReleaseFilter(Rml::CompiledFilterHandle filter) {
    filters_to_release.push_back(reinterpret_cast<FilterData *>(filter));
}

Rml::CompiledShaderHandle RDRenderInterfaceGodot::CompileShader(const Rml::String& name, const Rml::Dictionary& parameters) {
    ShaderData *shader_data = memnew(ShaderData);

    if (name == "linear-gradient" || name == "radial-gradient" || name == "conic-gradient") {
        if (name == "linear-gradient") {
            shader_data->name = "Linear Gradient";
        } else if (name == "radial-gradient") {
            shader_data->name = "Radial Gradient";
        } else if (name == "conic-gradient") {
            shader_data->name = "Conic Gradient";
        }
        
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

        shader_data->pipeline = pipeline_shader_gradient;

        struct {
            float transform[16];
            float inv_viewport_size[2];
            unsigned int flags;
            float pad0;

            float p[2];
            float v[2];

            unsigned int type;
            unsigned int stop_count;

            float pad2[2];
        } push_const;

        push_const.p[0] = p.x;
        push_const.p[1] = p.y;
        push_const.v[0] = v.x;
        push_const.v[1] = v.y;
        push_const.type = (grad_type << 1) | (repeating ? 1 : 0);
        push_const.stop_count = stop_count;

        shader_data->push_const.resize(sizeof(push_const));
        memcpy(shader_data->push_const.ptrw(), &push_const, sizeof(push_const));

        shader_data->uniform_buffer = rendering_resources.create_storage_buffer({
            {"data", storage_buffer}
        });
    }

    return reinterpret_cast<uintptr_t>(shader_data);
}

void RDRenderInterfaceGodot::RenderShader(Rml::CompiledShaderHandle shader, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
    DrawCommand *c = push_draw_command();
	c->type = DrawCommand::TYPE_RENDER_SHADER;
    c->render_shader.shader_id = shader;
	c->render_shader.geometry_id = geometry;
	c->render_shader.texture_id = texture;
	c->render_shader.translation = Vector2(translation.x, translation.y);
}

void RDRenderInterfaceGodot::ReleaseShader(Rml::CompiledShaderHandle shader) {
    shaders_to_release.push_back(reinterpret_cast<ShaderData *>(shader));
}
