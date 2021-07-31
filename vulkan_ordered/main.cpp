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

	glyph_data glyph = ttf.get_glyph(0x01DF); // 0x01DF (ǟ)

	//constexpr uint32_t bmp_size = 256;
	//
	//bitmap_file bmp("textures\\glyph_points.bmp", och::fio::open_truncate, bmp_size, bmp_size);
	//
	//if (!bmp)
	//	return MSG_ERROR("Could not open bmp file");
	//
	//constexpr texel_b8g8r8 contour_colours[4][2]
	//{
	//	{{0x00, 0x00, 0xFF}, {0x00, 0x00, 0x5F}},
	//	{{0x00, 0xFF, 0x00}, {0x00, 0x5F, 0x00}},
	//	{{0xFF, 0x00, 0x00}, {0x5F, 0x00, 0x00}},
	//	{{0x00, 0xFF, 0xFF}, {0x00, 0x5F, 0x6F}},
	//};
	//
	//for (uint32_t i = 0; i != glyph.contour_cnt(); ++i)
	//{
	//	const uint32_t beg = glyph.contour_beg_index(i), end = glyph.contour_end_index(i);
	//
	//	och::print("\n       Contour {}:\n", i);
	//
	//	for (uint32_t j = beg; j != end; ++j)
	//	{
	//		const och::vec2 p = glyph[j];
	//
	//		och::print("{:3>} ({:3>}):   ({}, {})\n", j - beg, j, p.x, p.y);
	//
	//		bmp(static_cast<uint32_t>(p.x * bmp_size), static_cast<uint32_t>(p.y * bmp_size)) = contour_colours[i & 3][((j - beg) & 1)];
	//	}
	//}

	constexpr uint32_t sdf_size = 512;

	sdf_image sdf;

	check(sdf.from_glyph(glyph, sdf_size, sdf_size));

	

	sdf.save_bmp("textures/glyph_sdf.bmp", true, 
		[](float dst) noexcept -> texel_b8g8r8
		{
			dst = dst * 0.5F + 0.5F;

			//if (dst == 0.0F)
			//	return { 0xFF, 0xFF, 0xFF };

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
	}

	return {};
}

och::err_info testing() noexcept
{
	check(font_testing());

	float a = 3.0F, b = -10.0F, c = 1.0F, d = 4.0F;

	float r0, r1, r2;

	och::cubic_poly_roots(a, b, c, d, r0, r1, r2);

	och::print("{}x^3 + {}x^2 + {}x + {}\nr0: {}\nr1: {}\nr2: {}\n", a, b, c, d, r0, r1, r2);

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
