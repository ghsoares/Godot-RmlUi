#include "rendering_utils.h"

#include <iostream>

#include <godot_cpp/classes/resource_loader.hpp>
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

using namespace godot;
using RD = RenderingDevice;

void RenderingResources::map_resource(const RID &p_rid, ResourceMap &p_map) {
	p_map.insert(p_rid);
}

void RenderingResources::free_resource(const RID &p_rid, ResourceMap &p_map) {
	bool has_rid = p_map.find(p_rid) != p_map.end();
	ERR_FAIL_COND_MSG(!has_rid, vformat("Has no resource of type '%s' with %s", p_map.resource_name, p_rid));
	rendering_device->free_rid(p_rid);
	p_map.erase(p_rid);
}

void RenderingResources::free_all_resources(ResourceMap &p_map) {
	for (auto it : p_map) {
		rendering_device->free_rid(it);
	}
	p_map.clear();
}

void RenderingResources::free_all_resources() {
	// Must follow a order to be able to free the resources correctly
	free_all_resources(framebuffer_map);
	free_all_resources(pipeline_map);
	free_all_resources(shader_map);
	free_all_resources(sampler_map);
	free_all_resources(texture_map);
	free_all_resources(vertex_array_map);
	free_all_resources(index_array_map);
	free_all_resources(vertex_buffer_map);
	free_all_resources(index_buffer_map);
}

#define IMPLEMENT_RENDERING_RESOURCE(p_name) \
void RenderingResources::free_##p_name(const RID &p_rid) { free_resource(p_rid, p_name ##_map); }

IMPLEMENT_RENDERING_RESOURCE(sampler)
IMPLEMENT_RENDERING_RESOURCE(texture)
IMPLEMENT_RENDERING_RESOURCE(framebuffer)
IMPLEMENT_RENDERING_RESOURCE(shader)
IMPLEMENT_RENDERING_RESOURCE(pipeline)
IMPLEMENT_RENDERING_RESOURCE(vertex_buffer)
IMPLEMENT_RENDERING_RESOURCE(index_buffer)
IMPLEMENT_RENDERING_RESOURCE(vertex_array)
IMPLEMENT_RENDERING_RESOURCE(index_array)

#undef IMPLEMENT_RENDERING_RESOURCE

Variant map_get(const std::map<String, Variant> &p_map, const String &p_key, const Variant &p_default) {
	auto it = p_map.find(p_key);
	if (it == p_map.end()) {
		return p_default;
	}
	return it->second;
}

Variant map_get2(const std::map<String, Variant> &p_map, const String &p_key0, const String &p_key1, const Variant &p_default) {
	auto it = p_map.find(p_key0);
	if (it == p_map.end()) {
		it = p_map.find(p_key1);
	}
	if (it == p_map.end()) {
		return p_default;
	}
	return it->second;
}

RID RenderingResources::create_sampler(const std::map<String, Variant> &p_data) {
	Ref<RDSamplerState> sampler_state;
    sampler_state.instantiate();

	sampler_state->set_min_filter((RD::SamplerFilter)(int)map_get(p_data, "min_filter", RD::SAMPLER_FILTER_NEAREST));
    sampler_state->set_mag_filter((RD::SamplerFilter)(int)map_get(p_data, "mag_filter", RD::SAMPLER_FILTER_NEAREST));
    sampler_state->set_repeat_u((RD::SamplerRepeatMode)(int)map_get(p_data, "repeat_u", RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE));
    sampler_state->set_repeat_v((RD::SamplerRepeatMode)(int)map_get(p_data, "repeat_v", RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE));
    sampler_state->set_repeat_w((RD::SamplerRepeatMode)(int)map_get(p_data, "repeat_w", RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE));

	RID rid = rendering_device->sampler_create(sampler_state);
	ERR_FAIL_COND_V(!rid.is_valid(), RID());

	map_resource(rid, sampler_map);
	return rid;
}

