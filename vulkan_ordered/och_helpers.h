#pragma once

#include <cstdint>
#include <cmath>

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
		constexpr float TOO_SMALL = 1e-7F;

		constexpr float PI = 3.14159265359F;
		
		a2 /= a3;

		a1 /= a3;

		a0 /= a3;

		const float q = (3.0F * a1 - (a2 * a2)) / 9.0F;

		const float r = (-27.0F * a0 + a2 * (9.0F * a1 - 2.0F * a2 * a2)) / 54.0F;

		const float disc = q * q * q + r * r;

		const float term_1 = -a2 / 3.0F;

		if (disc > TOO_SMALL)
		{
			const float disc_sqrt = sqrtf(disc);

			float s = r + disc_sqrt;

			s = s < 0.0F ? -cbrtf(-s) : cbrtf(s);

			float t = r - disc_sqrt;

			t = t < 0.0F ? -cbrtf(-t) : cbrtf(t);

			r0 = term_1 + s + t;

			r1 = INFINITY;

			r2 = INFINITY;
		}
		else if (disc < -TOO_SMALL)
		{
			const float dummy = acosf(r / sqrtf(-q * q * q));

			const float r13 = 2.0F * sqrtf(-q);

			r0 = term_1 + r13 * cosf(dummy / 3.0F);

			r1 = term_1 + r13 * cosf((dummy + 2.0F * PI) / 3.0F);

			r2 = term_1 + r13 * cosf((dummy + 4.0F * PI) / 3.0F);
		}
		else
		{
			const float r13 = r < 0.0F ? -cbrtf(-r) : cbrtf(r);

			r0 = term_1 + 2.0F * r13;

			r1 = term_1 - r13;

			r2 = INFINITY;
		}
	}
}
