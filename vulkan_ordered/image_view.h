#pragma once

#include <cstdint>

template<typename T>
struct image_view
{
private:

	T* m_full_image;

	uint32_t m_stride;

	uint32_t m_offset_x;

	uint32_t m_offset_y;

public:

	image_view(T* full_image, uint32_t stride, uint32_t offset_x, uint32_t offset_y) : m_full_image{ full_image }, m_stride{ stride }, m_offset_x{ offset_x }, m_offset_y{ offset_y } {}

	T& operator()(uint32_t x, uint32_t y) noexcept
	{
		return m_full_image[m_offset_x + x + (m_offset_y + y) * m_stride];
	}

	const T& operator()(uint32_t x, uint32_t y) const noexcept
	{
		return m_full_image[m_offset_x + x + (m_offset_y + y) * m_stride];
	}
};
