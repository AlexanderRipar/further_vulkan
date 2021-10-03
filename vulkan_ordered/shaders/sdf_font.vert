#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 atlas_pos;
layout(location = 1) in vec2 screen_pos;

layout(location = 0) out vec2 tex_pos;

layout(push_constant) uniform Push_data
{
	mat4 transform;
} push_data;

void main()
{
	gl_Position = vec4(screen_pos, 0.0, 1.0) * push_data.transform;

	tex_pos = atlas_pos;
}
