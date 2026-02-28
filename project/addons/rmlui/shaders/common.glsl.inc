
#define FLAGS_CONVERT_SRGB_TO_LINEAR (1 << 0)

vec3 srgb_to_linear(vec3 color) {
	return mix(pow((color.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), color.rgb * (1.0 / 12.92), lessThan(color.rgb, vec3(0.04045)));
}

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

vec4 blend_mix(in vec4 p_dst, in vec4 p_src) {
	return vec4(
		mix(p_dst.rgb, p_src.rgb, p_src.a),
		p_src.a + p_dst.a * (1 - p_src.a)
	);
}

// from http://www.java-gaming.org/index.php?topic=35123.0
vec4 cubic(float v){
    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
    vec4 s = n * n * n;
    float x = s.x;
    float y = s.y - 4.0 * s.x;
    float z = s.z - 4.0 * s.y + 6.0 * s.x;
    float w = 6.0 - x - y - z;
    return vec4(x, y, z, w) * (1.0/6.0);
}

vec4 textureBicubicLod(sampler2D p_tex, vec2 p_uv, int p_lod) {
	vec2 pix_size = textureSize(p_tex, p_lod);
	vec2 inv_pix_size = 1.0 / pix_size;
	
	vec2 tex_coords = p_uv * pix_size - 0.5;
   
    vec2 fxy = fract(tex_coords);
    tex_coords -= fxy;

    vec4 xcubic = cubic(fxy.x);
    vec4 ycubic = cubic(fxy.y);

    vec4 c = tex_coords.xxyy + vec2 (-0.5, +1.5).xyxy;
    
    vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
    vec4 offset = c + vec4 (xcubic.yw, ycubic.yw) / s;
    
    offset *= inv_pix_size.xxyy;
    
    vec4 sample0 = textureLod(p_tex, offset.xz, float(p_lod));
    vec4 sample1 = textureLod(p_tex, offset.yz, float(p_lod));
    vec4 sample2 = textureLod(p_tex, offset.xw, float(p_lod));
    vec4 sample3 = textureLod(p_tex, offset.yw, float(p_lod));

    float sx = s.x / (s.x + s.y);
    float sy = s.z / (s.z + s.w);

    return mix(
       mix(sample3, sample2, sx), mix(sample1, sample0, sx)
    , sy);
}

vec4 textureBicubic(sampler2D p_tex, vec2 p_uv, float p_lod) {
	int lvl0 = int(floor(p_lod));
	p_lod -= lvl0;

	vec4 c0 = textureBicubicLod(p_tex, p_uv, lvl0);
	vec4 c1 = textureBicubicLod(p_tex, p_uv, lvl0 + 1);
	return mix(c0, c1, p_lod);
}
