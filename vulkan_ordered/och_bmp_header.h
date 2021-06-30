#pragma once

#include <cstdint>

#pragma pack(push, 1)
struct bitmap_header
{
	//Header
	char header_field[2];
	uint32_t size_bytes;
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

	uint8_t* pixel_data()
	{
		return reinterpret_cast<uint8_t*>(this) + image_offset;
	}

	const uint8_t* pixel_data() const
	{
		return reinterpret_cast<const uint8_t*>(this) + image_offset;
	}
};
#pragma pack(pop)
