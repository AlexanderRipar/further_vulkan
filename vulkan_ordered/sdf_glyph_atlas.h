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

	struct size_helper
	{
		uint32_t x;
		uint32_t y;
		uint32_t w;
		uint32_t h;
		uint32_t glyph_id;
	};

	struct glyph_location
	{
		och::vec2 pos;

		och::vec2 size;

		float advance_width;
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

		och::heap_buffer<uint32_t> glyph_ids(codept_cnt + 1);

		glyph_ids[0] = 0;

		uint32_t curr_id_idx = 1;

		for (auto& r : codept_ranges)
			for (uint32_t cp = r.beg; cp != r.beg + r.cnt; ++cp)
				glyph_ids[curr_id_idx++] = file.get_glyph_id_from_codept(cp);

		sort_by_id(glyph_ids);

		// Dedupe ids
		{
			uint32_t unique_id_cnt = 0;

			uint32_t prev_id = ~0u;

			for (uint32_t i = 0; i != glyph_ids.size(); ++i)
			{
				uint32_t curr_id = glyph_ids[i];

				if (curr_id != prev_id)
				{
					glyph_ids[unique_id_cnt++] = curr_id;

					prev_id = curr_id;
				}
			}

			glyph_ids.shrink(unique_id_cnt);
		}

		// Load all necessary glyph sizes by id

		och::heap_buffer<uint8_t> sdf_buffer;

		uint32_t buffer_w = map_width, buffer_h = 0;

		och::heap_buffer<size_helper> glyph_sizes(glyph_ids.size());

		uint32_t curr_glyph_idx = 0;

		const float padding_factor = 1.0F + sdf_clamp * 2.0F;

		for (uint32_t i = 0; i != glyph_sizes.size(); ++i)
		{
			glyph_metrics mtx = file.get_glyph_metrics_from_id(glyph_ids[i]);
			
			const uint32_t w = static_cast<uint32_t>(mtx.x_size() * glyph_scale * padding_factor);

			const uint32_t h = static_cast<uint32_t>(mtx.y_size() * glyph_scale * padding_factor);

			if (!w || !h)
				continue;

			if (w > buffer_w)
				buffer_w = w;

			glyph_sizes[curr_glyph_idx++] = { 0, 0, w, h, glyph_ids[i] };
		}

		glyph_sizes.shrink(curr_glyph_idx);

		sort_by_size(glyph_sizes);

		// Find an arrangement for the given glyph sizes and allocate a correspondingly sized buffer
		
		{
			uint32_t curr_x = 0;
			
			uint32_t curr_y = glyph_sizes[0].h;

			for (auto& s : glyph_sizes)
			{
				if (curr_x + s.w > buffer_w)
				{
					curr_y += s.h;

					curr_x = s.w;
				}
				else
					curr_x += s.w;

				s.x = curr_x - s.w;

				s.y = curr_y - s.h;
			}

			buffer_h = och::max(curr_y, 1u);

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

		for (auto& s : glyph_sizes)
		{
			const glyph_data glyph = file.get_glyph_data_from_id(s.glyph_id);

			image_view sdf_view(sdf_buffer.data(), buffer_w, s.x, s.y);

			sdf_from_glyph(sdf_view, glyph, s.w, s.h, sdf_mapper);

			uint32_t min_x = ~0u, max_x = 0, min_y = ~0u, max_y = 0;

			for(uint32_t y = 0; y != s.h; ++y)
				for (uint32_t x = 0; x != s.w; ++x)
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

			if (min_x == ~0u && max_x == 0)
				continue;

			if (max_x - min_x > map_width || max_y - min_y > map_width)
				return MSG_ERROR("Glyph too large for the given map_width parameter");

			glyph_sizes[final_size_idx].x = min_x;

			glyph_sizes[final_size_idx].y = min_y;

			++final_size_idx;
		}

		glyph_sizes.shrink(final_size_idx);

		sort_by_size(glyph_sizes);

		// Find a new arrangement for the reduced glyph sizes and allocate a correspondingly sized buffer (the final sdf)

		och::heap_buffer<size_helper> final_sizes(glyph_sizes.size());

		{
			uint32_t curr_x = 0;

			uint32_t curr_y = glyph_sizes[0].h;

			for (uint32_t i = 0; i != glyph_sizes.size(); ++i)
			{
				const size_helper& s = glyph_sizes[i];

				if (curr_x + s.w > map_width)
				{
					curr_y += s.h;

					curr_x = s.w;
				}
				else
					curr_x += s.w;

				final_sizes[i] = { curr_x - s.w, curr_y - s.h, s.w, s.h, s.glyph_id };
			}

			m_height = och::max(curr_y, 1u);

			m_width = map_width;

			m_image.allocate(m_width * m_height);

			memset(m_image.data(), 0x7F, m_width* m_height);
		}

		// copy from buffer to final image.
		{
			for (uint32_t i = 0; i != glyph_sizes.size(); ++i)
			{
				const size_helper& b = glyph_sizes[i];

				const size_helper& f = final_sizes[i];

				for(uint32_t y = 0; y != b.h; ++y)
					for (uint32_t x = 0; x != b.w; ++x)
						m_image[x + f.x + (y + f.y) * map_width] = sdf_buffer[x + b.x + (y + b.y) * buffer_w];
			}
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

	static void sort_by_id(och::heap_buffer<uint32_t>& glyph_ids) noexcept
	{
		och::heap_buffer<uint32_t> swap_buffer(glyph_ids.size());

		{
			uint32_t id_cnts_0[256]{};

			for (auto& id : glyph_ids)
				++id_cnts_0[id & 0xFF];

			uint32_t prev_sum = 0;

			for (auto& cnt : id_cnts_0)
			{
				uint32_t tmp = cnt;

				cnt = prev_sum;

				prev_sum += tmp;
			}

			for (auto& id : glyph_ids)
				swap_buffer[id_cnts_0[id & 0xFF]++] = id;
		}

		{
			uint32_t id_cnts_1[256]{};

			for (auto& id : swap_buffer)
				++id_cnts_1[(id >> 8) & 0xFF];

			uint32_t prev_sum = 0;

			for (auto& cnt : id_cnts_1)
			{
				uint32_t tmp = cnt;

				cnt = prev_sum;

				prev_sum += tmp;
			}

			for (auto& id : swap_buffer)
				glyph_ids[id_cnts_1[(id >> 8) & 0xFF]++] = id;
		}

		{
			uint32_t id_cnts_2[256]{};

			for (auto& id : glyph_ids)
				++id_cnts_2[(id >> 16) & 0xFF];

			uint32_t prev_sum = 0;

			for (auto& cnt : id_cnts_2)
			{
				uint32_t tmp = cnt;

				cnt = prev_sum;

				prev_sum += tmp;
			}

			for (auto& id : glyph_ids)
				swap_buffer[id_cnts_2[(id >> 16) & 0xFF]++] = id;
		}

		glyph_ids.attach(swap_buffer.detach());
	}

	static void sort_by_id(och::heap_buffer<size_helper>& glyph_sizes) noexcept
	{
		och::heap_buffer<size_helper> swap_buffer(glyph_sizes.size());

		{
			uint32_t id_cnts_0[256]{};

			for (auto& sz : glyph_sizes)
				++id_cnts_0[sz.glyph_id & 0xFF];

			uint32_t prev_sum = 0;

			for (auto& cnt : id_cnts_0)
			{
				uint32_t tmp = cnt;

				cnt = prev_sum;

				prev_sum += tmp;
			}

			for (auto& sz : glyph_sizes)
				swap_buffer[id_cnts_0[sz.glyph_id & 0xFF]++] = sz;
		}

		{
			uint32_t id_cnts_1[256]{};

			for (auto& sz : swap_buffer)
				++id_cnts_1[(sz.glyph_id >> 8) & 0xFF];

			uint32_t prev_sum = 0;

			for (auto& cnt : id_cnts_1)
			{
				uint32_t tmp = cnt;

				cnt = prev_sum;

				prev_sum += tmp;
			}

			for (auto& sz : swap_buffer)
				glyph_sizes[id_cnts_1[(sz.glyph_id >> 8) & 0xFF]++] = sz;
		}

		{
			uint32_t id_cnts_2[256]{};

			for (auto& sz : glyph_sizes)
				++id_cnts_2[(sz.glyph_id >> 16) & 0xFF];

			uint32_t prev_sum = 0;

			for (auto& cnt : id_cnts_2)
			{
				uint32_t tmp = cnt;

				cnt = prev_sum;

				prev_sum += tmp;
			}

			for (auto& sz : glyph_sizes)
				swap_buffer[id_cnts_2[(sz.glyph_id >> 16) & 0xFF]++] = sz;
		}

		glyph_sizes.attach(swap_buffer.detach());
	}

	static void sort_by_size(och::heap_buffer<size_helper>& glyph_sizes) noexcept
	{
		och::heap_buffer<size_helper> swap_buffer(glyph_sizes.size());

		{
			uint32_t x_cnts[MAX_GLYPH_SIZE]{};

			for (auto& s : glyph_sizes)
				++x_cnts[MAX_GLYPH_SIZE - s.w];

			uint32_t prev_sum = 0;

			for (auto& cnt : x_cnts)
			{
				uint32_t tmp = cnt;

				cnt = prev_sum;

				prev_sum += tmp;
			}

			for (auto& s : glyph_sizes)
				swap_buffer[x_cnts[MAX_GLYPH_SIZE - s.w]++] = s;
		}

		{
			uint32_t y_cnts[MAX_GLYPH_SIZE]{};

			for (auto& s : swap_buffer)
				++y_cnts[MAX_GLYPH_SIZE - s.h];

			uint32_t prev_sum = 0;

			for (auto& cnt : y_cnts)
			{
				uint32_t tmp = cnt;

				cnt = prev_sum;

				prev_sum += tmp;
			}

			for (auto& s : swap_buffer)
				glyph_sizes[y_cnts[MAX_GLYPH_SIZE - s.h]++] = s;
		}
	}

	static const size_helper& find_by_id(const och::heap_buffer<size_helper>& glyph_sizes, uint32_t glyph_id)
	{
		int64_t lo = 0, hi = glyph_sizes.size() - 1;

		while (lo <= hi)
		{
			int64_t mid = lo + ((hi - lo) >> 1);

			uint32_t mid_id = glyph_sizes[static_cast<uint32_t>(mid)].glyph_id;

			if (mid_id == glyph_id)
				return glyph_sizes[static_cast<uint32_t>(mid)];
			else if (mid_id > glyph_id)
				hi = mid - 1;
			else
				lo = mid + 1;
		}

		return glyph_sizes[0];
	}
};
