#[vertex]
#version 450 core

#include "common.glsl.inc"

layout(location = 0) in vec2 i_vertex_position;
layout(location = 1) in vec4 i_vertex_color;
layout(location = 2) in vec2 i_vertex_uv;

layout(location = 0) out vec2 o_uv;
layout(location = 1) out vec4 o_color;

layout(push_constant, std430) uniform GeometryData {
	mat4 transform;
	vec2 inv_viewport_size;
	uint flags;
	float pad0;
} geometry_data;

void main() {
	vec2 pos = i_vertex_position;
	pos = (geometry_data.transform * vec4(pos, 0.0, 1.0)).xy;

	vec2 screen_uv = pos * geometry_data.inv_viewport_size;

	gl_Position = vec4(screen_uv * 2.0 - 1.0, 0.0, 1.0);

	o_uv = i_vertex_uv;
	o_color = i_vertex_color;

	o_color.rgb = bool(geometry_data.flags & FLAGS_CONVERT_SRGB_TO_LINEAR) ? srgb_to_linear(o_color.rgb) : o_color.rgb;
}

#[fragment]
#version 450 core

layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec4 i_color;

layout(location = 0) out vec4 o_color;

layout(set = 0, binding = 0) uniform sampler2D albedo_tex;

void main() {
	o_color = texture(albedo_tex, i_uv) * i_color;
}