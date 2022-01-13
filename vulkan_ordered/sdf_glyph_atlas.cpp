#include "sdf_glyph_atlas.h"

#include <cstdint>
#include <cstdlib>
#include <cassert>

#include "och_err.h"
#include "och_matmath.h"
#include "truetype.h"
#include "heap_buffer.h"
#include "image_view.h"
#include "bitmap.h"

#define TEMP_STATUS_MACRO to_status(och::status(1, och::error_type::och))

static void cubic_poly_roots(float a3, float a2, float a1, float a0, float& r0, float& r1, float& r2) noexcept
{
	constexpr float TOO_SMALL = 1e-7F;

	constexpr float PI = 3.14159265359F;

	a2 /= a3;

	a1 /= a3;

	a0 /= a3;

	const float q = (3.0F * a1 - (a2 * a2)) / 9.0F;

	const float r = (-27.0F * a0 + a2 * (9.0F * a1 - 2.0F * a2 * a2)) / 54.0F;

	const float disc = q * q * q + r * r;

	const float term_1 = -a2 / 3.0F;

	if (disc > TOO_SMALL)
	{
		const float disc_sqrt = sqrtf(disc);

		float s = r + disc_sqrt;

		s = s < 0.0F ? -cbrtf(-s) : cbrtf(s);

		float t = r - disc_sqrt;

		t = t < 0.0F ? -cbrtf(-t) : cbrtf(t);

		r0 = term_1 + s + t;

		r1 = INFINITY;

		r2 = INFINITY;
	}
	else if (disc < -TOO_SMALL)
	{
		const float dummy = acosf(r / sqrtf(-q * q * q));

		const float r13 = 2.0F * sqrtf(-q);

		r0 = term_1 + r13 * cosf(dummy / 3.0F);

		r1 = term_1 + r13 * cosf((dummy + 2.0F * PI) / 3.0F);

		r2 = term_1 + r13 * cosf((dummy + 4.0F * PI) / 3.0F);
	}
	else
	{
		const float r13 = r < 0.0F ? -cbrtf(-r) : cbrtf(r);

		r0 = term_1 + 2.0F * r13;

		r1 = term_1 - r13;

		r2 = INFINITY;
	}
}

static och::vec2 bezier_interp(och::vec2 p0, och::vec2 p1, och::vec2 p2, float t) noexcept
{
	return (1.0F - t) * (1.0F - t) * p0 + 2.0F * (1.0F - t) * t * p1 + t * t * p2;
}

static void check_roots(float r0, float r1, float r2, och::vec2 p0, och::vec2 p1, och::vec2 p2, och::vec2 p, float& min_dst_sq, float& min_t, och::vec2& min_p)
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

