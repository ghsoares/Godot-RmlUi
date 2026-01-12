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

layout(set = 0, binding = 0) uniform sampler2D screen;
layout(set = 0, binding = 1) uniform sampler2D shadow;

layout(location = 0) out vec4 o_color;

layout(push_constant, std430) uniform FilterParams {
	vec4 color;
} params;

vec4 blend_mix(in vec4 p_dst, in vec4 p_src) {
	return vec4(
		mix(p_dst.rgb, p_src.rgb, p_src.a),
		p_src.a + p_dst.a * (1 - p_src.a)
	);
}

void main() {
    o_color = params.color;
	o_color.a *= texelFetch(shadow, ivec2(gl_FragCoord.xy), 0).a;
	vec4 pix = texelFetch(screen, ivec2(gl_FragCoord.xy), 0);

	o_color = o_color.a < 1.0 / 256.0 ? pix : blend_mix(o_color, pix);
}