RID RenderingResources::create_texture(const std::map<String, Variant> &p_data) {
	Ref<RDTextureFormat> tex_format;
    Ref<RDTextureView> tex_view;
    tex_format.instantiate();
    tex_view.instantiate();
    tex_format->set_texture_type((RD::TextureType)(int)map_get(p_data, "texture_type", RD::TEXTURE_TYPE_2D));
    tex_format->set_width(map_get(p_data, "width", 1));
    tex_format->set_height(map_get(p_data, "height", 1));
    tex_format->set_format((RD::DataFormat)(int)map_get(p_data, "format", RD::DATA_FORMAT_R8G8B8A8_UNORM));
    tex_format->set_usage_bits((uint64_t)map_get(p_data, "usage_bits", RD::TEXTURE_USAGE_SAMPLING_BIT));

	TypedArray<PackedByteArray> data = map_get(p_data, "data", TypedArray<PackedByteArray>());
	RID rid = rendering_device->texture_create(
		tex_format,
		tex_view, 
		data
	);
	ERR_FAIL_COND_V(!rid.is_valid(), RID());

	map_resource(rid, texture_map);
	return rid;
}

RID RenderingResources::create_framebuffer(const std::map<String, Variant> &p_data) {
	TypedArray<RID> textures = map_get(p_data, "textures", TypedArray<RID>());

	RID rid = rendering_device->framebuffer_create(textures);
	ERR_FAIL_COND_V(!rid.is_valid(), RID());

	map_resource(rid, framebuffer_map);
	return rid;
}

RID RenderingResources::create_shader(const std::map<String, Variant> &p_data) {
    Ref<RDShaderFile> shader_file = ResourceLoader::get_singleton()->load(map_get(p_data, "path", ""));
    ERR_FAIL_COND_V(!shader_file.is_valid(), RID());

    Ref<RDShaderSPIRV> shader_spirv = shader_file->get_spirv();
    ERR_FAIL_COND_V(!shader_spirv.is_valid(), RID());

    RID rid = rendering_device->shader_create_from_spirv(shader_spirv, map_get(p_data, "name", ""));
    ERR_FAIL_COND_V(!rid.is_valid(), RID());

   	map_resource(rid, shader_map);
	return rid;
}

