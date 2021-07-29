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
	och::file_search folder("C:/Windows/Fonts", och::fio::search_for_files, "?ttf");

	och::print("Starting search...\n\n");

	uint32_t file_cnt = 0;

	for (auto file_info : folder)
	{
		och::stringview ending = file_info.ending();

		++file_cnt;

		truetype_file file(file_info.absolute_name().raw_cbegin());

		if (!file)
		{
			och::print("\nError with file {}\n", file_info.absolute_name());

			continue;
		}

		och::print("{}\n", file_info.name());

		//bool has_erred_before = false;
		//
		//for (uint32_t i = 0; i != 0x10000; ++i)
		//	write_glyph_info(file, i, file_info.name(), has_erred_before);
	}

	och::print("\n{} Files found\n", file_cnt);

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
