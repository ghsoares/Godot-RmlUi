#[vertex]
#version 450 core

vec2 uvs[3] = vec2[](
	vec2(0, 0), vec2(2, 0), vec2(0, 2)
);

layout(location = 0) out vec2 o_uv;

void main() {
	vec2 uv = uvs[gl_VertexIndex];
	gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
	o_uv = uv;
}

#[fragment]
#version 450 core

layout(set = 0, binding = 0) uniform sampler2D source;

layout(location = 0) in vec2 i_uv;

layout(location = 0) out vec4 o_color;

layout(push_constant, std430) uniform BlurData {
	vec2 dst_pixel_size;
	float pad0[2];
} blur;

#define GAUSSIAN_BLUR

void main() {
#ifdef GAUSSIAN_BLUR
	vec4 A = texture(source, i_uv + blur.dst_pixel_size * vec2(-1.0, -1.0));
	vec4 B = texture(source, i_uv + blur.dst_pixel_size * vec2(0.0, -1.0));
	vec4 C = texture(source, i_uv + blur.dst_pixel_size * vec2(1.0, -1.0));
	vec4 D = texture(source, i_uv + blur.dst_pixel_size * vec2(-0.5, -0.5));
	vec4 E = texture(source, i_uv + blur.dst_pixel_size * vec2(0.5, -0.5));
	vec4 F = texture(source, i_uv + blur.dst_pixel_size * vec2(-1.0, 0.0));
	vec4 G = texture(source, i_uv);
	vec4 H = texture(source, i_uv + blur.dst_pixel_size * vec2(1.0, 0.0));
	vec4 I = texture(source, i_uv + blur.dst_pixel_size * vec2(-0.5, 0.5));
	vec4 J = texture(source, i_uv + blur.dst_pixel_size * vec2(0.5, 0.5));
	vec4 K = texture(source, i_uv + blur.dst_pixel_size * vec2(-1.0, 1.0));
	vec4 L = texture(source, i_uv + blur.dst_pixel_size * vec2(0.0, 1.0));
	vec4 M = texture(source, i_uv + blur.dst_pixel_size * vec2(1.0, 1.0));

	// Pre-multiply alpha, neccessary to avoid dark pixels bleeding
	A.rgb *= A.a;
	B.rgb *= B.a;
	C.rgb *= C.a;
	D.rgb *= D.a;
	E.rgb *= E.a;
	F.rgb *= F.a;
	G.rgb *= G.a;
	H.rgb *= H.a;
	I.rgb *= I.a;
	J.rgb *= J.a;
	K.rgb *= K.a;
	L.rgb *= L.a;
	M.rgb *= M.a;

	float base_weight = 0.5 / 4.0;
	float lesser_weight = 0.125 / 4.0;

	o_color = (D + E + I + J) * base_weight;
	o_color += (A + B + G + F) * lesser_weight;
	o_color += (B + C + H + G) * lesser_weight;
	o_color += (F + G + L + K) * lesser_weight;
	o_color += (G + H + M + L) * lesser_weight;

#else
	vec4 A = texture(source, i_uv + blur.dst_pixel_size * vec2(-0.5, -0.5));
	vec4 B = texture(source, i_uv + blur.dst_pixel_size * vec2(0.5, -0.5));
	vec4 C = texture(source, i_uv + blur.dst_pixel_size * vec2(0.5, 0.5));
	vec4 D = texture(source, i_uv + blur.dst_pixel_size * vec2(-0.5, 0.5));

	// Pre-multiply alpha, neccessary to avoid dark pixels bleeding
	A.rgb *= A.a;
	B.rgb *= B.a;
	C.rgb *= C.a;
	D.rgb *= D.a;

	o_color = (A + B + C + D) * 0.25;
#endif

	// Un-multiply alpha
	o_color.rgb /= o_color.a > 0.0 ? o_color.a : 1.0;
}