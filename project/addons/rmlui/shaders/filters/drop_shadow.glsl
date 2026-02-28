#[vertex]
#version 450 core

layout(location = 0) out vec2 o_uv;

vec2 uvs[3] = vec2[](
	vec2(0, 0), vec2(2, 0), vec2(0, 2)
);

void main() {
	vec2 uv = uvs[gl_VertexIndex];
	gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    o_uv = uv;
}

#[fragment]
#version 450 core

#include "../common.glsl.inc"

layout(set = 0, binding = 0) uniform sampler2D shadow;
layout(set = 0, binding = 1) uniform sampler2D source;

layout(location = 0) in vec2 i_uv;

layout(location = 0) out vec4 o_color;

layout(push_constant, std430) uniform FilterParams {
	vec4 color;
	vec2 offset;
	float lvl;
	float pad0;
} params;

void main() {
#ifdef SAMPLE_BICUBIC
	float shadow_alpha = textureBicubic(shadow, i_uv - params.offset, params.lvl).a;
#else
	float shadow_alpha = textureLod(shadow, i_uv - params.offset, params.lvl).a;
#endif
	o_color = texelFetch(source, ivec2(gl_FragCoord.xy), 0);
	o_color = blend_mix(params.color * vec4(1, 1, 1, shadow_alpha), o_color);
	// o_color = texelFetch(source, ivec2(gl_FragCoord.xy), 0);
	// o_color = blend_mix(shadow_color, o_color);
}