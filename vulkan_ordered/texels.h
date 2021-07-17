#pragma once

struct texel_b8g8r8
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

namespace col
{
	namespace b8g8r8
	{
		constexpr texel_b8g8r8 black  = { 0x00, 0x00, 0x00 };
		constexpr texel_b8g8r8 white  = { 0xFF, 0xFF, 0xFF };
		constexpr texel_b8g8r8 blue   = { 0xFF, 0x00, 0x00 };
		constexpr texel_b8g8r8 green  = { 0x00, 0xFF, 0x00 };
		constexpr texel_b8g8r8 red    = { 0x00, 0x00, 0xFF };
		constexpr texel_b8g8r8 orange = { 0x00, 0x7F, 0xFF };
	}
}
