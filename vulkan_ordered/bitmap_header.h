#pragma once

#include <cstdint>

#include "texels.h"

struct bitmap_image_data
{
	uint8_t* m_data;

	uint32_t m_stride;

	bitmap_image_data(uint8_t* data, uint32_t stride) noexcept : m_data{ data }, m_stride{ stride } {}

	texel_b8g8r8& operator()(uint32_t x, uint32_t y) noexcept
	{
		return reinterpret_cast<texel_b8g8r8*>(m_data + y * m_stride)[x];
	}

	const texel_b8g8r8& operator()(uint32_t x, uint32_t y) const noexcept
	{
		return reinterpret_cast<const texel_b8g8r8*>(m_data + y * m_stride)[x];
	}
};

#pragma pack(push, 1)
struct bitmap_header
{
	//Header
	char header_field[2];
	uint32_t file_size_bytes;
	uint16_t application_specific_1;
	uint16_t application_specific_2;
	uint32_t image_offset;

	//DIB (Info-Header)
	uint32_t header_bytes;
	int32_t width;
	int32_t height;
	uint16_t colour_planes;
	uint16_t bits_per_pixel;
	uint32_t compression_method;
	uint32_t image_size;
	uint32_t horizontal_resolution;
	uint32_t vertical_resolution;
	uint32_t colour_palette_count;
	uint32_t important_colour_count;

	bitmap_image_data image_data() noexcept
	{
		return { reinterpret_cast<uint8_t*>(this) + image_offset, stride(width) };
	}

	uint8_t* raw_image_data() noexcept
	{
		return { reinterpret_cast<uint8_t*>(this) + image_offset };
	}

	static uint32_t allocation_size(uint32_t image_offset, uint32_t width, uint32_t height) noexcept
	{
		return image_offset + height * stride(width);
	}

	static uint32_t stride(uint32_t width) noexcept
	{
		return (width * 3 + 3) & ~3;
	}

	void initialize(uint32_t file_sz, uint32_t image_data_offset, uint32_t image_width, uint32_t image_height) noexcept
	{
		header_field[0] = 'B';
		header_field[1] = 'M';
		file_size_bytes = file_sz;
		application_specific_1 = 0;
		application_specific_2 = 0;
		image_offset = image_data_offset;
		header_bytes = 40;
		width = image_width;
		height = image_height;
		colour_planes = 1;
		bits_per_pixel = 24;
		compression_method = 0;
		image_size = 0;
		horizontal_resolution = 3780;
		vertical_resolution = 3780;
		colour_palette_count = 0;
		important_colour_count = 0;
	}
};
#pragma pack(pop)
