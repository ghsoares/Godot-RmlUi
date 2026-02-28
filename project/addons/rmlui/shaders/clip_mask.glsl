#[vertex]
#version 450 core

layout(location = 0) in vec2 i_vertex_position;

layout(push_constant, std430) uniform GeometryData {
	mat4 transform;
	vec2 inv_viewport_size;
	float pad0[2];
} geometry_data;

void main() {
	vec2 pos = i_vertex_position;
	pos = (geometry_data.transform * vec4(pos, 0.0, 1.0)).xy;

	vec2 screen_uv = pos * geometry_data.inv_viewport_size;

	gl_Position = vec4(screen_uv * 2.0 - 1.0, 0.0, 1.0);
}

#[fragment]
#version 450 core

void main() { }