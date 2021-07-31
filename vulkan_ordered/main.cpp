#include "och_fmt.h"

#include "och_error_handling.h"

#include "och_helpers.h"



#include "vulkan_tutorial.h"

#include "compute_buffer_copy.h"

#include "compute_to_swapchain.h"

#include "sdf_font.h"



#include "binary_image.h"

#include "sdf_image.h"

#include "truetype.h"

enum class sample_type
{
	none,
	testing,
	vulkan_tutorial,
	compute_buffer_copy,
	compute_colour_to_swapchain,
	compute_simplex_to_swapchain,
	sdf_font,
};

och::err_info image_testing() noexcept
{
	binary_image bin_img;

	check(bin_img.load_bmp("textures/sdf_src.bmp", [](texel_b8g8r8 col) noexcept { return col.r > 40; }));

	sdf_image sdf_img;

	check(sdf_img.from_bim(bin_img));

	check(sdf_img.save_bmp("textures/sdf_dst.bmp", true));

	return {};
}

void write_glyph_info(const truetype_file& file, uint32_t codept, och::stringview font_name, bool& has_erred_before)
{
	glyph_data glyph = file.get_glyph(codept);

	if (!has_erred_before)
	{
		has_erred_before = true;

		och::print("\n\nFont: {}\n\n", font_name);
	}

	och::print(
		"0x{:4>~0X}: advance_width: {}; left_side_bearing: {}; contours : {}; points : {}\n",
		codept,
		glyph.metrics().advance_width(),
		glyph.metrics().left_side_bearing(),
		glyph.contour_cnt(),
		glyph.point_cnt());
}

och::err_info font_testing() noexcept
{
	truetype_file ttf("C:/Windows/Fonts/consola.ttf");

	if (!ttf)
		return MSG_ERROR("Could not open ttf file");

	glyph_data glyph = ttf.get_glyph(0x0460); // 0x01DF (ǟ)

	constexpr uint32_t sdf_size = 512;

	sdf_image sdf;

	check(sdf.from_glyph(glyph, sdf_size, sdf_size));

	sdf.save_bmp("textures/glyph_sdf.bmp", true
		//, [](float dst) noexcept -> texel_b8g8r8
		//{
		//	const float dst_1_0 = dst * 0.5F + 0.5F;
		//
		//	const float dst_mod = fmodf(dst_1_0, 0.01F);
		//
		//	uint8_t c = static_cast<uint8_t>((-0.1F / (dst_mod * 100.0F + 0.1F) + 1.0F) * 256.0F);
		//
		//	if (dst < 0.0F)
		//		return { c, c, 0 };
		//	else
		//		return { 0, c, c };
		//}
		
		, [](float dst) noexcept -> texel_b8g8r8
		{
			dst = dst * 0.5F + 0.5F;
		
			if (dst < 0.1666F)
			{
				const uint8_t v = static_cast<uint8_t>(((dst * 5.0F) + 0.1666F) * 255.0F);
		
				return { 0, 0, v };
			}
			if (dst < 0.3333F)
			{
				const uint8_t v = static_cast<uint8_t>((((dst - 0.1666F) * 5.0F) + 0.1666F) * 128.0F);
		
				return { 0, v, v };
			}
			if (dst < 0.5000F)
			{
				const uint8_t v = static_cast<uint8_t>((((dst - 0.3333F) * 5.0F) + 0.1666F) * 255.0F);
		
				return { 0, v, 0 };
			}
			if (dst < 0.6666F)
			{
				const uint8_t v = static_cast<uint8_t>((((dst - 0.5000F) * 5.0F) + 0.1666F) * 128.0F);
		
				return { v, v, 0 };
			}
			if (dst < 0.8333F)
			{
				const uint8_t v = static_cast<uint8_t>((((dst - 0.6666F) * 5.0F) + 0.1666F) * 255.0F);
		
				return { v, 0, 0 };
			}
		
			const uint8_t v = static_cast<uint8_t>((((dst - 0.8333F) * 5.0F) + 0.1666F) * 128.0F);
		
			return { v, 0, v };
		}
	);

	bitmap_file sdf_bmp("textures/glyph_sdf.bmp", och::fio::open_normal);

	if (!sdf_bmp)
		return MSG_ERROR("Could not open sdf-bitmap for drawing outline-points");


	for (uint32_t i = 0; i != glyph.contour_cnt(); ++i)
	{
		const uint32_t beg = glyph.contour_beg_index(i), end = glyph.contour_end_index(i);

		och::print("\n       Contour {}:\n", i);

		for (uint32_t j = beg; j != end; ++j)
		{
			const och::vec2 p = glyph[j];

			och::print("{:3>} ({:3>}):   ({}, {})\n", j - beg, j, p.x, p.y);

			const texel_b8g8r8 c = ((j - beg) & 1) ? col::b8g8r8::black : col::b8g8r8::white;

			sdf_bmp(static_cast<uint32_t>(p.x * sdf_size), static_cast<uint32_t>(p.y * sdf_size)) = c;
		}

		const och::vec2 p = glyph[beg];

		sdf_bmp(static_cast<uint32_t>(p.x * sdf_size), static_cast<uint32_t>(p.y * sdf_size)) = col::b8g8r8::orange;
	}

	return {};
}

och::err_info testing() noexcept
{
	check(font_testing());

	return {};
}

int main()
{
	constexpr sample_type to_run = sample_type::testing;

	och::err_info err{};

	switch (to_run)
	{
	case sample_type::none:
		break;

	case sample_type::testing:
		err = testing();
		break;

	case sample_type::vulkan_tutorial:
		err = run_vulkan_tutorial();
		break;

	case sample_type::compute_buffer_copy:
		err = run_compute_buffer_copy();
		break;

	case sample_type::compute_colour_to_swapchain:
		err = run_compute_to_swapchain(false);
		break;

	case sample_type::compute_simplex_to_swapchain:
		err = run_compute_to_swapchain(true);
		break;

	case sample_type::sdf_font:
		err = run_sdf_font();
		break;
	}

	if (err)
	{
		och::print("An Error occurred!\n");
	
		auto stack = och::get_stacktrace();
	
		for (auto& e : stack)
			och::print("Function {} on Line {}: \"{}\"\n\n", e.function, e.line_num, e.call);
	}
	else
		och::print("\nProcess terminated successfully\n");
}
