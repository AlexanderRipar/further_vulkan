#version 450

layout(local_size_x_id = 1) in;
layout(local_size_y_id = 2) in;
layout(local_size_z_id = 3) in;

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0, rgba8) uniform writeonly image2D image;

layout(push_constant) uniform Push_data
{
	vec4 offset_xyz_scale_w;
} push_data;



float d_dot_with_hashed_vec(float i, float j, float k, float x, float y, float z)
{
	uint h = (floatBitsToUint(i) * 73856093u) ^ (floatBitsToUint(j) * 19349663u) ^ (floatBitsToUint(k) * 83492791u);

	//Two masks, which are either 0.0F or -0.0F, depending on positional hash
	uint neg1 = h & 0x80000000u;
	uint neg2 = (h & 0x10000000u) << 3;

	//Get hash in [0, 2]
	uint h_3 = ((h >> 4) * 3u) >> 28;

	//Decide which inputs to pick depending on h_3
	uint a, b;

	if (h_3 == 0u)
	{
		a = floatBitsToUint(y);
		b = floatBitsToUint(z);
	}
	else if (h_3 == 1u)
	{
		a = floatBitsToUint(x);
		b = floatBitsToUint(z);
	}
	else
	{
		a = floatBitsToUint(x);
		b = floatBitsToUint(y);
	}

	//Return picked inputs, either negated or not, depending on masks
	return uintBitsToFloat(a ^ neg1) + uintBitsToFloat(b ^ neg2);
}

