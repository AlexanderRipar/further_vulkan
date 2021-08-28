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
private:

	static constexpr uint32_t MAX_GLYPH_SIZE = 256;

	static constexpr uint32_t MAX_GLYPH_CNT = 1 << 16;

	struct pos_helper
	{
		uint16_t bx;
		uint16_t by;
		uint16_t w;
		uint16_t h;
		uint16_t fx;
		uint16_t fy;
		uint32_t glyph_id;
		uint32_t codept;
	};

	struct glyph_location
	{
		och::vec2 pos;

		och::vec2 size;

		float advance_width;
	};

	struct codept_id_pair
	{
		uint32_t codept;
		uint32_t glyph_id;
	};

	struct codept_mapper
	{
		struct mapper_range
		{
			uint32_t beg;
			uint32_t end;
			uint32_t offset;
		};

		och::heap_buffer<mapper_range> glyph_ranges;

		och::heap_buffer<glyph_location> glyph_locations;

		glyph_location operator()(uint32_t codepoint) const noexcept
		{
			int64_t lo = 0, hi = glyph_ranges.size() - 1;
			
			uint32_t location_index = 0;

			while (lo <= hi)
			{
				int64_t mid = lo + ((hi - lo) >> 1);

				const uint32_t beg_code = glyph_ranges[static_cast<uint32_t>(mid)].beg;

				const uint32_t end_code = glyph_ranges[static_cast<uint32_t>(mid)].end;

				if (beg_code <= codepoint && end_code >= codepoint)
					location_index = codepoint - beg_code - glyph_ranges[static_cast<uint32_t>(mid)].offset;
				else if (beg_code > codepoint)
					hi = mid - 1;
				else if (end_code < codepoint)
					lo = mid + 1;
				else
					break;
			}

			return glyph_locations[location_index];
		}
	};

	uint32_t m_width = 0;

	uint32_t m_height = 0;

	och::heap_buffer<uint8_t> m_image;

	codept_mapper m_mapper;

