#pragma once

#include <cstdint>

namespace och
{
	template<typename T>
	T clamp(const T& val, const T& min, const T& max) noexcept
	{
		if (val < min)
			return min;

		if (val > max)
			return max;

		return val;
	}

	template<typename T>
	T min(const T& v1, const T& v2) noexcept
	{
		return v1 < v2 ? v1 : v2;
	}

	template<typename T>
	T max(const T& v1, const T& v2) noexcept
	{
		return v1 > v2 ? v1 : v2;
	}

	template<typename V, typename M>
	bool contains_all(const V& v, const M& mask) noexcept
	{
		return (v & mask) == static_cast<V>(mask);
	}

	template<typename V, typename M>
	bool contains_none(const V& v, const M& mask) noexcept
	{
		return !(v & mask);
	}

	inline uint64_t next_pow2(uint64_t n) noexcept
	{
		n -= 1;

		n |= n >> 1;
		n |= n >> 2;
		n |= n >> 4;
		n |= n >> 8;
		n |= n >> 16;
		n |= n >> 32;

		return n + 1;
	}

	inline uint32_t next_pow2(uint32_t n)
	{
		n -= 1;

		n |= n >> 1;
		n |= n >> 2;
		n |= n >> 4;
		n |= n >> 8;
		n |= n >> 16;

		return n + 1;
	}

	inline uint16_t next_pow2(uint16_t n) noexcept
	{
		n -= 1;

		n |= n >> 1;
		n |= n >> 2;
		n |= n >> 4;
		n |= n >> 8;

		return n + 1;
	}

	inline uint8_t next_pow2(uint8_t n) noexcept
	{
		n -= 1;

		n |= n >> 1;
		n |= n >> 2;
		n |= n >> 4;

		return n + 1;
	}
}
