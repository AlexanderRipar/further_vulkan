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

	static inline void cubic_poly_roots(float a3_, float a2_, float a1_, float a0_, float& r0, float& r1, float& r2) noexcept
	{
		constexpr double one_third = 1.0 / 3.0;

		const double a3_inv = 1.0F / a3_;

		const double a2 = a2_ * a3_inv;

		const double a1 = a1_ * a3_inv;

		const double a0 = a0_ * a3_inv;

		const double p = a1 - (a2 * a2) * one_third;

		const double q = a2 * a2 * a2 * (2.0 / 27.0) - a2 * a1 * one_third + a0;

		const double d = q * q * (1.0 / 4.0) + p * p * p * (1.0 / 27.0);

		r0 = r1 = r2 = INFINITY;

		if (d < -1e-7F) // d < 0
		{
			constexpr double pi = 3.14159265359;

			const double r = sqrt(-p * p * p * (1.0 / 27.0));

			const double alpha_raw = atan(sqrt(-d) / -q * 2.0);

			const double alpha = q > 0.0 ? 2.0 * pi - alpha_raw : alpha_raw;

			r0 = static_cast<float>(cbrt(r) * (cos((6.0 * pi - alpha) * one_third) + cos((           alpha) * one_third)) - a2 * one_third);
																							   
			r1 = static_cast<float>(cbrt(r) * (cos((2.0 * pi + alpha) * one_third) + cos((4.0 * pi - alpha) * one_third)) - a2 * one_third);
																							   
			r2 = static_cast<float>(cbrt(r) * (cos((4.0 * pi + alpha) * one_third) + cos((2.0 * pi - alpha) * one_third)) - a2 * one_third);
		}
		else if (d > 1e-7F) // d > 0
		{
			const double off = sqrt(d);

			const double u = cbrt(-q * 0.5 + off);

			const double v = cbrt(-q * 0.5 - off);

			r0 = static_cast<float>(u + v - a2 * one_third);
		}
		else // d == 0
		{
			const double u = cbrt(-q * 0.5);

			r0 = static_cast<float>(2.0 * u - a2 * one_third);

			r1 = static_cast<float>(-u - a2 * one_third);
		}
	}
}
