#include "och_fmt.h"

#include "och_err.h"



#include "vulkan_tutorial.h"
#include "compute_buffer_copy.h"
#include "compute_to_swapchain.h"
#include "sdf_font.h"
#include "voxel_volume.h"



enum class sample_type
{
	none,
	vulkan_tutorial,
	compute_buffer_copy,
	compute_colour_to_swapchain,
	compute_simplex_to_swapchain,
	sdf_font,
	voxel_volume,
};

int main()
{
	constexpr sample_type to_run = sample_type::sdf_font;

	och::status err{};

	switch (to_run)
	{
	case sample_type::none:
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

	case sample_type::voxel_volume:
		err = run_voxel_volume();
		break;
	}

	if (err)
	{
		och::print("An Error occurred!\n\n0x{:X} (From \"{}\")\n{}\n\n{}\n\n", och::err::get_native_error_code(), och::err::get_error_type_name(), och::err::get_error_description(), och::err::get_error_message());
		
		for (uint32_t c = och::err::get_stack_depth(), i = 0; i != c; ++i)
		{
			const och::error_context& ctx = och::err::get_error_context(i);
			och::print("File: {}\nFunction: {}\nLine {}: \"{}\"\n\n", ctx.filename(), ctx.function(), ctx.line_number(), ctx.line_content());
		}
	}
	else
		och::print("\nProcess terminated successfully\n");
}
