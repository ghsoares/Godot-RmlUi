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

#include "../common.glsl.inc"

layout(set = 0, binding = 0) uniform sampler2D source;

layout(location = 0) out vec4 o_color;

#define SPACE_RGB 0
#define SPACE_HSV 1

layout(push_constant, std430) uniform FilterParams {
	vec4 modulate;
	uint space;
} params;

void main() {
    o_color = texelFetch(source, ivec2(gl_FragCoord.xy), 0);

	o_color = params.space == SPACE_HSV ? hue_rotate(o_color, params.modulate.r) : o_color * params.modulate;
}