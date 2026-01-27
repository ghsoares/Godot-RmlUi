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

layout(push_constant, std430) uniform FilterParams {
	vec2 dir;
	vec2 off;
	float sigma;
} params;

void main() {
	vec2 tex_size = textureSize(source, 0);
	vec2 inv_tex_size = 1.0 / tex_size;
	vec2 uv = (gl_FragCoord.xy - params.off) * inv_tex_size;
	vec2 dir = params.dir * inv_tex_size;
	float inv_sig = 1.0 / params.sigma;

	vec3 color = vec3(0.0);
	float alpha = 0.0;
	
	float color_weight = 0.0;
	float alpha_weight = 0.0;

	for (float t = -params.sigma; t <= params.sigma; t += 1.0) {
		float w = 1.0 - pow(t * inv_sig, 2.0);

		vec2 new_uv = uv + dir * t;
		bool outside = new_uv.x < 0.0 || new_uv.x > 1.0 || new_uv.y < 0.0 || new_uv.y > 1.0;
		vec4 pix = !outside ? texture(source, uv + dir * t) : vec4(0.0);
		bool transparent = pix.a <= 0.0;

		color = transparent ? color : color + pix.rgb * w;
		alpha = alpha + pix.a * w;

		color_weight = transparent ? color_weight : color_weight + w;
		alpha_weight = alpha_weight + w;
	}

	o_color.rgb = color_weight > 0.0 ? color / color_weight : vec3(0.0);
	o_color.a = alpha_weight > 0.0 ? alpha / alpha_weight : 0.0;
}