void main()
{
	ivec2 image_sz = imageSize(image);

	if (image_sz.x <= gl_GlobalInvocationID.x || image_sz.y <= gl_GlobalInvocationID.y)
		return;

	vec3 pos = vec3(push_data.offset_xyz_scale_w.xyz) + vec3(gl_GlobalInvocationID.xyz) * push_data.offset_xyz_scale_w.w;

	const float skew_factor = 1.0 / 3.0;
	const float unskew_factor = 1.0 / 6.0;

	float skew = (pos.x + pos.y + pos.z) * skew_factor;

	float i0 = floor(pos.x + skew);
	float j0 = floor(pos.y + skew);
	float k0 = floor(pos.z + skew);

	float unskew = (i0 + j0 + k0) * unskew_factor;

	float x0 = pos.x - i0 + unskew;
	float y0 = pos.y - j0 + unskew;
	float z0 = pos.z - k0 + unskew;

	float i1 = ((x0 >= y0) && (x0 >= z0)) ? 1.0 : 0.0;    //max == x
	float j1 = ((y0 >  x0) && (y0 >= z0)) ? 1.0 : 0.0;    //max == y
	float k1 = ((z0 >  x0) && (z0 >  y0)) ? 1.0 : 0.0;    //max == z
	float i2 = ((x0 >= y0) || (x0 >= z0)) ? 1.0 : 0.0;    //min != x
	float j2 = ((y0 >  x0) || (y0 >= z0)) ? 1.0 : 0.0;    //min != y
	float k2 = ((z0 >  x0) || (z0 >  y0)) ? 1.0 : 0.0;    //min != z

	float x1 = x0 - i1 + unskew_factor;
	float y1 = y0 - j1 + unskew_factor;
	float z1 = z0 - k1 + unskew_factor;

	float x2 = x0 - i2 + unskew_factor * 2.0;
	float y2 = y0 - j2 + unskew_factor * 2.0;
	float z2 = z0 - k2 + unskew_factor * 2.0;

	float x3 = x0 - 1.0 + unskew_factor * 3.0;
	float y3 = y0 - 1.0 + unskew_factor * 3.0;
	float z3 = z0 - 1.0 + unskew_factor * 3.0;

	float t0 = 0.5 - x0 * x0 - y0 * y0 - z0 * z0;
	if (t0 < 0.0F) t0 = 0.0F;
	t0 = t0 * t0 * t0 * t0 * d_dot_with_hashed_vec(i0, j0, k0, x0, y0, z0);

	float t1 = 0.5 - x1 * x1 - y1 * y1 - z1 * z1;
	if (t1 < 0.0) t1 = 0.0;
	t1 = t1 * t1 * t1 * t1 * d_dot_with_hashed_vec(i1 + i0, j1 + j0, k1 + k0, x1, y1, z1);

	float t2 = 0.5 - x2 * x2 - y2 * y2 - z2 * z2;
	if (t2 < 0.0) t2 = 0.0;
	t2 = t2 * t2 * t2 * t2 * d_dot_with_hashed_vec(i2 + i0, j2 + j0, k2 + k0, x2, y2, z2);

	float t3 = 0.5 - x3 * x3 - y3 * y3 - z3 * z3;
	if (t3 < 0.0) t3 = 0.0;
	t3 = t3 * t3 * t3 * t3 * d_dot_with_hashed_vec(1.0F + i0, 1.0F + j0, 1.0F + k0, x3, y3, z3);

	float rst = 38.0 * (t0 + t1 + t2 + t3) + 0.5;

	vec4 rst_vec = vec4(rst, rst, rst, 1.0);

	imageStore(image, ivec2(gl_GlobalInvocationID.xy), rst_vec);

	/*
	const float skew_factor = 1.0 / 3.0;

	const float unskew_factor = 1.0 / 6.0;

	float skew = (pos.x + pos.y + pos.z) * skew_factor;

	vec3 d0 = floor(pos + skew.xxx);

	float unskew = (d0.x + d0.y + d0.z) * unskew_factor;

	vec3 c0 = pos - d0 + unskew;
	
	vec3 d1 = vec3(((c0.x >= c0.y) && (c0.x >= c0.z)) ? 1.0 : 0.0, ((c0.y > c0.x) && (c0.y >= c0.z)) ? 1.0 : 0.0, ((c0.z > c0.x) && (c0.z > c0.y)) ? 1.0 : 0.0);

	vec3 d2 = vec3(((c0.x >= c0.y) || (c0.x >= c0.z)) ? 1.0 : 0.0, ((c0.y > c0.x) || (c0.y >= c0.z)) ? 1.0 : 0.0, ((c0.z > c0.x) || (c0.z > c0.y)) ? 1.0 : 0.0);

	vec3 c1 = c0 - d1 + unskew.xxx;

	vec3 c2 = c0 - d2 + unskew.xxx * 2.0;

	vec3 c3 = c0 - vec3(1.0) + unskew.xxx * 3.0;

	float t0 = 0.5 - c0.x * c0.x - c0.y * c0.y - c0.z * c0.z;
	if (t0 < 0.0) t0 = 0.0;
	t0 = t0 * t0 * t0 * t0 * dot_with_hashed_vec(d0     , c0);

	float t1 = 0.5 - c1.x * c1.x - c1.y * c1.y - c1.z * c1.z;
	if (t1 < 0.0) t1 = 0.0;
	t1 = t1 * t1 * t1 * t1 * dot_with_hashed_vec(d0 + d1, c1);

	float t2 = 0.5 - c2.x * c2.x - c2.y * c2.y - c2.z * c2.z;
	if (t2 < 0.0) t2 = 0.0;
	t2 = t2 * t2 * t2 * t2 * dot_with_hashed_vec(d0 + d2, c2);

	float t3 = 0.5 - c3.x * c3.x - c3.y * c3.y - c3.z * c3.z;
	if (t3 < 0.0) t3 = 0.0;
	t3 = t3 * t3 * t3 * t3 * dot_with_hashed_vec(d0 + 1.0.xxx, c3);

	const float normalization_fct = 76.0 / 2.0;

	float result = normalization_fct * (t0 + t1 + t2 + t3) + 0.5;
	*/
}

