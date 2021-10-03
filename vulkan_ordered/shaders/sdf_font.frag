#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 tex_position;

layout(binding = 0) uniform sampler2D tex_sampler;

layout(location = 0) out vec4 out_colour;

void main()
{
	vec4 sampled_colour = texture(tex_sampler, tex_position);

	if(sampled_colour.r >= 0.5)
		out_colour = vec4(0.0, 0.0, 0.0, 1.0);
	else
		discard;
}
