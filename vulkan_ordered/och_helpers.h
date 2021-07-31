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

	template<typename T>
	T abs(const T& v) noexcept
	{
		return v < static_cast<T>(0) ? -v : v;
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

	static inline void cubic_poly_roots(float a3, float a2, float a1, float a0, float& r0, float& r1, float& r2) noexcept
	{
		constexpr float one_third = 1.0F / 3.0F;

		constexpr float pi = 3.14159265359F;

		const float a3_inv = 1.0F / a3;

		a2 *= a3_inv;

		a1 *= a3_inv;

		a0 *= a3_inv;

		const float p = a1 - (a2 * a2) * one_third;

		const float q = a2 * a2 * a2 * (2.0F / 27.0F) - a2 * a1 * one_third + a0;

		const float d = q * q * (1.0F / 4.0F) + p * p * p * (1.0F / 27.0F);

		r0 = r1 = r2 = INFINITY;

		if (d < -1e-7F) // d < 0
		{
			const float r = sqrtf(-p * p * p * (1.0F / 27.0F));

			const float alpha_raw = atanf(sqrtf(-d) / -q * 2.0F);

			const float alpha = q > 0.0F ? 2.0F * pi - alpha_raw : alpha_raw;

			r0 = cbrtf(r) * (cosf((6.0F * pi - alpha) * one_third) + cosf((            alpha) * one_third)) - a2 * one_third;
																		   
			r1 = cbrtf(r) * (cosf((2.0F * pi + alpha) * one_third) + cosf((4.0F * pi - alpha) * one_third)) - a2 * one_third;
																		   
			r2 = cbrtf(r) * (cosf((4.0F * pi + alpha) * one_third) + cosf((2.0F * pi - alpha) * one_third)) - a2 * one_third;
		}
		else if (d > 1e-7F) // d > 0
		{
			const float off = sqrtf(d);

			const float u = cbrtf(-q * 0.5F + off);

			const float v = cbrtf(-q * 0.5F - off);

			r0 = u + v - a2 * one_third;
		}
		else // d == 0
		{
			const float u = cbrtf(-q * 0.5F);

			r0 = 2.0F * u - a2 * one_third;

			r1 = -u - a2 * one_third;
		}
	}
}
