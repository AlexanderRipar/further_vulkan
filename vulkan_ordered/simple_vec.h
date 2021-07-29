#pragma once

#include <cstdint>
#include <cstdlib>

#include "och_helpers.h"

template<typename T>
struct simple_vec
{
private:

	T* m_data;

	uint32_t m_size;

	uint32_t m_capacity;

public:

	simple_vec(uint32_t initial_capacity) noexcept : m_data{ static_cast<T*>(malloc(och::next_pow2(initial_capacity) * sizeof(T))) }, m_size{ 0 }, m_capacity{ och::next_pow2(initial_capacity) } {}

	~simple_vec() noexcept { free(m_data); }

	void add(const T& t) noexcept
	{
		assert_capacity(m_size + 1);

		m_data[m_size++] = t;
	}

	void add(T&& t) noexcept
	{
		assert_capacity(m_size + 1);

		m_data[m_size++] = t;
	}

	const T& operator[](uint32_t idx) const noexcept
	{
		return m_data[idx];
	}

	T& operator[](uint32_t idx) noexcept
	{
		return m_data[idx];
	}

	uint32_t size() const noexcept
	{
		return m_size;
	}

	uint32_t capacity() const noexcept
	{
		return m_capacity;
	}

	void reserve(uint32_t min_capacity) const noexcept
	{
		assert_capacity(min_capacity);
	}

	const T* begin() const noexcept
	{
		return m_data;
	}

	const T* end() const noexcept
	{
		return m_data + m_size;
	}

	T* begin() noexcept
	{
		return m_data;
	}

	T* end() noexcept
	{
		return m_data + m_size;
	}

	void reset() noexcept
	{
		m_size = 0;
	}

private:

	void assert_capacity(uint32_t requested) noexcept
	{
		if (requested < m_capacity)
		{
			m_capacity = och::next_pow2(requested);

			m_data = realloc(m_data, m_capacity);
		}
	}
};