/*
__global__ void d_simplex_3d_float(cudaPitchedPtr dst, uint3 dim, float3 begin, float3 step, uint32_t seed)
{
	const uint32_t idx_x = blockIdx.x * blockDim.x + threadIdx.x;
	const uint32_t idx_y = blockIdx.y * blockDim.y + threadIdx.y;
	const uint32_t idx_z = blockIdx.z * blockDim.z + threadIdx.z;

	if (idx_x >= dim.x || idx_y >= dim.y || idx_z >= dim.z)
		return;

	const float x_in = begin.x + step.x * idx_x;
	const float y_in = begin.y + step.y * idx_y;
	const float z_in = begin.z + step.z * idx_z;

	//Begin algorithm

	constexpr float skew_factor = 1.0F / 3.0F;
	constexpr float unskew_factor = 1.0F / 6.0F;

	const float skew = (x_in + y_in + z_in) * skew_factor;

	const float i0 = floorf(x_in + skew);
	const float j0 = floorf(y_in + skew);
	const float k0 = floorf(z_in + skew);

	const float unskew = (i0 + j0 + k0) * unskew_factor;

	const float x0 = x_in - i0 + unskew;
	const float y0 = y_in - j0 + unskew;
	const float z0 = z_in - k0 + unskew;

	const float i1 = ((x0 >= y0) & (x0 >= z0)) ? 1.0F : 0.0F;    //max == x
	const float j1 = ((y0 >  x0) & (y0 >= z0)) ? 1.0F : 0.0F;    //max == y
	const float k1 = ((z0 >  x0) & (z0 >  y0)) ? 1.0F : 0.0F;    //max == z
	const float i2 = ((x0 >= y0) | (x0 >= z0)) ? 1.0F : 0.0F;    //min != x
	const float j2 = ((y0 >  x0) | (y0 >= z0)) ? 1.0F : 0.0F;    //min != y
	const float k2 = ((z0 >  x0) | (z0 >  y0)) ? 1.0F : 0.0F;    //min != z

	const float x1 = x0 - i1 + unskew_factor;
	const float y1 = y0 - j1 + unskew_factor;
	const float z1 = z0 - k1 + unskew_factor;

	const float x2 = x0 - i2 + unskew_factor * 2.0F;
	const float y2 = y0 - j2 + unskew_factor * 2.0F;
	const float z2 = z0 - k2 + unskew_factor * 2.0F;

	const float x3 = x0 - 1.0F + unskew_factor * 3.0F;
	const float y3 = y0 - 1.0F + unskew_factor * 3.0F;
	const float z3 = z0 - 1.0F + unskew_factor * 3.0F;

	float t0 = 0.5F - x0 * x0 - y0 * y0 - z0 * z0;
	if (t0 < 0.0F) t0 = 0.0F;
	t0 = t0 * t0 * t0 * t0 * d_dot_with_hashed_vec(       i0,        j0,        k0, x0, y0, z0, seed);

	float t1 = 0.5F - x1 * x1 - y1 * y1 - z1 * z1;
	if (t1 < 0.0F) t1 = 0.0F;
	t1 = t1 * t1 * t1 * t1 * d_dot_with_hashed_vec(  i1 + i0,   j1 + j0,   k1 + k0, x1, y1, z1, seed);

	float t2 = 0.5F - x2 * x2 - y2 * y2 - z2 * z2;
	if (t2 < 0.0F) t2 = 0.0F;
	t2 = t2 * t2 * t2 * t2 * d_dot_with_hashed_vec(  i2 + i0,   j2 + j0,   k2 + k0, x2, y2, z2, seed);

	float t3 = 0.5F - x3 * x3 - y3 * y3 - z3 * z3;
	if (t3 < 0.0F) t3 = 0.0F;
	t3 = t3 * t3 * t3 * t3 * d_dot_with_hashed_vec(1.0F + i0, 1.0F + j0, 1.0F + k0, x3, y3, z3, seed);

	//76.0F maps to just within [-1.0F, 1.0F]
	reinterpret_cast<float*>(dst.ptr)[idx_x + idx_y * dst.pitch + idx_z * dst.pitch * dst.ysize] = 76.0F * (t0 + t1 + t2 + t3);

	return;
}
*/

