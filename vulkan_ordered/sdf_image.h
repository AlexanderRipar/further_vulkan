#pragma once

#include <cstdint>

#include <cstdlib>

#include "och_error_handling.h"

#include "och_fio.h"

#include "bitmap.h"

#include "binary_image.h"

struct sdf_image
{
private:

	struct point
	{
		int32_t dx, dy;

		int32_t d_sq() const noexcept { return dx * dx + dy * dy; }
	};

	static constexpr point INSIDE = { 0, 0 }, OUTSIDE = { 0x3FFF, 0x3FFF };

	float* m_data = nullptr;

	int32_t m_width = 0;

	int32_t m_height = 0;

public:

	using colour_mapper_fn = texel_b8g8r8(*) (float) noexcept;

	och::err_info from_bim(const binary_image& img)
	{
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

		for(int32_t y = 0; y != m_height; ++y)
			for (int32_t x = 0; x != m_width; ++x)
			{
				float pos_dst = sqrtf(static_cast<float>(pos[x + y * m_width].d_sq()));
				
				float neg_dst = sqrtf(static_cast<float>(neg[x + y * m_width].d_sq()));

				m_data[x + y * m_width] = pos_dst - neg_dst;
			}

		free(pos);

		free(neg);

		const float normalization_fct = 1.0F / och::max(m_width, m_height);

		for (int32_t y = 0; y != m_height; ++y)
			for (int32_t x = 0; x != m_width; ++x)
				m_data[x + y * m_width] *= normalization_fct;

		return {};
	}

	och::err_info save_bmp(const char* filename, bool overwrite_existing_file = false, colour_mapper_fn colour_mapper = [](float dst) noexcept { uint8_t c = static_cast<uint8_t>((-0.1F / ((dst < 0.0F ? -dst : dst) + 0.1F) + 1.0F) * 256.0F); return dst < 0.0F ? texel_b8g8r8(c, c >> 1, 0) : texel_b8g8r8(c, c, c); })
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
};
