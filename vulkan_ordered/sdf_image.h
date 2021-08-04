#pragma once

#include <cstdint>

#include <cstdlib>

#include "och_error_handling.h"

#include "och_fio.h"

#include "bitmap.h"

#include "binary_image.h"

#include "truetype.h"

#include "simple_vec.h"

struct sdf_image
{
private:

	struct point
	{
		int32_t dx, dy;

		int32_t d_sq() const noexcept { return dx * dx + dy * dy; }
	};

	float* m_data = nullptr;

	int32_t m_width = 0;

	int32_t m_height = 0;

public:

	using colour_mapper_fn = texel_b8g8r8(*) (float) noexcept;

	struct colour_mapper
	{
		static texel_b8g8r8 linear_distance(float dst) noexcept
		{
			const uint8_t c = static_cast<uint8_t>(dst * 0.5F + 0.5F);

			if (dst < 0.0F)
				return { c, c,c };
			else
				return { c, static_cast<uint8_t>(c >> 1), 0 };
		}

		static texel_b8g8r8 nonlinear_distance(float dst) noexcept
		{
			const uint8_t c = static_cast<uint8_t>((-0.1F / ((dst < 0.0F ? -dst : dst) + 0.1F) + 1.0F) * 256.0F); 
			
			if (dst < 0.0F)
				return { c, c, c };
			else
				return { c, static_cast<uint8_t>(c >> 1), 0 };
		}

		static texel_b8g8r8 binary(float dst) noexcept
		{
			if (dst < 0.0F)
				return col::b8g8r8::white;
			else
				return col::b8g8r8::black;
		}

		static texel_b8g8r8 bands(float dst) noexcept
		{
			dst = dst * 0.5F + 0.5F;
			
			if (dst < 0.1666F)
			{
				const uint8_t v = static_cast<uint8_t>(((dst * 5.0F) + 0.1666F) * 255.0F);
			
				return { 0, 0, v };
			}
			if (dst < 0.3333F)
			{
				const uint8_t v = static_cast<uint8_t>((((dst - 0.1666F) * 5.0F) + 0.1666F) * 128.0F);
			
				return { 0, v, v };
			}
			if (dst < 0.5000F)
			{
				const uint8_t v = static_cast<uint8_t>((((dst - 0.3333F) * 5.0F) + 0.1666F) * 255.0F);
			
				return { 0, v, 0 };
			}
			if (dst < 0.6666F)
			{
				const uint8_t v = static_cast<uint8_t>((((dst - 0.5000F) * 5.0F) + 0.1666F) * 128.0F);
			
				return { v, v, 0 };
			}
			if (dst < 0.8333F)
			{
				const uint8_t v = static_cast<uint8_t>((((dst - 0.6666F) * 5.0F) + 0.1666F) * 255.0F);
			
				return { v, 0, 0 };
			}
			
			const uint8_t v = static_cast<uint8_t>((((dst - 0.8333F) * 5.0F) + 0.1666F) * 128.0F);
			
			return { v, 0, v };
		}

		static texel_b8g8r8 quadratic_waves(float dst) noexcept
		{
			constexpr float mod_fct = 0.01F;

			const float dst_1_0 = dst * 0.5F + 0.5F;

			const float dst_mod = fmodf(dst_1_0, mod_fct) * (1.0F / mod_fct);

			const float dst_lo = dst_mod - 0.5F;

			const float v = 1.0F - (4.0F * dst_lo * dst_lo);

			const uint8_t c = static_cast<uint8_t>(v * 128.0F + 112.0F);

			if (dst < 0.0F)
				return { c, c, static_cast<uint8_t>(255 - c) };
			else
				return { static_cast<uint8_t>(255 - c), c,c };
		}

