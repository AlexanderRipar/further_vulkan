#pragma once

#include <cstdint>

namespace och
{
	template<typename T>
	T clamp(const T& val, const T& min, const T& max)
	{
		if (val < min)
			return min;

		if (val > max)
			return max;

		return val;
	}

	template<typename T>
	T min(const T& v1, const T& v2)
	{
		return v1 < v2 ? v1 : v2;
	}

	template<typename T>
	T max(const T& v1, const T& v2)
	{
		return v1 > v2 ? v1 : v2;
	}
}