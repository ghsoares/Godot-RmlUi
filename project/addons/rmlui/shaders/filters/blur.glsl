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

layout(set = 0, binding = 0) uniform sampler2D source;

layout(location = 0) in vec2 i_uv;

layout(location = 0) out vec4 o_color;

layout(push_constant, std430) uniform BlurParms {
	float lvl;
	float pad0[3];
} params;

#define SAMPLE_BICUBIC

void main() {
#ifdef SAMPLE_BICUBIC
	o_color = textureBicubic(source, i_uv, params.lvl);
#else
	o_color = textureLod(source, i_uv, params.lvl);
#endif
}