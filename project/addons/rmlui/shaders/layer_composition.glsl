#[vertex]
#version 450 core

vec2 uvs[3] = vec2[](
	vec2(0, 0), vec2(2, 0), vec2(0, 2)
);

void main() {
	vec2 uv = uvs[gl_VertexIndex];
	gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}

#[fragment]
#version 450 core

#include "common.glsl.inc"

layout(set = 0, binding = 0) uniform sampler2D layer0;
layout(set = 0, binding = 1) uniform sampler2D layer1;

layout(location = 0) out vec4 o_color;

#define MODE_BLEND 0
#define MODE_REPLACE 1

layout(push_constant, std430) uniform GeometryData {
	uint mode;
} params;

void main() {
    vec4 color0 = texelFetch(layer0, ivec2(gl_FragCoord.xy), 0);
	vec4 color1 = texelFetch(layer1, ivec2(gl_FragCoord.xy), 0);

	o_color = params.mode == MODE_BLEND ? (
		blend_mix(color0, color1)
	) : (
		color1
	);
}