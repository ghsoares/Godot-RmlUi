#[vertex]
#version 450 core

layout(location = 0) in vec2 i_vertex_position;
layout(location = 1) in vec4 i_vertex_color;
layout(location = 2) in vec2 i_vertex_uv;

layout(location = 0) out vec2 o_uv;
layout(location = 1) out vec4 o_color;

layout(push_constant, std430) uniform GeometryData {
	vec2 inv_viewport_size;
	mat4 transform;
	bool clip_mask_enabled;
} geometry_data;

void main() {
	vec2 pos = i_vertex_position;
	pos = (geometry_data.transform * vec4(pos, 0.0, 1.0)).xy;

	vec2 screen_uv = pos * geometry_data.inv_viewport_size;

	gl_Position = vec4(screen_uv * 2.0 - 1.0, 0.0, 1.0);

	o_uv = i_vertex_uv;
	o_color = i_vertex_color;
}

#[fragment]
#version 450 core

layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec4 i_color;

layout(location = 0) out vec4 o_color;

layout(push_constant, std430) uniform GeometryData {
	vec2 inv_viewport_size;
	mat4 transform;
	bool clip_mask_enabled;
} geometry_data;

layout(set = 0, binding = 0) uniform sampler2D screen;
layout(set = 0, binding = 1) uniform sampler2D tex;

vec4 blend_mix(in vec4 p_dst, in vec4 p_src) {
	return vec4(
		mix(p_dst.rgb, p_src.rgb, p_src.a),
		p_src.a + p_dst.a * (1 - p_src.a)
	);
}

void main() {
	o_color = texelFetch(screen, ivec2(gl_FragCoord.xy), 0);

	vec4 pix_color = vec4(vec3(1.0), texture(tex, i_uv).a) * i_color;

	// Custom alpha blending function, when this pixel is being rendered by the first time,
	// sets it's color equal to the geometry color, else blend with current color
	o_color = o_color.a <= 1.0 / 256.0 ? pix_color : blend_mix(o_color, pix_color);
}