/*
__global__ void d_simplex_3d_uint8_t(cudaPitchedPtr dst, uint3 dim, float3 begin, float3 step, uint32_t seed)
{
	const uint32_t idx_x = blockIdx.x * blockDim.x + threadIdx.x;
	const uint32_t idx_y = blockIdx.y * blockDim.y + threadIdx.y;
	const uint32_t idx_z = blockIdx.z * blockDim.z + threadIdx.z;

	if (idx_x >= dim.x || idx_y >= dim.y || idx_z >= dim.z)
		return;

	const float x_in = begin.x + step.x * idx_x;
	const float y_in = begin.y + step.y * idx_y;
	const float z_in = begin.z + step.z * idx_z;

	//Begin algorithm

	constexpr float skew_factor = 1.0F / 3.0F;

	const float skew = (x_in + y_in + z_in) * skew_factor;

	const float i0 = floorf(x_in + skew);
	const float j0 = floorf(y_in + skew);
	const float k0 = floorf(z_in + skew);

	constexpr float unskew_factor = 1.0F / 6.0F;

	const float unskew = (i0 + j0 + k0) * unskew_factor;

	const float x_orig = i0 - unskew;
	const float y_orig = j0 - unskew;
	const float z_orig = k0 - unskew;

	const float x0 = x_in - x_orig;
	const float y0 = y_in - y_orig;
	const float z0 = z_in - z_orig;

	const float i1 = ((x0 >= y0) & (x0 >= z0)) ? 1.0F : 0.0F;    //max == x
	const float j1 = ((y0 >  x0) & (y0 >= z0)) ? 1.0F : 0.0F;    //max == y
	const float k1 = ((z0 >  x0) & (z0 >  y0)) ? 1.0F : 0.0F;    //max == z
	const float i2 = ((x0 >= y0) | (x0 >= z0)) ? 1.0F : 0.0F;    //min != x
	const float j2 = ((y0 >  x0) | (y0 >= z0)) ? 1.0F : 0.0F;    //min != y
	const float k2 = ((z0 >  x0) | (z0 >  y0)) ? 1.0F : 0.0F;    //min != z

	const float x1 = x0 - i1 + unskew_factor;
	const float y1 = y0 - j1 + unskew_factor;
	const float z1 = z0 - k1 + unskew_factor;

	const float x2 = x0 - i2 + unskew_factor * 2.0F;
	const float y2 = y0 - j2 + unskew_factor * 2.0F;
	const float z2 = z0 - k2 + unskew_factor * 2.0F;

	const float x3 = x0 - 1.0F + unskew_factor * 3.0F;
	const float y3 = y0 - 1.0F + unskew_factor * 3.0F;
	const float z3 = z0 - 1.0F + unskew_factor * 3.0F;

	float t0 = 0.5F - x0 * x0 - y0 * y0 - z0 * z0;
	if (t0 < 0.0F) t0 = 0.0F;
	t0 = t0 * t0 * t0 * t0 * d_dot_with_hashed_vec(       i0,        j0,        k0, x0, y0, z0, seed);

	float t1 = 0.5F - x1 * x1 - y1 * y1 - z1 * z1;
	if (t1 < 0.0F) t1 = 0.0F;
	t1 = t1 * t1 * t1 * t1 * d_dot_with_hashed_vec(  i1 + i0,   j1 + j0,   k1 + k0, x1, y1, z1, seed);

	float t2 = 0.5F - x2 * x2 - y2 * y2 - z2 * z2;
	if (t2 < 0.0F) t2 = 0.0F;
	t2 = t2 * t2 * t2 * t2 * d_dot_with_hashed_vec(  i2 + i0,   j2 + j0,   k2 + k0, x2, y2, z2, seed);

	float t3 = 0.5F - x3 * x3 - y3 * y3 - z3 * z3;
	if (t3 < 0.0F) t3 = 0.0F;
	t3 = t3 * t3 * t3 * t3 * d_dot_with_hashed_vec(1.0F + i0, 1.0F + j0, 1.0F + k0, x3, y3, z3, seed);

	reinterpret_cast<uint8_t*>(dst.ptr)[idx_x + idx_y * dst.pitch + idx_z * dst.pitch * dst.ysize] = (76.0F * (t0 + t1 + t2 + t3)) * 128 + 128;

	return;
}
*/

