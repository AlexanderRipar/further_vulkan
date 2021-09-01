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

#include "sdf_glyph_atlas.h"

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

och::err_info font_testing() noexcept
{
	truetype_file ttf("C:/Windows/Fonts/consola.ttf");
	
	if (!ttf)
		return MSG_ERROR("Could not open ttf file");
	


	codept_range ranges[1]{ {0, 1024} }; // { {32, 95} };
	
	glyph_atlas atlas;
	
	constexpr float clamp = 0.015625F * 2.0F;
	
	check(atlas.create("C:/Windows/Fonts/consola.ttf", 128, 2, clamp, 1024, och::range(ranges)));
	
	check(atlas.save_bmp("textures/atlas.bmp", true));
	
	atlas.destroy();



	//glyph_data glyph = ttf.get_glyph_data_from_id(591); // 165 -> { 131, 333, 319 }
	//
	//glyph_data glyph = ttf.get_glyph_data_from_codepoint(U'A');
	//
	//sdf_image img;
	//
	//check(img.from_glyph(glyph, 64, 64, 0.75F));
	//
	//check(img.save_bmp("textures/glyph.bmp", true, sdf_image::colour_mapper::monochrome));



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
