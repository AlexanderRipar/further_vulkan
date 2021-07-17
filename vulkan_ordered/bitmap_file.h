#pragma once

#include "bitmap_header.h"

#include "och_fio.h"

struct bitmap_file
{
private:

	static constexpr uint32_t image_data_offset = 64;

	static_assert(image_data_offset >= sizeof(bitmap_header));

	och::mapped_file<bitmap_header> m_file;

	uint8_t* m_image_data = nullptr;

	uint32_t m_stride = 0;

	uint32_t m_width = 0;

	uint32_t m_height = 0;

public:

	using point_op_fn = texel_b8g8r8(*) (texel_b8g8r8) noexcept;

	bitmap_file(const char* filename, uint32_t existing_mode = och::fio::open_normal, uint32_t new_width = 0, uint32_t new_height = 0) :
		m_file{ och::mapped_file<bitmap_header>(filename, och::fio::access_readwrite, existing_mode, new_width && new_height ? och::fio::open_normal : och::fio::open_fail, new_width && new_height ? bitmap_header::stride(new_width) * new_height + image_data_offset : 0) },
		m_image_data{ m_file ? m_file[0].image_offset ? m_file[0].raw_image_data() : reinterpret_cast<uint8_t*>(m_file.data()) + image_data_offset : nullptr },
		m_stride{ m_file ? m_file[0].image_offset ? bitmap_header::stride(m_file[0].width) : bitmap_header::stride(new_width) : 0 },
		m_width{ m_file ? m_file[0].image_offset ? m_file[0].width : new_width : 0 },
		m_height{ m_file ? m_file[0].image_offset ? m_file[0].height : new_height : 0 }
	{
		if (m_file && !m_file[0].image_offset)
			m_file[0].initialize(m_stride * m_height + image_data_offset, image_data_offset, m_width, m_height);
	}

	texel_b8g8r8& operator()(uint32_t x, uint32_t y) noexcept
	{
		return reinterpret_cast<texel_b8g8r8*>(m_image_data + y * m_stride)[x];
	}

	const texel_b8g8r8& operator()(uint32_t x, uint32_t y) const noexcept
	{
		return reinterpret_cast<const texel_b8g8r8*>(m_image_data + y * m_stride)[x];
	}

	uint32_t width() const noexcept
	{
		return m_width;
	}

	uint32_t height() const noexcept
	{
		return m_height;
	}

	uint8_t* raw_image_data() noexcept
	{
		return m_image_data;
	}

	const uint8_t* raw_image_data() const noexcept
	{
		return m_image_data;
	}

	void point_op(point_op_fn operation) noexcept
	{
		for (uint32_t y = 0; y != m_height; ++y)
			for (uint32_t x = 0; x != m_width; ++x)
				operator()(x, y) = operation(operator()(x, y));
	}

	operator bool() const noexcept { return m_file; }
};
