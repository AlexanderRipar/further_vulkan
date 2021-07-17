#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (local_size_x_id = 1) in;
layout (local_size_y_id = 2) in;
layout (local_size_z_id = 3) in;

layout (local_size_x = 128) in;

layout (set = 0, binding = 0) buffer Dst_buf {
	uint elems[];
} dst_buf;

layout (set = 0, binding = 1) buffer Src_buf {
	uint elems[];
} src_buf;

void main()
{
	dst_buf.elems[gl_GlobalInvocationID.x] = src_buf.elems[gl_GlobalInvocationID.x];
}