/*
__global__ void d_simplex_3d_surface2d_grayscale_argb(cudaSurfaceObject_t surf, uint2 dim, float3 begin, float2 step, uint32_t seed)
{
	const uint32_t idx_x = blockIdx.x * blockDim.x + threadIdx.x;
	const uint32_t idx_y = blockIdx.y * blockDim.y + threadIdx.y;

	if (idx_x >= dim.x || idx_y >= dim.y)
		return;

	const float x_in = begin.x + step.x * idx_x;
	const float y_in = begin.y + step.y * idx_y;
	const float z_in = begin.z;

	//Begin algorithm

	constexpr float skew_factor = 1.0F / 3.0F;

	const float skew = (x_in + y_in + z_in) * skew_factor;

	const float i0 = floorf(x_in + skew);
	const float j0 = floorf(y_in + skew);
	const float k0 = floorf(z_in + skew);

	constexpr float unskew_factor = 1.0F / 6.0F;

	const float unskew = (i0 + j0 + k0) * unskew_factor;

	const float x_orig = i0 - unskew;
	const float y_orig = j0 - unskew;
	const float z_orig = k0 - unskew;

	const float x0 = x_in - x_orig;
	const float y0 = y_in - y_orig;
	const float z0 = z_in - z_orig;

	const float i1 = ((x0 >= y0) & (x0 >= z0)) ? 1.0F : 0.0F;    //max == x
	const float j1 = ((y0 >  x0) & (y0 >= z0)) ? 1.0F : 0.0F;    //max == y
	const float k1 = ((z0 >  x0) & (z0 >  y0)) ? 1.0F : 0.0F;    //max == z
	const float i2 = ((x0 >= y0) | (x0 >= z0)) ? 1.0F : 0.0F;    //min != x
	const float j2 = ((y0 >  x0) | (y0 >= z0)) ? 1.0F : 0.0F;    //min != y
	const float k2 = ((z0 >  x0) | (z0 >  y0)) ? 1.0F : 0.0F;    //min != z

	const float x1 = x0 - i1 + unskew_factor;
	const float y1 = y0 - j1 + unskew_factor;
	const float z1 = z0 - k1 + unskew_factor;

	const float x2 = x0 - i2 + unskew_factor * 2.0F;
	const float y2 = y0 - j2 + unskew_factor * 2.0F;
	const float z2 = z0 - k2 + unskew_factor * 2.0F;

	const float x3 = x0 - 1.0F + unskew_factor * 3.0F;
	const float y3 = y0 - 1.0F + unskew_factor * 3.0F;
	const float z3 = z0 - 1.0F + unskew_factor * 3.0F;

	float t0 = 0.5F - x0 * x0 - y0 * y0 - z0 * z0;
	if (t0 < 0.0F) t0 = 0.0F;
	t0 = t0 * t0 * t0 * t0 * d_dot_with_hashed_vec(i0, j0, k0, x0, y0, z0, seed);

	float t1 = 0.5F - x1 * x1 - y1 * y1 - z1 * z1;
	if (t1 < 0.0F) t1 = 0.0F;
	t1 = t1 * t1 * t1 * t1 * d_dot_with_hashed_vec(i1 + i0, j1 + j0, k1 + k0, x1, y1, z1, seed);

	float t2 = 0.5F - x2 * x2 - y2 * y2 - z2 * z2;
	if (t2 < 0.0F) t2 = 0.0F;
	t2 = t2 * t2 * t2 * t2 * d_dot_with_hashed_vec(i2 + i0, j2 + j0, k2 + k0, x2, y2, z2, seed);

	float t3 = 0.5F - x3 * x3 - y3 * y3 - z3 * z3;
	if (t3 < 0.0F) t3 = 0.0F;
	t3 = t3 * t3 * t3 * t3 * d_dot_with_hashed_vec(1.0F + i0, 1.0F + j0, 1.0F + k0, x3, y3, z3, seed);

	uint8_t val = static_cast<uint8_t>((76.0F * (t0 + t1 + t2 + t3)) * 128 + 128.0F);

	int32_t pixel_val = 0xFF000000 | val | (static_cast<int32_t>(val >> 1) << 8);

	surf2Dwrite(pixel_val, surf, idx_x * 4, idx_y);

	return;
}
*/