		static texel_b8g8r8 quartic_waves(float dst) noexcept
		{
			constexpr float mod_fct = 0.01F;
			
			const float dst_1_0 = dst * 0.5F + 0.5F;
			
			const float dst_mod = fmodf(dst_1_0, mod_fct) * (1.0F / mod_fct);
			
			const float dst_lo = dst_mod - 0.5F;
			
			const float v = 1.0F - (16.0F * dst_lo * dst_lo * dst_lo * dst_lo);
			
			const uint8_t c = static_cast<uint8_t>(v * 128.0F + 112.0F);
			
			if (dst < 0.0F)
				return { c, c, static_cast<uint8_t>(255 - c) };
			else
				return { static_cast<uint8_t>(255 - c), c,c };
		}

		static texel_b8g8r8 layers(float dst) noexcept
		{
			const float dst_1_0 = dst * 0.5F + 0.5F;

			const float dst_mod = fmodf(dst_1_0, 0.01F);

			uint8_t c = static_cast<uint8_t>((-0.1F / (dst_mod * 100.0F + 0.1F) + 1.0F) * 256.0F);

			if (dst < 0.0F)
				return { c, c, 0 };
			else
				return { 0, c, c };
		}
	};

	och::err_info from_bim(const binary_image& img)
	{
		constexpr point INSIDE = { 0, 0 }, OUTSIDE = { 0x3FFF, 0x3FFF };

		m_width = img.width();

		m_height = img.height();

		m_data = static_cast<float*>(malloc(m_width * m_height * sizeof(float)));

		point* pos = static_cast<point*>(malloc(m_width * m_height * sizeof(point)));

		point* neg = static_cast<point*>(malloc(m_width * m_height * sizeof(point)));

		for(int32_t y = 0; y != m_height; ++y)
			for (int32_t x = 0; x != m_width; ++x)
			{
				if (img.get(x, y))
				{
					pos[x + y * m_width] = INSIDE;

					neg[x + y * m_width] = OUTSIDE;
				}
				else
				{
					pos[x + y * m_width] = OUTSIDE;

					neg[x + y * m_width] = INSIDE;
				}
			}

		pass(pos);

		pass(neg);

		const float normalization_fct = 1.0F / sqrtf(static_cast<float>(m_width * m_width + m_height * m_height));

		for(int32_t y = 0; y != m_height; ++y)
			for (int32_t x = 0; x != m_width; ++x)
			{
				float pos_dst = sqrtf(static_cast<float>(pos[x + y * m_width].d_sq()));
				
				float neg_dst = sqrtf(static_cast<float>(neg[x + y * m_width].d_sq()));

				m_data[x + y * m_width] = (neg_dst - pos_dst) * normalization_fct;
			}

		free(pos);

		free(neg);

		return {};
	}

	och::err_info from_glyph(const glyph_data& glyph, uint32_t width, uint32_t height) noexcept
	{
		m_width = width;

		m_height = height;

		m_data = static_cast<float*>(malloc(m_width * m_height * sizeof(float)));

		if (glyph.point_cnt() < 6)
		{
			for (int32_t y = 0; y != m_height; ++y)
				for (int32_t x = 0; x != m_width; ++x)
					m_data[x + y * m_width] = -1.0F;

			return {};
		}

		const float inv_width = 1.0F / static_cast<float>(m_width);

		const float inv_height = 1.0F / static_cast<float>(m_height);

		for (int32_t y = 0; y != m_height; ++y)
			for (int32_t x = 0; x != m_width; ++x)
			{
				float min_dst_sq = INFINITY;

				float min_dst_sgn = -1.0F;

				float min_dst_max_orthogonality = -INFINITY;

				uint32_t min_curve = 0;

				for (uint32_t i = 0; i != glyph.contour_cnt(); ++i)
				{
					const uint32_t beg = glyph.contour_beg_index(i), end = glyph.contour_end_index(i);

					const och::vec2 p(static_cast<float>(x) * inv_width, static_cast<float>(y) * inv_height);

					for (uint32_t j = beg; j + 2 < end; j += 2)
					{
						const och::vec2 p0 = glyph[j], p1 = glyph[j + 1], p2 = glyph[j + 2];

						if (evaluate_curve_for_pixel(p0, p1, p2, p, min_dst_sq, min_dst_sgn, min_dst_max_orthogonality))
							min_curve = j;
					}

					if (evaluate_curve_for_pixel(glyph[end - 2], glyph[end - 1], glyph[beg], p, min_dst_sq, min_dst_sgn, min_dst_max_orthogonality))
						min_curve = end - 2;
				}

				//m_data[x + y * m_width] = (static_cast<float>(min_curve) / glyph.point_cnt()) * 2.0F - 1.0F;

				m_data[x + y * m_width] = sqrtf(min_dst_sq)* min_dst_sgn;
			}

			return {};
	}