static bool evaluate_curve_for_pixel(och::vec2 p0, och::vec2 p1, och::vec2 p2, och::vec2 p, float& global_min_dst_sq, float& global_min_dst_sgn, float& global_min_dst_max_orthogonality)
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


	if (fabs(a3) > 1e-7F)
	{
		float r0, r1, r2;

		cubic_poly_roots(a3, a2, a1, a0, r0, r1, r2);

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

		const float orthogonality = fabs(och::cross(och::normalize(deriv), och::normalize(p - min_p)));

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

template<typename Texel, typename Mapper>
static void sdf_from_glyph(image_view<Texel> img, const glyph_data& glyph, uint32_t pixel_width, uint32_t pixel_height, float glyph_scale, const Mapper& mapper)
{
	{
		const Texel min_texel = mapper(-1.0F);

		for (uint32_t y = 0; y != pixel_height; ++y)
			for (uint32_t x = 0; x != pixel_width; ++x)
				img(x, y) = min_texel;

		if (glyph.point_cnt() < 6) // Nothing to draw...
			return;
	}

	const float glf_offset = (1.0F - glyph_scale) * 0.5F;

	const float img_step = 1.0F / fmaxf(static_cast<float>(pixel_width), static_cast<float>(pixel_height));

	const float test = 1.0F * glyph_scale + glf_offset;

	// Start looping over image

	for (uint32_t y = 0; y != pixel_height; ++y)
		for (uint32_t x = 0; x != pixel_width; ++x)
		{
			const och::vec2 p(img_step * static_cast<float>(x), img_step * static_cast<float>(y));

			float min_dst_sq = INFINITY;

			float min_dst_sgn = -1.0F;

			float min_dst_max_orthogonality = -INFINITY;

			uint32_t min_curve = 0;

			for (uint32_t i = 0; i != glyph.contour_cnt(); ++i)
			{
				const uint32_t beg = glyph.contour_beg_index(i), end = glyph.contour_end_index(i);

				for (uint32_t j = beg; j + 2 < end; j += 2)
				{
					const och::vec2 p0_u = glyph[j];
					const och::vec2 p0{ p0_u.x * glyph_scale + glf_offset, p0_u.y * glyph_scale + glf_offset };
					const och::vec2 p1_u = glyph[j + 1];
					const och::vec2 p1{ p1_u.x * glyph_scale + glf_offset, p1_u.y * glyph_scale + glf_offset };
					const och::vec2 p2_u = glyph[j + 2];
					const och::vec2 p2{ p2_u.x * glyph_scale + glf_offset, p2_u.y * glyph_scale + glf_offset };

					if (evaluate_curve_for_pixel(p0, p1, p2, p, min_dst_sq, min_dst_sgn, min_dst_max_orthogonality))
						min_curve = j;
				}

				const och::vec2 p0_u = glyph[end - 2];
				const och::vec2 p0{ p0_u.x * glyph_scale + glf_offset, p0_u.y * glyph_scale + glf_offset };
				const och::vec2 p1_u = glyph[end - 1];
				const och::vec2 p1{ p1_u.x * glyph_scale + glf_offset, p1_u.y * glyph_scale + glf_offset };
				const och::vec2 p2_u = glyph[beg];
				const och::vec2 p2{ p2_u.x * glyph_scale + glf_offset, p2_u.y * glyph_scale + glf_offset };

				if (evaluate_curve_for_pixel(p0, p1, p2, p, min_dst_sq, min_dst_sgn, min_dst_max_orthogonality))
					min_curve = end - 2;
			}

			img(x, y) = mapper(sqrtf(min_dst_sq) * min_dst_sgn);
		}
}

template<ptrdiff_t offset, size_t bytes, bool reverse = false, typename T>
static void sort(heap_buffer<T>& input)
{
	static_assert(offset + bytes <= sizeof(T));

	const uint32_t sz = input.size();

	heap_buffer<T> swap_buffer(sz);

	for (size_t b = 0; b != bytes; ++b)
	{
		T* curr = (b & 1) ? swap_buffer.data() : input.data();

		T* buf = (b & 1) ? input.data() : swap_buffer.data();

		uint8_t* cb = reinterpret_cast<uint8_t*>(curr) + offset + b;

		uint32_t cnts[256]{};

		for (uint32_t i = 0; i != sz; ++i)
		{
			if constexpr (reverse)
				++cnts[255 - *cb];
			else
				++cnts[*cb];

			cb += sizeof(T);
		}

		uint32_t prev_sum = 0;

		for (uint32_t& cnt : cnts)
		{
			uint32_t tmp = cnt;

			cnt = prev_sum;

			prev_sum += tmp;
		}

		cb = reinterpret_cast<uint8_t*>(curr) + offset + b;

		for (uint32_t i = 0; i != sz; ++i)
		{
			if constexpr (reverse)
				buf[cnts[255 - *cb]++] = curr[i];
			else
				buf[cnts[*cb]++] = curr[i];

			cb += sizeof(T);
		}
	}

	if constexpr (bytes & 1)
		input.attach(swap_buffer.detach());
}

struct mapper_range
{
	uint32_t beg;
	uint32_t end;
	int32_t offset;
};

struct glfatl_fileheader
{
	uint32_t m_width;
	uint32_t m_height;
	float m_line_height;
	uint32_t m_glyph_scale;
	uint32_t m_map_ranges_size;
	uint32_t m_map_indices_size;

	uint32_t map_ranges_bytes() const noexcept { return m_map_ranges_size * sizeof(mapper_range); }

	uint32_t map_indices_bytes() const noexcept { return m_map_indices_size * sizeof(glyph_atlas::glyph_index); }

	uint32_t image_bytes() const noexcept { return m_width * m_height; }

	mapper_range* map_ranges_data() noexcept
	{
		return reinterpret_cast<mapper_range*>(reinterpret_cast<uint8_t*>(this) + sizeof(*this));
	}

	const mapper_range* map_ranges_data() const noexcept
	{
		return reinterpret_cast<const mapper_range*>(reinterpret_cast<const uint8_t*>(this) + sizeof(*this));
	}

	glyph_atlas::glyph_index* map_indices_data() noexcept
	{
		return reinterpret_cast<glyph_atlas::glyph_index*>(reinterpret_cast<uint8_t*>(this) + sizeof(*this) + map_ranges_bytes());
	}

	const glyph_atlas::glyph_index* map_indices_data() const noexcept
	{
		return reinterpret_cast<const glyph_atlas::glyph_index*>(reinterpret_cast<const uint8_t*>(this) + sizeof(*this) + map_ranges_bytes());
	}

	uint8_t* image_data() noexcept
	{
		return reinterpret_cast<uint8_t*>(this) + sizeof(*this) + map_ranges_bytes() + map_indices_bytes();
	}

	const uint8_t* image_data() const noexcept
	{
		return reinterpret_cast<const uint8_t*>(this) + sizeof(*this) + map_ranges_bytes() + map_indices_bytes();
	}
};

// TODO: 
// Calculate advance to save in m_map_indices
// Implement mapping equivalent glyphs to a single spot in the image.
och::status glyph_atlas::create(const char* truetype_filename, uint32_t glyph_size, uint32_t glyph_padding_pixels, float sdf_clamp, uint32_t map_width, const och::range<codept_range> codept_ranges) noexcept
{
	struct glyph_address
	{
		uint32_t glyph_id;
		uint32_t buffer_begin;
		uint32_t w;
		uint32_t h;
		uint32_t x;
		uint32_t y;
	};

	struct codept_id_pair
	{
		uint32_t codept;
		uint32_t glyph_id;
	};

	// Open file

	truetype_file file;

	{
		check(file.create(truetype_filename));

		m_line_height = file.line_height();

		m_glyph_scale = glyph_size;
	}


	// Count Codepoints to be mapped

	uint32_t codept_cnt = 0;

	for (auto& r : codept_ranges)
		codept_cnt += r.end - r.beg;

	// Create list of (unique) codepoints with their matching glyph ids.

	heap_buffer<codept_id_pair> cp_ids(codept_cnt);

	{
		uint32_t curr_cp_id = 0;

		for (const auto& r : codept_ranges)
			for (uint32_t cp = r.beg; cp != r.end; ++cp)
				cp_ids[curr_cp_id++].codept = cp;

		sort<offsetof(codept_id_pair, codept), 3>(cp_ids);

		uint32_t prev_cp = ~cp_ids[0].codept;

		uint32_t curr_idx = 0;

		for (uint32_t i = 0; i != cp_ids.size(); ++i)
			if (cp_ids[i].codept != prev_cp)
			{
				cp_ids[curr_idx++].codept = cp_ids[i].codept;

				prev_cp = cp_ids[i].codept;
			}

		cp_ids.shrink(curr_idx);

		for (auto& cp_id : cp_ids)
			cp_id.glyph_id = file.get_glyph_id_from_codept(cp_id.codept);
	}

	// Create list of (unique) glyph ids

	heap_buffer<uint32_t> ids(cp_ids.size() + 1);

	{
		for (uint32_t i = 0; i != cp_ids.size(); ++i)
			ids[i] = cp_ids[i].glyph_id;

		ids[cp_ids.size()] = 0;

		sort<0, 4>(ids);

		uint32_t curr_idx = 0;

		uint32_t prev_id = ~ids[0];

		for (auto id : ids)
		{
			if (id != prev_id)
			{
				ids[curr_idx++] = id;

				prev_id = id;
			}
		}

		ids.shrink(curr_idx);
	}

	// Draw glyphs into buffer and record their true sizes

	const uint32_t padded_glyph_size = static_cast<uint32_t>(static_cast<float>(glyph_size) * (1.0F + 2.0F * sdf_clamp) + 2.0F);

	const float glyph_scale = static_cast<float>(glyph_size) / static_cast<float>(padded_glyph_size);


	heap_buffer<uint8_t> sdf_buffer(ids.size() * padded_glyph_size * padded_glyph_size);

	heap_buffer<glyph_address> addresses(ids.size());

	{
		uint32_t curr_beg = 0;

		uint32_t curr_idx = 0;

		for (auto& id : ids)
		{
			glyph_data glyph = file.get_glyph_data_from_id(id);

			if (!glyph.metrics().x_size())
				continue;

			image_view buffer_view(sdf_buffer.data() + curr_beg, padded_glyph_size, 0, 0);

			struct {
				const float clamp;

				uint8_t operator()(float dst) const noexcept
				{
					const float clamped = dst < -clamp ? -clamp : dst > clamp ? clamp : dst;

					return static_cast<uint8_t>((clamped + clamp) * (127.5F / clamp));
				}
			} mapper{ sdf_clamp };

			sdf_from_glyph(buffer_view, glyph, padded_glyph_size, padded_glyph_size, glyph_scale, mapper);

			uint32_t min_x = ~0u, max_x = 0, min_y = ~0u, max_y = 0;

			for (uint32_t y = 0; y != padded_glyph_size; ++y)
				for (uint32_t x = 0; x != padded_glyph_size; ++x)
					if (buffer_view(x, y))
					{
						if (x < min_x)
							min_x = x;
						if (x > max_x)
							max_x = x;
						if (y < min_y)
							min_y = y;
						if (y > max_y)
							max_y = y;
					}

			if (min_x != ~0u)
			{
				uint32_t w = max_x - min_x + 1;

				uint32_t h = max_y - min_y + 1;

				addresses[curr_idx].glyph_id = id;
				addresses[curr_idx].buffer_begin = curr_beg + min_x + min_y * padded_glyph_size;
				addresses[curr_idx].w = w;
				addresses[curr_idx].h = h;
				++curr_idx;

				curr_beg += padded_glyph_size * padded_glyph_size;
			}
		}

		addresses.shrink(curr_idx);
	}

	// Find an arrangement for the glyphs and allocate final image accordingly
	{
		sort<offsetof(glyph_address, w), 8, true>(addresses);

		uint32_t curr_x = 0;

		uint32_t curr_y = addresses[0].h + 2 * glyph_padding_pixels;

		for (auto& a : addresses)
		{
			uint32_t padded_w = a.w + glyph_padding_pixels;

			uint32_t padded_h = a.h + glyph_padding_pixels;

			if (padded_w > map_width - glyph_padding_pixels)
				return TEMP_STATUS_MACRO; // Glyph too large for given map_width parameter

			if (curr_x + padded_w > map_width - glyph_padding_pixels)
			{
				curr_y += padded_h;

				curr_x = padded_w;
			}
			else
				curr_x += padded_w;

			a.x = curr_x - padded_w;

			a.y = curr_y - padded_h;
		}

		m_width = map_width;

		m_height = curr_y + glyph_padding_pixels >= 1u ? curr_y + glyph_padding_pixels : 1u;

		m_image.allocate(m_width * m_height);

		memset(m_image.data(), 0x00, m_width * m_height);
	}

	// Copy glyphs from buffer to final image
	{
		for (auto& a : addresses)
			for (uint32_t y = 0; y != a.h; ++y)
				for (uint32_t x = 0; x != a.w; ++x)
				{
					const uint8_t v = sdf_buffer[a.buffer_begin + x + y * padded_glyph_size];

					m_image[a.x + x + glyph_padding_pixels + (a.h + a.y + glyph_padding_pixels - y - 1) * m_width] = v;
				}
	}

	// Generate mapping data

	uint32_t range_cnt = 1;

	// Count continuous codepoint ranges
	{
		uint32_t prev_cp = cp_ids[0].codept;

		for (uint32_t i = 1; i != cp_ids.size(); ++i)
			if (cp_ids[i].codept != prev_cp + 1)
			{
				++range_cnt;

				prev_cp = cp_ids[i].codept;
			}
			else
				++prev_cp;
	}

	// Generate codepoint ranges
	{
		m_map_ranges.allocate(range_cnt);

		uint32_t curr_idx = 0;

		uint32_t prev_cp = cp_ids[0].codept;

		uint32_t last_beg = cp_ids[0].codept;

		m_map_ranges[0].beg = prev_cp;

		for (uint32_t i = 0; i != cp_ids.size(); ++i)
			if (cp_ids[i].codept != prev_cp + 1)
			{
				m_map_ranges[curr_idx].beg = last_beg;
				m_map_ranges[curr_idx].end = prev_cp;
				m_map_ranges[curr_idx].offset = static_cast<int32_t>(i - cp_ids[i].codept + 1);
				++curr_idx;

				prev_cp = last_beg = cp_ids[i].codept;
			}
			else
				++prev_cp;

		m_map_ranges[range_cnt - 1].end = cp_ids[cp_ids.size() - 1].codept;
		m_map_ranges[range_cnt - 1].offset = static_cast<int32_t>(cp_ids.size() - 1 - cp_ids[cp_ids.size() - 1].codept + 1);
	}

	m_map_indices.allocate(cp_ids.size() + 1);

	//m_map_indices[0] = { {0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F}, 0.0F };

	// Generate codepoint indices
	{
		sort<offsetof(glyph_address, glyph_id), 4>(addresses);

		const float inv_width = 1.0F / m_width;

		const float inv_height = 1.0F / m_height;

		const float inv_scale = 1.0F / m_glyph_scale;

		// Set first map index to missing-character glyph
		{
			const glyph_address& a = addresses[0];

			glyph_metrics mtx = file.get_glyph_metrics_from_id(0);

			const float atlas_pos_x = a.x * inv_width;

			const float atlas_pos_y = a.y * inv_height;

			const float atlas_ext_x = a.w * inv_width;

			const float atlas_ext_y = a.h * inv_height;

			const float real_ext_x = a.w * inv_scale;

			const float real_ext_y = a.h * inv_scale;

			const float real_bearing_x = mtx.left_side_bearing();

			const float real_bearing_y = 1.0F - mtx.y_size() - (mtx.y_min() - file.baseline_offset());

			const float real_advance = mtx.advance_width();

			m_map_indices[0].atlas_position = { atlas_pos_x   , atlas_pos_y };
			m_map_indices[0].atlas_extent = { atlas_ext_x   , atlas_ext_y };
			m_map_indices[0].real_extent = { real_ext_x    , real_ext_y };
			m_map_indices[0].real_bearing = { real_bearing_x, real_bearing_y };
			m_map_indices[0].real_advance = real_advance;
		}

		for (uint32_t i = 0; i != cp_ids.size(); ++i)
		{
			uint32_t curr_id = cp_ids[i].glyph_id;

			{
				int64_t lo = 0, hi = addresses.size() - 1;

				while (lo <= hi)
				{
					int64_t mid = lo + ((hi - lo) >> 1);

					const uint32_t mid_id = addresses[static_cast<uint32_t>(mid)].glyph_id;

					if (mid_id == curr_id)
					{
						const glyph_address& a = addresses[static_cast<uint32_t>(mid)];

						glyph_metrics mtx = file.get_glyph_metrics_from_id(curr_id);

						const float atlas_pos_x = (a.x + glyph_padding_pixels) * inv_width;

						const float atlas_pos_y = (a.y + glyph_padding_pixels) * inv_height;

						const float atlas_ext_x = a.w * inv_width;

						const float atlas_ext_y = a.h * inv_height;

						const float real_ext_x = a.w * inv_scale;

						const float real_ext_y = a.h * inv_scale;

						const float real_bearing_x = mtx.left_side_bearing();

						const float real_bearing_y = 1.0F - mtx.y_size() - (mtx.y_min() - file.baseline_offset());

						const float real_advance = mtx.advance_width();

						m_map_indices[i + 1].atlas_position = { atlas_pos_x   , atlas_pos_y };
						m_map_indices[i + 1].atlas_extent = { atlas_ext_x   , atlas_ext_y };
						m_map_indices[i + 1].real_extent = { real_ext_x    , real_ext_y };
						m_map_indices[i + 1].real_bearing = { real_bearing_x, real_bearing_y };
						m_map_indices[i + 1].real_advance = real_advance;

						break;
					}
					else if (mid_id > curr_id)
						hi = mid - 1;
					else
						lo = mid + 1;
				}

				if (lo > hi)
				{
					glyph_metrics mtx = file.get_glyph_metrics_from_id(curr_id);

					const float advance = mtx.advance_width();

					m_map_indices[i + 1] = { {0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F}, advance };
				}
			}
		}
	}

	file.close();

	return {};
}

void glyph_atlas::destroy() noexcept {}

och::status glyph_atlas::save_glfatl(const char* filename, bool overwrite_existing_file) const noexcept
{
	const uint32_t image_bytes = m_width * m_height;

	const uint32_t map_ranges_bytes = m_map_ranges.size() * sizeof(*m_map_ranges.data());

	const uint32_t map_indices_bytes = m_map_indices.size() * sizeof(*m_map_indices.data());

	const uint32_t metadata_bytes = sizeof(glfatl_fileheader);

	const uint32_t total_file_bytes = image_bytes + map_ranges_bytes + map_indices_bytes + metadata_bytes;

	och::mapped_file<glfatl_fileheader> file;

	check(file.create(filename, och::fio::access::read_write, overwrite_existing_file ? och::fio::open::truncate : och::fio::open::fail, och::fio::open::normal, total_file_bytes));

	// Layout: m_width, m_height, m_map_ranges.size(), m_map_indices.size(), m_map_ranges, m_map_indices, m_image

	glfatl_fileheader& hdr = file[0];

	hdr.m_width = m_width;

	hdr.m_height = m_height;

	hdr.m_line_height = m_line_height;

	hdr.m_glyph_scale = m_glyph_scale;

	hdr.m_map_ranges_size = m_map_ranges.size();

	hdr.m_map_indices_size = m_map_indices.size();

	memcpy(hdr.map_ranges_data(), m_map_ranges.data(), map_ranges_bytes);

	memcpy(hdr.map_indices_data(), m_map_indices.data(), map_indices_bytes);

	memcpy(hdr.image_data(), m_image.data(), image_bytes);

	file.close();

	return {};
}

och::status glyph_atlas::load_glfatl(const char* filename) noexcept
{
	och::mapped_file<const glfatl_fileheader> file;

	check(file.create(filename, och::fio::access::read, och::fio::open::normal, och::fio::open::fail));

	const glfatl_fileheader& hdr = file[0];

	m_width = hdr.m_width;

	m_height = hdr.m_height;

	m_line_height = hdr.m_line_height;

	m_glyph_scale = hdr.m_glyph_scale;

	m_map_ranges.allocate(hdr.m_map_ranges_size);

	memcpy(m_map_ranges.data(), hdr.map_ranges_data(), hdr.map_ranges_bytes());

	m_map_indices.allocate(hdr.m_map_indices_size);

	memcpy(m_map_indices.data(), hdr.map_indices_data(), hdr.map_indices_bytes());

	m_image.allocate(m_width * m_height);

	memcpy(m_image.data(), hdr.image_data(), hdr.image_bytes());

	file.close();

	return {};
}

och::status glyph_atlas::save_bmp(const char* filename, bool overwrite_existing_file) const noexcept
{
	bitmap_file file;

	check(file.create(filename, overwrite_existing_file ? och::fio::open::truncate : och::fio::open::fail, m_width, m_height));

	for (uint32_t y = 0; y != m_height; ++y)
		for (uint32_t x = 0; x != m_width; ++x)
		{
			uint8_t v = m_image[x + (m_height - 1 - y) * m_width];

			file(x, y) = { v, v, v };
		}

	file.destroy();

	return {};
}

float glyph_atlas::line_height() const noexcept
{
	return m_line_height;
}

uint32_t glyph_atlas::width() const noexcept
{
	return m_width;
}

uint32_t glyph_atlas::height() const noexcept
{
	return m_height;
}

uint32_t glyph_atlas::glyph_scale() const noexcept
{
	return m_glyph_scale;
}

uint8_t* glyph_atlas::data() noexcept
{
	return m_image.data();
}

const uint8_t* glyph_atlas::raw_data() const noexcept
{
	return m_image.data();
}

image_view<uint8_t> glyph_atlas::view() noexcept
{
	return image_view(m_image.data(), m_width, 0, 0);
}

image_view<const uint8_t> glyph_atlas::view() const noexcept { return image_view(m_image.data(), m_width, 0, 0); };

glyph_atlas::glyph_index glyph_atlas::operator()(uint32_t codepoint) const noexcept
{
	int64_t lo = 0, hi = m_map_ranges.size() - 1;

	while (lo <= hi)
	{
		int64_t mid = lo + ((hi - lo) >> 1);

		const uint32_t beg_code = m_map_ranges[static_cast<uint32_t>(mid)].beg;

		const uint32_t end_code = m_map_ranges[static_cast<uint32_t>(mid)].end;

		if (beg_code <= codepoint && end_code >= codepoint)
			return m_map_indices[codepoint + m_map_ranges[static_cast<uint32_t>(mid)].offset];
		else if (beg_code > codepoint)
			hi = mid - 1;
		else if (end_code < codepoint)
			lo = mid + 1;
		else
			break;
	}

	return m_map_indices[0];
}

och::range<const uint8_t> glyph_atlas::get_mapper_ranges() const noexcept
{
	return och::range<const uint8_t>(reinterpret_cast<const uint8_t*>(m_map_ranges.data()), reinterpret_cast<const uint8_t*>(m_map_ranges.data() + m_map_ranges.size()));
}

och::range<const uint8_t> glyph_atlas::get_mapper_indices() const noexcept
{
	return och::range<const uint8_t>(reinterpret_cast<const uint8_t*>(m_map_indices.data()), reinterpret_cast<const uint8_t*>(m_map_indices.data() + m_map_indices.size()));
}

glyph_atlas::~glyph_atlas() noexcept
{

}
