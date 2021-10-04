#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "och_err.h"

#include "bitmap.h"

struct binary_image
{
private:

	uint8_t* m_data;

	uint32_t m_width;

	uint32_t m_height;

	uint32_t m_stride;

public:

	using threshold_fn = bool (*) (texel_b8g8r8) noexcept;

	och::status load_bmp(const char* filename, threshold_fn cutoff = [](texel_b8g8r8 pixel) noexcept {return pixel.b || pixel.g || pixel.r; }) noexcept
	{
		bitmap_file file;

		check(file.create(filename));

		if (m_data)
			free(m_data);

		m_width = file.width();

		m_height = file.height();

		m_stride = stride_from_width(file.width());

		m_data = static_cast<uint8_t*>(malloc(m_height * m_stride));

		unset_all();

		for (uint32_t y = 0; y != m_height; ++y)
				for (uint32_t x = 0; x != m_width; ++x)
					if(cutoff(file(x, y)))
						set(x, y);

		file.destroy();

		return {};
	}

	och::status load_bim(const char* filename) noexcept
	{
		och::mapped_file file;
		
		check(file.create(filename, och::fio::access::read, och::fio::open::normal, och::fio::open::fail));

		if (m_data)
			free(m_data);

		memcpy(&m_width, file.data(), 4);

		memcpy(&m_height, file.data() + 4, 4);

		m_stride = stride_from_width(m_width);

		m_data = static_cast<uint8_t*>(malloc(m_height * m_stride));

		memcpy(m_data, file.data() + 8, m_height * m_stride);

		file.close();

		return {};
	}

	och::status save_bmp(const char* filename, texel_b8g8r8 set_colour = col::b8g8r8::white, texel_b8g8r8 unset_colour = col::b8g8r8::black, bool overwrite_existing_file = false) noexcept
	{
		bitmap_file file;
		
		check(file.create(filename, overwrite_existing_file ? och::fio::open::truncate : och::fio::open::fail, m_width, m_height));

		for (uint32_t y = 0; y != m_height; ++y)
			for (uint32_t x = 0; x != m_width; ++x)
				file(x, y) = get(x, y) ? set_colour : unset_colour;

		file.destroy();

		return {};
	}

	och::status save_bim(const char* filename, bool overwrite_existing_file = false)
	{
		och::mapped_file file;
		
		check(file.create(filename, och::fio::access::readwrite, overwrite_existing_file ? och::fio::open::truncate : och::fio::open::fail, och::fio::open::normal, m_stride * m_height + 8));

		memcpy(file.data(), &m_width, 4);

		memcpy(file.data() + 4, &m_height, 4);

		memcpy(file.data() + 8, m_data, m_stride * m_height);

		file.close();

		return {};
	}

	binary_image() : m_data{}, m_width{}, m_height{}, m_stride{} {};

	binary_image(uint32_t width, uint32_t height) : m_data{ static_cast<uint8_t*>(malloc(height * stride_from_width(width))) }, m_width{ width }, m_height{ height }, m_stride{ stride_from_width(width) } {}

	~binary_image() noexcept { free(m_data); }

	void set(uint32_t x, uint32_t y) noexcept { m_data[x / 8 + y * m_stride] |= 1 << (x & 7); }

	void unset(uint32_t x, uint32_t y) noexcept { m_data[x / 8 + y * m_stride] &= ~(1 << (x & 7)); }

	void flip(uint32_t x, uint32_t y) noexcept { m_data[x / 8 + y * m_stride] ^= 1 << (x & 7); }

	bool get(uint32_t x, uint32_t y) const noexcept { return m_data[x / 8 + y * m_stride] & (1 << (x & 7)); }

	uint32_t width() const noexcept { return m_width; }

	uint32_t height() const noexcept { return m_height; }

	void unset_all() noexcept
	{
		memset(m_data, 0, m_stride * m_height);
	}

	void set_all() noexcept
	{
		memset(m_data, 0xFF, m_stride * m_height);
	}

	uint8_t* data() noexcept { return m_data; }

	const uint8_t* data() const noexcept { return m_data; }

	operator bool() const noexcept { return m_data != nullptr; }

private:

	static uint32_t stride_from_width(uint32_t width) noexcept { return (width + 7) >> 3; }
};