RID RenderingResources::create_pipeline(const std::map<String, Variant> &p_data) {
	Ref<RDPipelineRasterizationState> rasterization_state;
    Ref<RDPipelineMultisampleState> multisample_state;
    Ref<RDPipelineDepthStencilState> depth_stencil_state;
    Ref<RDPipelineColorBlendState> color_blend_state;
    rasterization_state.instantiate();
    multisample_state.instantiate();
    depth_stencil_state.instantiate();
    color_blend_state.instantiate();

	depth_stencil_state->set_enable_stencil(map_get(p_data, "enable_stencil", false));

	depth_stencil_state->set_back_op_reference(map_get2(p_data, "back_op_ref", "op_ref", 0));
	depth_stencil_state->set_back_op_write_mask(map_get2(p_data, "back_op_write_mask", "op_write_mask", 0));
	depth_stencil_state->set_back_op_compare((RD::CompareOperator)(int)map_get2(p_data, "back_op_compare", "op_compare", RD::COMPARE_OP_ALWAYS));
	depth_stencil_state->set_back_op_compare_mask(map_get2(p_data, "back_op_compare_mask", "op_compare_mask", 0));
	depth_stencil_state->set_back_op_pass((RD::StencilOperation)(int)map_get2(p_data, "back_op_pass", "op_pass", RD::STENCIL_OP_ZERO));
	depth_stencil_state->set_back_op_fail((RD::StencilOperation)(int)map_get2(p_data, "back_op_fail", "op_fail", RD::STENCIL_OP_ZERO));

	depth_stencil_state->set_front_op_reference(map_get2(p_data, "front_op_ref", "op_ref", 0));
	depth_stencil_state->set_front_op_write_mask(map_get2(p_data, "front_op_write_mask", "op_write_mask", 0));
	depth_stencil_state->set_front_op_compare((RD::CompareOperator)(int)map_get2(p_data, "front_op_compare", "op_compare", RD::COMPARE_OP_ALWAYS));
	depth_stencil_state->set_front_op_compare_mask(map_get2(p_data, "front_op_compare_mask", "op_compare_mask", 0));
	depth_stencil_state->set_front_op_pass((RD::StencilOperation)(int)map_get2(p_data, "front_op_pass", "op_pass", RD::STENCIL_OP_ZERO));
	depth_stencil_state->set_front_op_fail((RD::StencilOperation)(int)map_get2(p_data, "front_op_fail", "op_fail", RD::STENCIL_OP_ZERO));

	TypedArray<Ref<RDPipelineColorBlendStateAttachment>> attachments;
	int attachment_count = map_get(p_data, "attachment_count", 1);
	for (int i = attachment_count - 1; i >= 0; i--) {
		Ref<RDPipelineColorBlendStateAttachment> attachment;
		attachment.instantiate();
		attachments.append(attachment);
	}
    color_blend_state->set_attachments(attachments);

	RID rid = rendering_device->render_pipeline_create(
        map_get(p_data, "shader", RID()), 
		map_get(p_data, "framebuffer_format", -1),
        map_get(p_data, "vertex_format", -1),
       	(RD::RenderPrimitive)(int)map_get(p_data, "primitive", RD::RENDER_PRIMITIVE_TRIANGLES),
        rasterization_state,
        multisample_state,
        depth_stencil_state,
        color_blend_state
    );
	ERR_FAIL_COND_V(!rid.is_valid(), RID());

   	map_resource(rid, pipeline_map);
	return rid;
}

RID RenderingResources::create_vertex_buffer(const std::map<String, Variant> &p_data) {
	PackedByteArray data = (PackedByteArray)map_get(p_data, "data", PackedByteArray());

	RID rid = rendering_device->vertex_buffer_create(data.size(), data);
	ERR_FAIL_COND_V(!rid.is_valid(), RID());

   	map_resource(rid, vertex_buffer_map);
	return rid;
}

RID RenderingResources::create_index_buffer(const std::map<String, Variant> &p_data) {
	PackedByteArray data = (PackedByteArray)map_get(p_data, "data", PackedByteArray());

	RID rid = rendering_device->index_buffer_create(
		map_get(p_data, "count",0), 
		(RD::IndexBufferFormat)(int)map_get(p_data, "format", RD::INDEX_BUFFER_FORMAT_UINT32),
		data
	);
	ERR_FAIL_COND_V(!rid.is_valid(), RID());

   	map_resource(rid, index_buffer_map);
	return rid;
}

RID RenderingResources::create_vertex_array(const std::map<String, Variant> &p_data) {
	RID rid = rendering_device->vertex_array_create(
		map_get(p_data, "count", 0), 
		map_get(p_data, "format", -1), 
		map_get(p_data, "buffers", TypedArray<RID>())
	);
	ERR_FAIL_COND_V(!rid.is_valid(), RID());

   	map_resource(rid, vertex_array_map);
	return rid;
}

RID RenderingResources::create_index_array(const std::map<String, Variant> &p_data) {
	RID rid = rendering_device->index_array_create(
		map_get(p_data, "buffer", RID()), 
		map_get(p_data, "offset", 0), 
		map_get(p_data, "count", 0)
	);
	ERR_FAIL_COND_V(!rid.is_valid(), RID());

   	map_resource(rid, index_array_map);
	return rid;
}

RenderingResources::RenderingResources(RD *p_rendering_device) {
	rendering_device = p_rendering_device;
}

RenderingResources::~RenderingResources() {
	free_all_resources();
}
