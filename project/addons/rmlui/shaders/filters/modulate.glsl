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

layout(location = 0) out vec4 o_color;

layout(push_constant, std430) uniform FilterParams {
	vec4 modulate;
	uint space;
} params;

vec3 rgb_to_hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv_to_rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec4 hue_rotate(vec4 c, float value) {
	return vec4(
		hsv_to_rgb(rgb_to_hsv(c.rgb) + vec3(value, 0.0, 0.0)),
		c.a
	);
}

void main() {
    o_color = texelFetch(screen, ivec2(gl_FragCoord.xy), 0);

	o_color = params.space == 1 ? hue_rotate(o_color, params.modulate.r) : o_color * params.modulate;
}