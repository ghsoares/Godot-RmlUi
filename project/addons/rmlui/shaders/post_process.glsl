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

layout(set = 0, binding = 0) uniform sampler2D source;

layout(location = 0) out vec4 o_color;

void main() {
    o_color = texelFetch(source, ivec2(gl_FragCoord.xy), 0);

	// Un-multiply to show colors correctly
	o_color.rgb /= o_color.a > 0 ? o_color.a : 1.0;
}