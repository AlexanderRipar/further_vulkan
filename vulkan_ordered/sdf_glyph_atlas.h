#pragma once

#include "och_error_handling.h"

#include "och_matmath.h"

#include "truetype.h"

struct codept_range
{
	uint32_t beg;

	uint32_t end;
};

struct glyph_index
{
	float m_x;

	float m_y;

	float m_x_size;

	float m_y_size;

	float m_advance_width;
};

struct glyph_atlas
{
	uint8_t* m_data;

	och::err_info create(const char* truetype_filename, float glyph_scale, uint32_t codept_range_cnt, const codept_range* codept_ranges) noexcept
	{
		struct glyph_size
		{
			uint16_t x_size;
			uint16_t y_size;
		};

		truetype_file file(truetype_filename);

		if (!file)
			return MSG_ERROR("Could not open file");

		uint32_t codept_cnt = 0;

		for (uint32_t i = 0; i != codept_range_cnt; ++i)
			codept_cnt += codept_ranges[i].end - codept_ranges[i].end;

		glyph_size* glyph_sizes = static_cast<glyph_size*>(malloc(codept_cnt * sizeof(glyph_size)));

		uint32_t curr_glyph = 0;

		for(uint32_t i = 0; i != codept_range_cnt; ++i)
			for (uint32_t cp = codept_ranges[i].beg; cp != codept_ranges[i].end; ++cp)
			{
				glyph_metrics metrics = file.get_glyph_metrics(cp);

				glyph_sizes[curr_glyph++] = { static_cast<uint16_t>(metrics.x_size() * glyph_scale), static_cast<uint16_t>(metrics.y_size() * glyph_scale) };
			}



		return {};
	}

	och::err_info destroy() noexcept
	{
		return {};
	}


};