	och::err_info save_bmp(const char* filename, bool overwrite_existing_file = false, colour_mapper_fn colour_mapper = colour_mapper::nonlinear_distance)
	{
		bitmap_file file(filename, overwrite_existing_file ? och::fio::open_truncate : och::fio::open_fail, m_width, m_height);

		if (!file)
			return MSG_ERROR("Could not open file");

		for (int32_t y = 0; y != m_height; ++y)
			for (int32_t x = 0; x != m_width; ++x)
				file(x, y) = colour_mapper(m_data[x + y * m_width]);

		return {};
	}

	och::err_info save_sdf(const char* filename, bool overwrite_existing_file = false)
	{
		och::mapped_file file(filename, och::fio::access_readwrite, overwrite_existing_file ? och::fio::open_truncate : och::fio::open_fail, och::fio::open_normal, m_width * m_height * sizeof(float) + 8);

		if (!file)
			return MSG_ERROR("Could not open file");

		memcpy(file.data(), &m_width, 4);

		memcpy(file.data() + 4, &m_height, 4);

		memcpy(file.data() + 8, m_data, m_width * m_height * sizeof(float));

		return {};
	}

	och::err_info load_sdf(const char* filename) noexcept
	{
		och::mapped_file file(filename, och::fio::access_readwrite, och::fio::open_normal, och::fio::open_fail);

		if (!file)
			return MSG_ERROR("Could not open file");

		if (m_data)
			free(m_data);

		memcpy(&m_width, file.data(), 4);

		memcpy(&m_height, file.data() + 4, 4);

		m_data = static_cast<float*>(malloc(m_width * m_height * sizeof(float)));

		memcpy(m_data, file.data() + 8, m_width * m_height * sizeof(float));

		return {};
	}

private:

	__forceinline void compare(point* ps, int32_t x, int32_t y, int32_t dx, int32_t dy) noexcept
	{
		if (x + dx < 0 || x + dx >= m_width || y + dy < 0 || y + dy >= m_height)
			return;

		
		point o = ps[x + dx + (y + dy) * m_width];

		o.dx += dx;

		o.dy += dy;

		if (o.d_sq() < ps[x + y * m_width].d_sq())
			ps[x + y * m_width] = o;
	}

	void pass(point* ps)
	{
		for (int32_t y = 0; y != m_height; ++y)
		{
			for (int32_t x = 0; x != m_width; ++x)
			{
				compare(ps, x, y, -1, 0);
				compare(ps, x, y, 0, -1);
				compare(ps, x, y, -1, -1);
				compare(ps, x, y, 1, -1);
			}

			for (int32_t x = m_width - 1; x >= 0; --x)
				compare(ps, x, y, 1, 0);
		}

		for (int32_t y = m_height - 1; y >= 0; --y)
		{
			for (int32_t x = 0; x != m_width; ++x)
			{
				compare(ps, x, y,  1,  0);
				compare(ps, x, y,  0,  1);
				compare(ps, x, y, -1,  1);
				compare(ps, x, y,  1,  1);
			}

			for (int32_t x = m_width - 1; x >= 0; --x)
				compare(ps, x, y, -1, 0);
		}
	}

	static och::vec2 bezier_interp(och::vec2 p0, och::vec2 p1, och::vec2 p2, float t) noexcept
	{
		return (1.0F - t) * (1.0F - t) * p0 + 2.0F * (1.0F - t) * t * p1 + t * t * p2;
	}

