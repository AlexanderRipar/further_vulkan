#include "och_fmt.h"

#include "och_error_handling.h"

#include "och_helpers.h"



#include "vulkan_tutorial.h"

#include "compute_buffer_copy.h"

#include "compute_to_swapchain.h"

#include "sdf_font.h"



#include "binary_image.h"

#include "sdf_image.h"

#include "opentype_file.h"

#include <cstdio>
#include <Windows.h>

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

och::err_info write_glyph_info(const opentype_file& file, char32_t codept)
{
	glyph_data glyph = file.get_glyph(codept);

	och::print(
		"'{}':\n\n"
		"x_min: {}\n"
		"x_max: {}\n"
		"y_min: {}\n"
		"y_max: {}\n"
		"advance_width: {}\n"
		"left_side_bearing: {}\n"
		"contours : {}\n"
		"points : {}\n\n",
		codept,
		glyph.x_min(),
		glyph.x_max(),
		glyph.y_min(),
		glyph.y_max(),
		glyph.advance_width(),
		glyph.left_side_bearing(),
		glyph.contour_cnt(),
		glyph.point_cnt());

	char bmp_name_buf[256];

	och::sprint(bmp_name_buf, "textures/codept_{:4>~0X}_.bmp", static_cast<uint32_t>(codept));

	constexpr uint32_t bmp_sz_x = 256, bmp_sz_y = 256;

	bitmap_file glyph_bmp(bmp_name_buf, och::fio::open_truncate, bmp_sz_x, bmp_sz_y);

	if (!glyph_bmp)
		return MSG_ERROR("Could not open bmp for drawing font");

	glyph_bmp.point_op([](texel_b8g8r8) noexcept -> texel_b8g8r8 {return { 0, 0, 0 }; });

	for (uint32_t i = 0; i != glyph.contour_cnt(); ++i)
	{
		glyph_contour c = glyph.get_contour(i);

		och::print("\nCONTOUR {} ({} points):\n", i, c.point_cnt());

		for (uint32_t j = 0; j != c.point_cnt(); ++j)
		{
			glyph_point p = c[j];

			och::print("{:3>}:   ({},{})\n", j, p.x(), p.y());
		}
	}

	for (uint16_t i = 0; i != glyph.point_cnt(); ++i)
	{
		glyph_point p = glyph.get_point(i);

		int32_t px = static_cast<int32_t>(p.x() * bmp_sz_x);
		int32_t py = static_cast<int32_t>(p.y() * bmp_sz_y);

		if (px < 0)
			px = 0;
		
		if (py < 0)
			py = 0;

		glyph_bmp(px, py) = p.is_on_line() ? texel_b8g8r8(0x00, 0x7F, 0xFF) : texel_b8g8r8(0xD0, 0x7F, 0x00);
	}

	och::print("\n\n");

	return {};
}

och::err_info font_testing() noexcept
{
	opentype_file file("C:\\Windows\\Fonts\\calibri.ttf");

	if (!file)
		return MSG_ERROR("Could not open file");

	check(write_glyph_info(file, U'ǻ'));

	//write_glyph_info(file, U'ü');

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
