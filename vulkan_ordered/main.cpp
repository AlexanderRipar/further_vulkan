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
	//consola.ttf
	#define FONT_NAME "ALGER"

	const char* ttf_filename = "C:/Windows/Fonts/" FONT_NAME ".ttf"; // "C:/Windows/Fonts/consola.ttf";
	const char* glfatl_filename = "C:/Users/alex_2/source/repos/vulkan_ordered/vulkan_ordered/textures/" FONT_NAME ".glfatl";

	{
		truetype_file ttf_file(ttf_filename);

		glyph_data glf = ttf_file.get_glyph_data_from_codepoint('y');

		sdf_image img;

		img.from_glyph(glf, 64, 64, 0.75F);

		img.save_bmp("C:/Users/alex_2/source/repos/vulkan_ordered/vulkan_ordered/textures/glyph_sdf.bmp", true, sdf_image::colour_mapper::nonlinear_distance);

		och::print("width: {}\nheight: {}\n", glf.metrics().x_size(), glf.metrics().y_size());
	}



	glyph_atlas atlas;

	if (atlas.load_glfatl(glfatl_filename))
	{
		och::print("Could not find existing glyph atlas \"{}\".\nCreating new file from \"{}\".\n", glfatl_filename, ttf_filename);

		codept_range ranges[1]{ {32, 95} };

		constexpr float clamp = 0.015625F * 2.0F;

		check(atlas.create(ttf_filename, 64, 2, clamp, 1024, och::range(ranges)));

		check(atlas.save_glfatl(glfatl_filename, true));

		check(atlas.save_bmp("textures/atlas_ttfdirect.bmp", true));
	}
	else
	{
		och::print("Existing glyph atlas found.\n");

		check(atlas.save_bmp("textures/atlas_readback.bmp", true));

		och::print("Loaded existing glyph atlas from {}\n", glfatl_filename);
	}

	constexpr uint32_t text_w = 2048;

	bitmap_file text_bmp("textures/text.bmp", och::fio::open::truncate, text_w, 256);

	image_view atlas_view{ atlas.view() };

	// uint32_t curr_x = 32;
	// 
	// const uint32_t line_y = 64;
	// 
	// uint8_t c = static_cast<uint8_t>(getchar());
	// 
	// while (c != '\n')
	// {
	// 	glyph_atlas::glyph_index idx = atlas(c);
	// 
	// 	const uint32_t x_lo = static_cast<uint32_t>(idx.position.x * atlas.width());
	// 
	// 	const uint32_t y_lo = static_cast<uint32_t>(idx.position.y * atlas.height());
	// 
	// 	const uint32_t x_sz = static_cast<uint32_t>(idx.size.x * atlas.width() + 2.0F);
	// 
	// 	const uint32_t y_sz = static_cast<uint32_t>(idx.size.y * atlas.height() + 2.0F);
	// 
	// 	const uint32_t brg_x = static_cast<uint32_t>(idx.bearing.x * atlas.width());
	// 
	// 	const uint32_t brg_y = static_cast<uint32_t>(idx.bearing.y * atlas.height());
	// 
	// 	och::print("U+{:4>~0X} ({}): brg_x: {} -> {}\n           brg_y: {} -> {}\n\n", static_cast<uint32_t>(c), static_cast<char>(c), idx.bearing.x, static_cast<int32_t>(brg_x), idx.bearing.y, static_cast<int32_t>(brg_y));
	// 
	// 	for (uint32_t y = 0; y != y_sz; ++y)
	// 		for (uint32_t x = 0; x != x_sz; ++x)
	// 			if (const uint8_t v = atlas_view(x_lo + x, y_lo + y))
	// 				text_bmp(curr_x + brg_x + x, line_y + brg_y + y) = texel_b8g8r8(v, v, v);
	// 
	// 	curr_x += static_cast<uint32_t>(idx.advance * atlas.width() + 0.99F);
	// 
	// 	c = static_cast<uint8_t>(getchar());
	// }

	atlas.destroy();

	return {};
}

och::err_info testing() noexcept
{
	check(font_testing());

	return {};
}

int main()
{
	constexpr sample_type to_run = sample_type::sdf_font;

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