	__forceinline static void check_roots(float r0, float r1, float r2, och::vec2 p0, och::vec2 p1, och::vec2 p2, och::vec2 p, float& min_dst_sq, float& min_t, och::vec2& min_p)
	{
		min_dst_sq = och::squared_magnitude(p - p0); // dst_b;
		
		min_t = 0.0F;
		
		min_p = p0;
		
		const float dst_e = och::squared_magnitude(p - p2);
		
		if (dst_e < min_dst_sq)
		{
			min_dst_sq = dst_e;
		
			min_t = 1.0F;
		
			min_p = p2;
		}
		
		if (r0 > 0.0F && r0 < 1.0F)
		{
			const och::vec2 pb = bezier_interp(p0, p1, p2, r0);
		
			const float dst_0 = och::squared_magnitude(p - pb);
		
			if (dst_0 < min_dst_sq)
			{
				min_dst_sq = dst_0;
		
				min_t = r0;
		
				min_p = pb;
			}
		}
		
		if (r1 > 0.0F && r1 < 1.0F)
		{
			const och::vec2 pb = bezier_interp(p0, p1, p2, r1);
		
			const float dst_1 = och::squared_magnitude(p - pb);
		
			if (dst_1 < min_dst_sq)
			{
				min_dst_sq = dst_1;
		
				min_t = r1;
		
				min_p = pb;
			}
		}
		
		if (r2 > 0.0F && r2 < 1.0F)
		{
			const och::vec2 pb = bezier_interp(p0, p1, p2, r2);
		
			const float dst_2 = och::squared_magnitude(p - pb);
		
			if (dst_2 < min_dst_sq)
			{
				min_dst_sq = dst_2;
		
				min_t = r2;
		
				min_p = pb;
			}
		}
	}

	__forceinline static bool evaluate_curve_for_pixel(och::vec2 p0, och::vec2 p1, och::vec2 p2, och::vec2 p, float& global_min_dst_sq, float& global_min_dst_sgn, float& global_min_dst_max_orthogonality)
	{
		const och::vec2 dp = p - p0;

		const och::vec2 d1 = p1 - p0;

		const och::vec2 d2 = p2 - 2.0F * p1 + p0;

		const float a3 = och::dot(d2, d2);

		const float a2 = 3.0F * och::dot(d1, d2);

		const float a1 = 2.0F * och::dot(d1, d1) - och::dot(d2, dp);

		const float a0 = -och::dot(d1, dp);


		float min_dst_sq;

		float min_t;

		och::vec2 min_p;


		if (och::abs(a3) > 1e-7F)
		{
			float r0, r1, r2;

			och::cubic_poly_roots(a3, a2, a1, a0, r0, r1, r2);

			check_roots(r0, r1, r2, p0, p1, p2, p, min_dst_sq, min_t, min_p);
		}
		else
		{
			min_t = och::dot(p - p0, p2 - p0) / och::dot(p2 - p0, p2 - p0);

			if (min_t > 1.0F)
				min_t = 1.0F;
			else if (min_t < 0.0F)
				min_t = 0.0F;

			min_p = p0 * (1.0F - min_t) + p2 * min_t;

			min_dst_sq = och::squared_magnitude(p - min_p);
		}

		if (min_dst_sq <= global_min_dst_sq + 1e-7F)
		{
			const och::vec2 deriv = 2.0F * min_t * (p2 - 2.0F * p1 + p0) + 2.0F * (p1 - p0); // 2.0F * (min_t * (p0 - 2.0F * p1 + p2) + p1 - p0);

			const float orthogonality = och::abs(och::cross(och::normalize(deriv), och::normalize(p - min_p)));

			if (min_dst_sq + 1e-7F < global_min_dst_sq || global_min_dst_max_orthogonality < orthogonality)
			{
				global_min_dst_sq = min_dst_sq;

				global_min_dst_sgn = och::cross(deriv, min_p - p) < 0.0F ? -1.0F : 1.0F;

				global_min_dst_max_orthogonality = orthogonality;

				return true;
			}
		}

		return false;
	}
};
