#pragma once

#include "och_error_handling.h"

#include "och_matmath.h"

#include "truetype.h"

#include "och_heap_buffer.h"

#include "och_fmt.h"

#include "image_view.h"

struct codept_range
{
	uint32_t beg;

	uint32_t cnt;
};

struct glyph_atlas
{
public:

	struct glyph_index
	{
		och::vec2 position;

		och::vec2 size;

		och::vec2 advance;
	};

private:

	struct glyph_address
	{
		uint32_t glyph_id;
		uint32_t begin;
		uint32_t stride;
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

	struct glyph_dimension
	{
		uint32_t glyph_id;
		float w;
		float h;
	};

	struct mapper_range
	{
		uint32_t beg;
		uint32_t end;
		int32_t offset;
	};

	uint32_t m_width = 0;

	uint32_t m_height = 0;

	och::heap_buffer<uint8_t> m_image;

	och::heap_buffer<mapper_range> m_map_ranges;

	och::heap_buffer<glyph_index> m_map_indices;

public:

	// TODO: 
	// Calculate advance to save in m_map_indices
	// Implement mapping equivalent glyphs to a single spot in the image.
	// Fix wrong sizing of glyphs. bad :(
	och::err_info create(const char* truetype_filename, uint32_t glyph_size, uint32_t glyph_padding_pixels, float sdf_clamp, uint32_t map_width, const och::range<codept_range> codept_ranges) noexcept
	{
		// Open file

		truetype_file file(truetype_filename);

		if (!file)
			return MSG_ERROR("Could not open file");

		uint32_t codept_cnt = 0;

		for (auto& r : codept_ranges)
			codept_cnt += r.cnt;

		// Create list of (unique) codepoints with their matching glyph ids.

		och::heap_buffer<codept_id_pair> cp_ids(codept_cnt + 1);

		{
			cp_ids[0].codept = ~0u;

			uint32_t curr_cp_id = 1;

			for (const auto& r : codept_ranges)
				for (uint32_t cp = r.beg; cp != r.beg + r.cnt; ++cp)
					cp_ids[curr_cp_id++].codept = cp;

			sort<offsetof(codept_id_pair, codept), 3>(cp_ids);

			uint32_t prev_cp = ~0u;

			uint32_t curr_idx = 1;

			for (uint32_t i = 1; i != cp_ids.size(); ++i)
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

		och::heap_buffer<uint32_t> ids(cp_ids.size());

		{
			for (uint32_t i = 0; i != ids.size(); ++i)
				ids[i] = cp_ids[i].glyph_id;

			sort<0, 4>(ids);

			uint32_t curr_idx = 0;

			uint32_t prev_id = ~0u;

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


		och::heap_buffer<uint8_t> sdf_buffer(ids.size() * padded_glyph_size * padded_glyph_size);

		och::heap_buffer<glyph_address> addresses(ids.size());

		{
			uint32_t curr_beg = 0;

			uint32_t curr_idx = 0;

			for (auto& id : ids)
			{
				glyph_data glyph = file.get_glyph_data_from_id(id);

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

					uint32_t beg = curr_beg + min_x + min_y * padded_glyph_size;

					addresses[curr_idx++] = { id, beg, padded_glyph_size, w, h };

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
					return MSG_ERROR("Glyph too large for given map_width parameter");

				if (curr_x + padded_w > map_width - glyph_padding_pixels)
				{
					curr_y += padded_h;
		
					curr_x = padded_w;
				}
				else
					curr_x +=padded_w;
		
				a.x = curr_x - padded_w;
		
				a.y = curr_y - padded_h;
			}
		
			m_width = map_width;

			m_height = och::max(curr_y, 1u);
		
			m_image.allocate(m_width * m_height);

			memset(m_image.data(), 0x00, m_width * m_height);
		}
		
		// Copy glyphs from buffer to final image
		{
			for (auto& a : addresses)
				for(uint32_t y = 0; y != a.h; ++y)
					for (uint32_t x = 0; x != a.w; ++x)
					{
						uint8_t v = sdf_buffer[a.begin + x + y * a.stride];

						m_image[a.x + x + glyph_padding_pixels + (a.y + glyph_padding_pixels + y) * m_width] = v;
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

			for (uint32_t i = 1; i != cp_ids.size(); ++i)
				if (cp_ids[i].codept != prev_cp + 1)
				{
					m_map_ranges[curr_idx].end = prev_cp;

					++curr_idx;

					prev_cp = cp_ids[i].codept;

					m_map_ranges[curr_idx].beg = prev_cp;

					m_map_ranges[curr_idx].offset = static_cast<int32_t>(i - cp_ids[i].codept);
				}
				else
					++prev_cp;
		}

		m_map_indices.allocate(cp_ids.size());

		// Generate codepoint indices
		{
			sort<offsetof(glyph_address, glyph_id), 4>(addresses);

			const float inv_width = 1.0F / static_cast<float>(m_width);

			const float inv_height = 1.0F / static_cast<float>(m_height);

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

							const float x = static_cast<float>(a.x) * inv_width;

							const float y = static_cast<float>(a.y) * inv_height;

							const float w = static_cast<float>(a.w) * inv_width;

							const float h = static_cast<float>(a.h) * inv_height;

							// TODO: Calculate advance widths
							const float adv_x = 0.0F;

							const float adv_y = 0.0F;

							m_map_indices[i] = { {x, y}, {w, h}, {adv_x, adv_y} };

							break;
						}
						else if (mid_id > curr_id)
							hi = mid - 1;
						else
							lo = mid + 1;
					}

					if (lo > hi)
						m_map_indices[i] = { {0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F} };
				}
			}
		}

		return {};
	}

	void destroy() noexcept
	{
		
	}

	och::err_info save_bmp(const char* filename, bool overwrite_existing_file = false) const noexcept
	{
		bitmap_file file(filename, overwrite_existing_file ? och::fio::open_truncate : och::fio::open_fail, m_width, m_height);

		if (!file)
			return MSG_ERROR("Could not open file");

		for (uint32_t y = 0; y != m_height; ++y)
			for (uint32_t x = 0; x != m_width; ++x)
			{
				uint8_t v = m_image[x + y * m_width];

				file(x, y) = { v, v, v };
			}

		return {};
	}

	glyph_index operator()(uint32_t codepoint) const noexcept
	{
		int64_t lo = 0, hi = m_map_ranges.size() - 1;

		uint32_t location_index = 0;

		while (lo <= hi)
		{
			int64_t mid = lo + ((hi - lo) >> 1);

			const uint32_t beg_code = m_map_ranges[static_cast<uint32_t>(mid)].beg;

			const uint32_t end_code = m_map_ranges[static_cast<uint32_t>(mid)].end;

			if (beg_code <= codepoint && end_code >= codepoint)
				location_index = codepoint - beg_code - m_map_ranges[static_cast<uint32_t>(mid)].offset;
			else if (beg_code > codepoint)
				hi = mid - 1;
			else if (end_code < codepoint)
				lo = mid + 1;
			else
				break;
		}

		return m_map_indices[location_index];
	}

private:

	template<ptrdiff_t offset, size_t bytes, bool reverse = false, typename T>
	static void sort(och::heap_buffer<T>& input)
	{
		static_assert(offset + bytes <= sizeof(T));

		const uint32_t sz = input.size();

		och::heap_buffer<T> swap_buffer(sz);

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
};
