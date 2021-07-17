#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (local_size_x_id = 1) in;
layout (local_size_y_id = 2) in;
layout (local_size_z_id = 3) in;

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0, rgba8) uniform writeonly image2D image;

layout(push_constant) uniform Push_data
{
	vec4 colour;
} push_data;

void main()
{
	ivec2 image_sz = imageSize(image);

	if (image_sz.x > gl_GlobalInvocationID.x && image_sz.y > gl_GlobalInvocationID.y)
	{
		imageStore(image, ivec2(gl_GlobalInvocationID.xy), push_data.colour);
	}
}
