#pragma once

namespace och
{
	template<typename T>
	struct heap_buffer
	{
		T* m_buf = nullptr;

		heap_buffer(uint32_t cnt) noexcept { m_buf = new T[cnt]; }

		~heap_buffer() noexcept { delete[] m_buf; }

		T* data() noexcept { return m_buf; }

		const T* data()	const noexcept { return m_buf; }

		T& operator[](uint32_t n) noexcept { return m_buf[n]; }

		const T& operator[](uint32_t n) const noexcept { return m_buf[n]; }
	};
}