public:

	och::err_info create(const char* truetype_filename, float glyph_scale, uint32_t glyph_padding_pixels, float sdf_clamp, uint32_t map_width, const och::range<codept_range> codept_ranges) noexcept
	{
		// Open file

		truetype_file file(truetype_filename);

		if (!file)
			return MSG_ERROR("Could not open file");

		uint32_t codept_cnt = 0;

		for (auto& r : codept_ranges)
			codept_cnt += r.cnt;

		// Get glyph ids

		och::heap_buffer<pos_helper> sizes(codept_cnt + 1);

		sizes[0].glyph_id = 0;

		sizes[0].codept = 0;

		uint32_t curr_id_idx = 1;

		for (auto& r : codept_ranges)
			for (uint32_t cp = r.beg; cp != r.beg + r.cnt; ++cp)
			{
				sizes[curr_id_idx].glyph_id = file.get_glyph_id_from_codept(cp);

				sizes[curr_id_idx].codept = cp;

				++curr_id_idx;
			}

		sort<offsetof(pos_helper, glyph_id), sizeof(pos_helper::glyph_id)>(sizes);

		// Dedupe ids
		{
			uint32_t unique_id_cnt = 0;

			uint32_t prev_id = ~0u;

			for (uint32_t i = 0; i != sizes.size(); ++i)
			{
				uint32_t curr_id = sizes[i].glyph_id;

				if (curr_id != prev_id)
				{
					sizes[unique_id_cnt++] = sizes[i];

					prev_id = curr_id;
				}
			}

			sizes.shrink(unique_id_cnt);
		}

		// Load all necessary glyph sizes by id

		och::heap_buffer<uint8_t> sdf_buffer;

		uint16_t buffer_w = static_cast<uint16_t>(map_width), buffer_h = 0;

		uint32_t curr_glyph_idx = 0;

		const float padding_factor = 1.0F + sdf_clamp * 2.0F;

		for (uint32_t i = 0; i != sizes.size(); ++i)
		{
			glyph_metrics mtx = file.get_glyph_metrics_from_id(sizes[i].glyph_id);
			
			const uint32_t w = static_cast<uint32_t>(mtx.x_size() * glyph_scale);

			const uint32_t h = static_cast<uint32_t>(mtx.y_size() * glyph_scale);

			if (!w || !h)
				continue;

			if (w > UINT16_MAX || h > UINT16_MAX)
				return MSG_ERROR("Glyph side length larger than 2^16 - 1");

			if (static_cast<uint16_t>(w) > buffer_w)
				buffer_w = static_cast<uint16_t>(w);

			sizes[curr_glyph_idx].w = static_cast<uint16_t>(w);
			sizes[curr_glyph_idx].h = static_cast<uint16_t>(h);

			++curr_glyph_idx;
		}

		sizes.shrink(curr_glyph_idx);

		sort<offsetof(pos_helper, w), 4, true>(sizes);

		// Find an arrangement for the given glyph sizes and allocate a correspondingly sized buffer
		{
			uint16_t curr_x = 0;
			
			uint16_t curr_y = sizes[0].h;

			for (auto& s : sizes)
			{
				if (curr_x + s.w > buffer_w)
				{
					curr_y += s.h;

					curr_x = s.w;
				}
				else
					curr_x += s.w;

				s.bx = curr_x - s.w;

				s.by = curr_y - s.h;
			}

			buffer_h = och::max(curr_y, static_cast<uint16_t>(1));

			sdf_buffer.allocate(buffer_w * buffer_h);
		}

		// Actually draw the sdfs into the buffer while also creating a heap_buffer of size_helpers with the actual glyph size.

		const struct
		{
			const float clamp;

			uint8_t operator()(float dst) const noexcept
			{
				const float clamped = dst < -clamp ? -clamp : dst > clamp ? clamp : dst;

				return static_cast<uint8_t>((clamped + clamp) * (127.5F / clamp));
			}
		} sdf_mapper{ sdf_clamp };

		uint32_t final_size_idx = 0;

		for (auto& s : sizes)
		{
			const glyph_data glyph = file.get_glyph_data_from_id(s.glyph_id);

			image_view sdf_view(sdf_buffer.data(), buffer_w, s.bx, s.by);

			sdf_from_glyph(sdf_view, glyph, s.w, s.h, sdf_mapper);

			uint16_t min_x = 0xFFFF, max_x = 0, min_y = 0xFFFF, max_y = 0;

			for(uint16_t y = 0; y != s.h; ++y)
				for (uint16_t x = 0; x != s.w; ++x)
				{
					if (sdf_view(x, y) != 0)
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
				}

			if (max_x == 0 || min_x == 0xFFFF || max_y == 0 || min_y == 0xFFFF)
				continue;

			if (max_x - min_x > static_cast<uint16_t>(map_width) || max_y - min_y > static_cast<uint16_t>(map_width))
				return MSG_ERROR("Glyph too large for the given map_width parameter");

			pos_helper& f = sizes[final_size_idx++];

			f.bx += min_x;
			f.by += min_y;
			f.w = max_x - min_x + 1 + 2 * static_cast<uint16_t>(glyph_padding_pixels);
			f.h = max_y - min_y + 1 + 2 * static_cast<uint16_t>(glyph_padding_pixels);
		}

		sizes.shrink(final_size_idx);

		sort<offsetof(pos_helper, w), 4, true>(sizes);

		// Find a new arrangement for the reduced glyph sizes and allocate a correspondingly sized buffer (the final sdf)

		{
			uint16_t curr_x = 0;

			uint16_t curr_y = sizes[0].h;

			for (auto& s : sizes)
			{
				if (curr_x + s.w > static_cast<uint16_t>(map_width))
				{
					curr_y += s.h;

					curr_x = s.w;
				}
				else
					curr_x += s.w;
				
				s.fx = curr_x - s.w;

				s.fy = curr_y - s.h;
			}

			m_height = och::max(curr_y, static_cast<uint16_t>(1));

			m_width = map_width;

			m_image.allocate(m_width * m_height);

			memset(m_image.data(), 0x00, m_width * m_height);
		}

		// copy from buffer to final image.
		for (auto& s : sizes)
			for (uint32_t y = 0; y != s.h - 2 * glyph_padding_pixels; ++y)
				for (uint32_t x = 0; x != s.w - 2 * glyph_padding_pixels; ++x)
				{
					const uint8_t dst = sdf_buffer[x + s.bx + (y + s.by) * buffer_w];

					m_image[x + s.fx + glyph_padding_pixels + (glyph_padding_pixels + y + s.fy) * map_width] = dst;
				}

		// TODO: Take care of mapper-stuff

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

private:

	template<uint32_t offset, uint32_t bytes, bool reverse = false, typename T>
	static void sort(och::heap_buffer<T>& input)
	{
		static_assert(offset + bytes < sizeof(T));

		const uint32_t sz = input.size();

		och::heap_buffer<T> swap_buffer(sz);

		for (uint32_t b = 0; b != bytes; ++b)
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
