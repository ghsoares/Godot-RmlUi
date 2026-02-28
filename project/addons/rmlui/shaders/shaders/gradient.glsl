#[vertex]
#version 450 core

#include "../common.glsl.inc"

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

	vec2 p;
	vec2 v;
	
	uint type;
	uint stop_count;
	float pad1[2];
} geometry_data;

void main() {
	vec2 pos = i_vertex_position;
	pos = (geometry_data.transform * vec4(pos, 0.0, 1.0)).xy * geometry_data.inv_viewport_size;

	gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);

	o_uv = i_vertex_uv;
	o_color = i_vertex_color;

	o_color.rgb = bool(geometry_data.flags & FLAGS_CONVERT_SRGB_TO_LINEAR) ? srgb_to_linear(o_color.rgb) : o_color.rgb;
}

#[fragment]
#version 450 core

#include "../common.glsl.inc"

layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec4 i_color;

layout(location = 0) out vec4 o_color;

#define TYPE_NONE 0
#define TYPE_LINEAR_GRADIENT 1
#define TYPE_RADIAL_GRADIENT 2
#define TYPE_CONIC_GRADIENT 3

#define PI 3.14159265359
#define TAU 6.28318530718
#define INV_TAU 0.15915494309

layout(push_constant, std430) uniform GeometryData {
	mat4 transform;
	vec2 inv_viewport_size;
	uint flags;
	float pad0;

	vec2 p;
	vec2 v;
	
	uint type;
	uint stop_count;
	float pad1[2];
} geometry_data;


layout(std430, set = 0, binding = 1) buffer GradientBuffer {
	// List of rgba + position
    float values[];
} gradient_buffer;

vec4 mix_stop_colors(float t) {
	vec4 color = vec4(
		gradient_buffer.values[0],
		gradient_buffer.values[1],
		gradient_buffer.values[2],
		gradient_buffer.values[3]
	);

	uint count = geometry_data.stop_count;
	for (uint i = 1; i < count; i++) {
		vec4 c = vec4(
			gradient_buffer.values[i * 5 + 0],
			gradient_buffer.values[i * 5 + 1],
			gradient_buffer.values[i * 5 + 2],
			gradient_buffer.values[i * 5 + 3]
		);
		float s0 = gradient_buffer.values[(i - 1) * 5 + 4];
		float s1 = gradient_buffer.values[i * 5 + 4];

		color = mix(color, c, smoothstep(s0, s1, t));
	}

	color.rgb = bool(geometry_data.flags & FLAGS_CONVERT_SRGB_TO_LINEAR) ? srgb_to_linear(color.rgb) : color.rgb;

	return color;
}

void main() {
	float t = 0.0;

	uint type = geometry_data.type >> 1;
	bool repeating = (geometry_data.type & 1) > 0;

	if (type == TYPE_LINEAR_GRADIENT) {
		float dsq = dot(geometry_data.v, geometry_data.v);
		vec2 v = i_uv - geometry_data.p;
		t = dot(geometry_data.v, v) / dsq;
	} else if (type == TYPE_RADIAL_GRADIENT) {
		vec2 v = i_uv - geometry_data.p;
		t = length(geometry_data.v * v);
	} else if (type == TYPE_CONIC_GRADIENT) {
		mat2 r = mat2(geometry_data.v.x, -geometry_data.v.y, geometry_data.v.y, geometry_data.v.x);
		vec2 v = r * (i_uv - geometry_data.p);
		t = 0.5 + atan(-v.x, v.y) * INV_TAU;
	}

	if (repeating) {
		float t0 = gradient_buffer.values[4];
		float t1 = gradient_buffer.values[geometry_data.stop_count * 5 - 1];
		t = t0 + mod(t - t0, t1 - t0);
	}

	o_color = i_color * mix_stop_colors(t);
}